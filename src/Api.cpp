#include "Api.h"
#include "Config.h"
#include "JumpNav.h"
#include "Locomotion.h"
#include "Mantle.h"

#include "../include/FluidMotionNPC_API.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>

namespace Api
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		struct Entry
		{
			Clock::time_point claimUntil{};
			Clock::time_point turnUntil{};
			float             turnDeg = 0.0f;
		};

		std::mutex                                 g_mutex;
		std::unordered_map<RE::FormID, Entry>      g_actors;

		constexpr int                              kMaxCallbacks = 8;
		std::atomic<FMNPC_PostGraphFn>             g_post[kMaxCallbacks]{};
		std::atomic<int>                           g_postCount{ 0 };
		std::atomic<FMNPC_FrameFn>                 g_frame[kMaxCallbacks]{};
		std::atomic<int>                           g_frameCount{ 0 };

		void Prune()
		{
			const auto now = Clock::now();
			for (auto it = g_actors.begin(); it != g_actors.end();) {
				if (it->second.claimUntil < now && it->second.turnUntil < now) {
					it = g_actors.erase(it);
				} else {
					++it;
				}
			}
		}
	}

	bool Claimed(RE::FormID a_id)
	{
		std::lock_guard lk(g_mutex);
		auto it = g_actors.find(a_id);
		return it != g_actors.end() && Clock::now() < it->second.claimUntil;
	}

	float MaxTurn(RE::FormID a_id)
	{
		std::lock_guard lk(g_mutex);
		auto it = g_actors.find(a_id);
		return it != g_actors.end() && Clock::now() < it->second.turnUntil ? it->second.turnDeg : 0.0f;
	}

	namespace
	{
		// Ride Along (Nexus 194579): seats followers on the player's horse and pins them there every frame. While an
		// actor is in its passenger faction (boarding, riding, driving, and ~2 s after getting off) it is not walking,
		// so FluidMotionNPC must not climb, jump or route it (that showed as "climbing gestures" on the passenger).
		RE::TESFaction* RideAlongPassengerFaction()
		{
			static std::atomic<int>          state{ 0 };  // 0 = not looked up yet, 1 = done
			static std::atomic<RE::TESFaction*> faction{ nullptr };
			if (state.load(std::memory_order_acquire) == 0) {
				if (auto* dh = RE::TESDataHandler::GetSingleton(); dh && dh->LookupModByName("RideAlong.esp")) {
					faction.store(dh->LookupForm<RE::TESFaction>(0x802, "RideAlong.esp"), std::memory_order_release);
					logger::info("Ride Along detected: its passengers are left alone (faction {})", faction.load() ? "found" : "NOT found");
				}
				state.store(1, std::memory_order_release);
			}
			return faction.load(std::memory_order_acquire);
		}
	}

	namespace
	{
		bool FollowerParkourLoaded()
		{
			static const bool loaded = [] {
				const bool l = ::GetModuleHandleW(L"FollowerParkour.dll") != nullptr;
				logger::info("Follower Parkour {}: {}", l ? "detected" : "not loaded",
					l ? "human vaults/climbs are offered to it first (bParkourHandoff)" : "FluidMotionNPC does all vaults/climbs");
				return l;
			}();
			return loaded;
		}

		struct Handoff
		{
			Clock::time_point t0{};
			RE::NiPoint3      spot{};
			bool              pending = false;
			std::vector<std::pair<RE::NiPoint3, Clock::time_point>> declined;  // spots where it did not trigger
		};
		std::mutex                                g_handoffMutex;
		std::unordered_map<RE::FormID, Handoff>   g_handoff;
	}

	bool ParkourHandoff(RE::Actor* a_actor, const RE::NiPoint3& a_spot, float a_maxDist)
	{
		const auto& cfg = Config::Get().jump;
		if (!a_actor || !cfg.parkourHandoff || cfg.parkourHandoffSec <= 0.0f || !FollowerParkourLoaded()) {
			return false;
		}
		bool ongoing = false;
		if (!a_actor->GetGraphVariableBool("SkyParkourOngoing", ongoing)) {
			return false;  // no SkyParkour moves on this actor (creatures, unpatched skeletons)
		}
		if (ongoing) {
			return true;  // Follower Parkour is doing it
		}
		// Follower Parkour only moves an NPC that is stuck against the obstacle: its AI pushing, the NPC barely moving,
		// the ledge within ~30u. Not there yet (or moving freely): nothing to wait for, FluidMotionNPC goes ahead.
		RE::NiPoint3 vel;
		a_actor->GetLinearVelocity(vel);
		const RE::NiPoint3 pos = a_actor->GetPosition();
		const bool         pressed = std::hypot(a_spot.x - pos.x, a_spot.y - pos.y) <= a_maxDist && std::hypot(vel.x, vel.y) < 40.0f;
		const auto         now = Clock::now();
		std::lock_guard    lk(g_handoffMutex);
		auto&              h = g_handoff[a_actor->GetFormID()];
		if (!pressed) {
			h.pending = false;
			return false;
		}
		std::erase_if(h.declined, [&](const auto& d) { return now - d.second > std::chrono::seconds(30); });
		for (const auto& d : h.declined) {
			if (d.first.GetDistance(a_spot) < 150.0f) {
				return false;  // it did not take this obstacle a moment ago: no second wait
			}
		}
		if (!h.pending || h.spot.GetDistance(a_spot) > 150.0f || now - h.t0 > std::chrono::seconds(10)) {
			h.pending = true;
			h.t0 = now;
			h.spot = a_spot;
			return true;
		}
		if (now - h.t0 < std::chrono::duration<float>(cfg.parkourHandoffSec)) {
			return true;
		}
		h.pending = false;
		h.declined.emplace_back(a_spot, now);
		if (cfg.log) {
			auto* n = a_actor->GetName();
			logger::info("PARKOURHANDOFF {:08X} '{}': Follower Parkour did not move it within {:.1f}s - FluidMotionNPC takes over", a_actor->GetFormID(),
				n ? n : "", cfg.parkourHandoffSec);
		}
		return false;
	}

	bool ExternalBusy(RE::Actor* a_actor)
	{
		if (!a_actor || !a_actor->Is3DLoaded()) {
			return false;
		}
		if (auto* f = RideAlongPassengerFaction(); f && a_actor->IsInFaction(f)) {
			return true;
		}
		// Set by SkyParkour's behaviour graph while a parkour move / landing roll / slide plays (Follower Parkour uses
		// the same graph on NPCs). Absent variables read as false, so this costs nothing without those mods.
		bool parkour = false, slide = false;
		a_actor->GetGraphVariableBool("SkyParkourOngoing", parkour);
		a_actor->GetGraphVariableBool("SkyParkourSliding", slide);
		return parkour || slide;
	}

	void RunPostGraph(RE::Actor* a_actor, int a_src)
	{
		if (!a_actor) {
			return;
		}
		const int n = g_postCount.load(std::memory_order_acquire);
		for (int i = 0; i < n; ++i) {
			if (auto fn = g_post[i].load(std::memory_order_acquire)) {
				fn(a_actor, a_src);
			}
		}
	}

	void RunFrame(float a_dt)
	{
		static int pruneTick = 0;
		if (++pruneTick >= 120) {
			pruneTick = 0;
			std::lock_guard lk(g_mutex);
			Prune();
		}
		const int n = g_frameCount.load(std::memory_order_acquire);
		for (int i = 0; i < n; ++i) {
			if (auto fn = g_frame[i].load(std::memory_order_acquire)) {
				fn(a_dt);
			}
		}
	}

	void Reset()
	{
		std::lock_guard lk(g_mutex);
		g_actors.clear();
	}

	template <class Fn>
	bool Register(std::atomic<Fn>* a_list, std::atomic<int>& a_count, Fn a_fn)
	{
		if (!a_fn) {
			return false;
		}
		static std::mutex m;
		std::lock_guard   lk(m);
		const int         n = a_count.load();
		for (int i = 0; i < n; ++i) {
			if (a_list[i].load() == a_fn) {
				return true;
			}
		}
		if (n >= kMaxCallbacks) {
			return false;
		}
		a_list[n].store(a_fn, std::memory_order_release);
		a_count.store(n + 1, std::memory_order_release);
		return true;
	}
}

