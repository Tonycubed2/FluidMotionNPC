#pragma once

namespace JumpNav
{
	enum class Trigger : std::uint8_t
	{
		kNone = 0,
		kShortcut = 1,
		kPursuit = 2,
		kFollow = 3,
		kRoute = 4,
	};

	struct Stats
	{
		std::uint32_t evals = 0;
		std::uint32_t launched[5]{};
		std::uint32_t routes = 0, routesFailed = 0, routesDone = 0;
		std::uint32_t landed = 0;
		std::uint32_t aborted = 0;
		std::uint32_t rejRange = 0, rejProfile = 0, rejRise = 0, rejSweep = 0, rejLanding = 0, rejOccupied = 0;
	};

	// v29: water creatures (slaughterfish, mod fish, dreugh...) are never moved, jumped, routed or climbed. By race
	// movement flags (swims but doesn't walk) and by race editor ID, so mod-added fish with odd names are covered too.
	inline bool IsAquatic(RE::Actor* a_actor)
	{
		auto* race = a_actor ? a_actor->GetRace() : nullptr;
		if (!race) {
			return false;
		}
		static std::mutex                             lock;  // may be called from animation job threads
		static std::unordered_map<RE::TESRace*, bool> cache;
		std::scoped_lock                              guard(lock);
		if (auto it = cache.find(race); it != cache.end()) {
			return it->second;
		}
		using F = RE::RACE_DATA::Flag;
		bool aq = race->data.flags.any(F::kSwims) && race->data.flags.none(F::kWalks);
		std::string edid = race->formEditorID.c_str();
		std::ranges::transform(edid, edid.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		for (const char* w : { "fish", "slaughter", "dreugh", "salmon", "shark", "trout", "squid", "octopus", "mudcrab" }) {
			aq = aq || edid.find(w) != std::string::npos;
		}
		return cache[race] = aq;
	}

	void  OnFrame(float a_delta);
	void  Reset();
	bool  OnNavmesh(RE::Actor* a_actor, RE::Actor* a_target);
	bool  Routing(RE::FormID a_id);
	bool  InFlight(RE::FormID a_id);
	int   TraceState(RE::FormID a_id);
	bool  InHurry(RE::Actor* a_actor);
	bool  IsFollowingPlayer(RE::Actor* a_actor);
	Stats GetStats();

	struct DebugArc
	{
		std::vector<RE::NiPoint3> points;
		float                     age = 0;
		bool                      launched = false;
		std::string               label;
	};
	void GetDebugArcs(std::vector<DebugArc>& a_out);

	inline std::atomic_bool enabled{ false };
}
