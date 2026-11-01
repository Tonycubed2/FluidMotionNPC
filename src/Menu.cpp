// v40: optional SKSE Menu Framework page ("FluidMotionNPC" in the Mod Control Panel).
// The menu edits a copy of the settings on its render thread. The main thread writes the changed keys to
// FluidMotionNPC.ini and reloads the config the same way loading a save does, so the INI stays the one source.
#include "Menu.h"
#include "Config.h"
#include "Locomotion.h"
#include "Tuning.h"

#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#	define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#include <Windows.h>
#include "../include/smf/SKSEMenuFramework.h"

namespace Menu
{
	namespace
	{
		constexpr auto kIniPath = L"Data/SKSE/Plugins/FluidMotionNPC.ini";

		using Ptr = void* (*)(Config::Data&);

		struct Item
		{
			const char* header;  // group title shown above this item (nullptr = same group)
			const char* section;
			const char* key;
			const char* label;
			const char* tip;
			bool        isFloat;
			Ptr         ptr;
			float       lo = 0.0f;
			float       hi = 1.0f;
			const char* fmt = "%.0f";
		};

		using D = Config::Data;

		const std::vector<Item>& Items()
		{
			static const std::vector<Item> items = {
				{ "General", "General", "bEnabled", "FluidMotionNPC on", "Master switch. Off = NPCs move exactly like vanilla.", false,
					[](D& d) -> void* { return &d.enabled; } },

				{ "Jumps and routes", "Jump", "bEnabled", "Jumps, routes and vaults",
					"NPCs jump gaps and ledges, vault fences and route over terrain the navmesh can't reach.", false, [](D& d) -> void* { return &d.jump.enabled; } },
				{ nullptr, "Jump", "bShortcut", "Shortcut jumps", "NPCs in a hurry jump when it cuts a long detour off their path.", false,
					[](D& d) -> void* { return &d.jump.shortcut; } },
				{ nullptr, "Jump", "bRoute", "Routes on foot", "Route over steep ground and shallow water when the navmesh can't reach the target.", false,
					[](D& d) -> void* { return &d.jump.route; } },
				{ nullptr, "Jump", "bVault", "Vaults", "Vault low obstacles (tables, benches, fences) instead of climbing onto them.", false,
					[](D& d) -> void* { return &d.jump.vault; } },
				{ nullptr, "Jump", "bTrail", "Follow your trail", "An NPC that can't find its own way to you follows the path you took.", false,
					[](D& d) -> void* { return &d.jump.trail; } },
				{ nullptr, "Jump", "bDeepScan", "Deep terrain scan", "Finer search for a way up or down rough terrain when routes keep failing (more CPU).", false,
					[](D& d) -> void* { return &d.jump.deepScan; } },

				{ "Combat", "Jump", "bCombatJumps", "Jumps for NPCs in combat", "Off = NPCs in combat only walk, route and climb.", false,
					[](D& d) -> void* { return &d.jump.combatJumps; } },
				{ nullptr, "Jump", "bCombatVault", "Combat vaults", "Melee enemies vault a low obstacle in the straight line to you.", false,
					[](D& d) -> void* { return &d.jump.combatVault; } },
				{ nullptr, "Jump", "fAttackDelayWhilePathing", "Attack delay after a route (s)",
					"Pause before an NPC still following its path may attack (vanilla 1.5). -1 = leave the game's value.", true,
					[](D& d) -> void* { return &d.jump.attackDelayWhilePathing; }, -1.0f, 3.0f, "%.2f" },

				{ "Followers", "Jump", "bFollow", "Help stuck followers", "A follower stuck below or above you jumps or climbs to reach you.", false,
					[](D& d) -> void* { return &d.jump.follow; } },
				{ nullptr, "Jump", "bFollowerJumps", "Jumps for followers", "Off = followers only walk, route and climb.", false,
					[](D& d) -> void* { return &d.jump.followerJumps; } },
				{ nullptr, "Jump", "bFollowVault", "Follower vaults", "Followers vault an obstacle only when walking around is a real detour.", false,
					[](D& d) -> void* { return &d.jump.followVault; } },
				{ nullptr, "Jump", "bFollowerPace", "Followers keep their own pace", "Followers on a route move at the speed their own AI asks for (no speed boost).",
					false, [](D& d) -> void* { return &d.jump.followerPace; } },
				{ nullptr, "Jump", "bCalmFollowers", "Calm places",
					"Followers out of combat in houses, inns, shops, temples, castles and your home are left to the game.", false,
					[](D& d) -> void* { return &d.jump.calmFollowers; } },
				{ nullptr, "Jump", "bCalmAllInteriors", "Every interior is calm", "Out of combat, every interior counts as calm (dungeons too).", false,
					[](D& d) -> void* { return &d.jump.calmAllInteriors; } },
				{ nullptr, "Jump", "fFollowRouteMin", "Follower help distance",
					"A follower is only routed or takes shortcut jumps when at least this far from you (units).", true,
					[](D& d) -> void* { return &d.jump.followRouteMin; }, 150.0f, 2000.0f, "%.0f" },

				{ "Animations", "Anim", "bEnabled", "Traversal animations", "Climb, ledge-grab and jump animations for NPCs.", false,
					[](D& d) -> void* { return &d.anim.enabled; } },
				{ nullptr, "Anim", "bClimb", "Climbing", "NPCs climb obstacles about knee to shoulder height.", false, [](D& d) -> void* { return &d.anim.climb; } },
				{ nullptr, "Anim", "bHang", "Ledge grabs", "NPCs grab a ledge and pull up after a jump that falls short.", false,
					[](D& d) -> void* { return &d.anim.hang; } },

				{ "Logging", "Jump", "bLog", "Log jumps and routes", "Writes details to FluidMotionNPC.log (for bug reports).", false,
					[](D& d) -> void* { return &d.jump.log; } },
				{ nullptr, "Anim", "bLog", "Log animations", "Writes animation details to FluidMotionNPC.log.", false, [](D& d) -> void* { return &d.anim.log; } },
			};
			return items;
		}