extern "C"
{
	__declspec(dllexport) int FMNPC_GetAPIVersion()
	{
		return FMNPC::kVersion;
	}

	__declspec(dllexport) bool FMNPC_IsEnabled()
	{
		return Config::Get().enabled;
	}

	__declspec(dllexport) bool FMNPC_IsTraversing(std::uint32_t a_formID)
	{
		return JumpNav::InFlight(a_formID) || JumpNav::Routing(a_formID) || Mantle::Active(a_formID);
	}

	__declspec(dllexport) void FMNPC_Claim(std::uint32_t a_formID, float a_seconds)
	{
		std::lock_guard lk(Api::g_mutex);
		auto&           e = Api::g_actors[a_formID];
		const auto      until = Api::Clock::now() + std::chrono::duration_cast<Api::Clock::duration>(std::chrono::duration<float>((std::max)(a_seconds, 0.0f)));
		e.claimUntil = a_seconds > 0.0f ? until : Api::Clock::time_point{};
	}

	__declspec(dllexport) bool FMNPC_OnNavmesh(RE::Actor* a_actor, RE::Actor* a_target)
	{
		return JumpNav::OnNavmesh(a_actor, a_target);
	}

	__declspec(dllexport) void FMNPC_NoteDriven(std::uint32_t a_formID, float a_speed)
	{
		Locomotion::NoteDriven(a_formID, a_speed);
	}

	__declspec(dllexport) void FMNPC_SetMaxTurnRate(std::uint32_t a_formID, float a_degPerSec, float a_seconds)
	{
		std::lock_guard lk(Api::g_mutex);
		auto&           e = Api::g_actors[a_formID];
		e.turnDeg = a_degPerSec;
		e.turnUntil = a_seconds > 0.0f && a_degPerSec > 0.0f ?
		                  Api::Clock::now() + std::chrono::duration_cast<Api::Clock::duration>(std::chrono::duration<float>(a_seconds)) :
		                  Api::Clock::time_point{};
	}

	__declspec(dllexport) bool FMNPC_RegisterPostGraph(FMNPC_PostGraphFn a_fn)
	{
		const bool ok = Api::Register(Api::g_post, Api::g_postCount, a_fn);
		logger::info("API: post-graph callback {}", ok ? "registered" : "rejected");
		return ok;
	}

	__declspec(dllexport) bool FMNPC_RegisterFrame(FMNPC_FrameFn a_fn)
	{
		const bool ok = Api::Register(Api::g_frame, Api::g_frameCount, a_fn);
		logger::info("API: frame callback {}", ok ? "registered" : "rejected");
		return ok;
	}
}