		std::mutex        g_lock;
		D                 g_ui;  // what the page shows and edits
		std::vector<char> g_dirty;
		bool              g_anyDirty = false;
		bool              g_reload = false;
		std::chrono::steady_clock::time_point g_lastEdit{};
		std::string       g_status;

		void __stdcall Render()
		{
			std::scoped_lock l(g_lock);
			const auto&      items = Items();
			if (g_dirty.size() != items.size()) {
				g_dirty.assign(items.size(), 0);
			}
			ImGuiMCP::TextWrapped("Changes are saved to FluidMotionNPC.ini and take effect right away (hover a setting for details).");
			for (std::size_t i = 0; i < items.size(); ++i) {
				const auto& it = items[i];
				if (it.header) {
					ImGuiMCP::SeparatorText(it.header);
				}
				void* p = it.ptr(g_ui);
				// Same label twice in ImGui (bLog in two sections) needs a unique id: "label##section.key".
				const std::string id = std::format("{}##{}.{}", it.label, it.section, it.key);
				const bool        changed = it.isFloat ? ImGuiMCP::SliderFloat(id.c_str(), static_cast<float*>(p), it.lo, it.hi, it.fmt) :
				                                         ImGuiMCP::Checkbox(id.c_str(), static_cast<bool*>(p));
				if (it.tip && ImGuiMCP::IsItemHovered()) {
					ImGuiMCP::SetTooltip("%s", it.tip);
				}
				if (changed) {
					g_dirty[i] = 1;
					g_anyDirty = true;
					g_lastEdit = std::chrono::steady_clock::now();
					g_status.clear();
				}
			}
			ImGuiMCP::Separator();
			if (ImGuiMCP::Button("Reload FluidMotionNPC.ini")) {
				std::fill(g_dirty.begin(), g_dirty.end(), 0);
				g_anyDirty = false;
				g_reload = true;
				g_status = "Reloading...";
			}
			if (ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", "Re-read the INI (after editing it by hand). Settings not shown here are only in the INI.");
			}
			if (!g_status.empty()) {
				ImGuiMCP::TextDisabled("%s", g_status.c_str());
			}
		}

		// Main thread: re-read the INI into the running config (as on a save load).
		void ApplyConfig()
		{
			Config::Load();
			Tuning::Reload();
			Locomotion::Reload();
		}

		bool SaveDirty(const D& a_ui, const std::vector<char>& a_dirty)
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey(false);
			if (ini.LoadFile(kIniPath) < 0) {
				logger::warn("Menu: FluidMotionNPC.ini not found - creating it");
			}
			D           copy = a_ui;
			const auto& items = Items();
			for (std::size_t i = 0; i < items.size() && i < a_dirty.size(); ++i) {
				if (!a_dirty[i]) {
					continue;
				}
				const auto& it = items[i];
				void*       p = it.ptr(copy);
				if (it.isFloat) {
					const float v = *static_cast<float*>(p);
					ini.SetValue(it.section, it.key, std::format("{:.2f}", v).c_str());
					logger::info("Menu: {}/{} = {:.2f}", it.section, it.key, v);
				} else {
					const bool v = *static_cast<bool*>(p);
					ini.SetValue(it.section, it.key, v ? "true" : "false");
					logger::info("Menu: {}/{} = {}", it.section, it.key, v);
				}
			}
			return ini.SaveFile(kIniPath) >= 0;
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("Menu: SKSE Menu Framework not installed - no in-game settings page (the INI works as always)");
			return;
		}
		{
			std::scoped_lock l(g_lock);
			g_ui = Config::Get();
		}
		SKSEMenuFramework::SetSection("FluidMotionNPC");
		SKSEMenuFramework::AddSectionItem("Settings", Render);
		logger::info("Menu: settings page added to SKSE Menu Framework");
	}

	void OnGameLoaded()
	{
		std::scoped_lock l(g_lock);
		if (!g_anyDirty) {
			g_ui = Config::Get();
		}
	}

	void RunFrame()
	{
		D                 ui;
		std::vector<char> dirty;
		bool              reload = false;
		{
			std::scoped_lock l(g_lock);
			if (g_reload) {
				g_reload = false;
				reload = true;
			} else if (g_anyDirty && std::chrono::steady_clock::now() - g_lastEdit > 400ms) {
				// a short wait so dragging a slider doesn't rewrite the INI every frame
				ui = g_ui;
				dirty = g_dirty;
				std::fill(g_dirty.begin(), g_dirty.end(), 0);
				g_anyDirty = false;
			} else {
				return;
			}
		}
		std::string status;
		if (reload) {
			ApplyConfig();
			status = "INI reloaded.";
		} else if (SaveDirty(ui, dirty)) {
			ApplyConfig();
			status = "Saved to FluidMotionNPC.ini.";
		} else {
			status = "Could not write FluidMotionNPC.ini (read-only?) - not saved.";
			logger::warn("Menu: {}", status);
		}
		std::scoped_lock l(g_lock);
		if (!g_anyDirty) {
			g_ui = Config::Get();
		}
		g_status = status;
	}
}
