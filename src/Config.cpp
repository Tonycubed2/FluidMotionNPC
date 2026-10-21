#include "Config.h"

namespace Config
{
	namespace
	{
		Data g_data;

		std::vector<std::string> SplitList(std::string_view a_list)
		{
			std::vector<std::string> out;
			std::string              cur;
			for (char c : a_list) {
				if (c == ',' || c == ';') {
					if (!cur.empty()) {
						out.push_back(std::move(cur));
					}
					cur.clear();
				} else if (!std::isspace(static_cast<unsigned char>(c))) {
					cur.push_back(c);
				}
			}
			if (!cur.empty()) {
				out.push_back(std::move(cur));
			}
			return out;
		}
	}

	const Data& Get() { return g_data; }

	Data& Mutable() { return g_data; }

	void Load()
	{
		constexpr auto path = L"Data/SKSE/Plugins/FluidMotionNPC.ini";

		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(path) < 0) {
			logger::warn("FluidMotionNPC.ini not found, using defaults");
		}

		Data d;
		d.enabled = ini.GetBoolValue("General", "bEnabled", d.enabled);

		auto& j = d.jump;
		auto  getf = [&](const char* a_key, float& a_v) {
			a_v = static_cast<float>(ini.GetDoubleValue("Jump", a_key, a_v));
		};
		j.enabled = ini.GetBoolValue("Jump", "bEnabled", j.enabled);
		j.shortcut = ini.GetBoolValue("Jump", "bShortcut", j.shortcut);
		j.pursuit = ini.GetBoolValue("Jump", "bPursuit", j.pursuit);
		j.follow = ini.GetBoolValue("Jump", "bFollow", j.follow);
		j.followerJumps = ini.GetBoolValue("Jump", "bFollowerJumps", j.followerJumps);
		j.calmFollowers = ini.GetBoolValue("Jump", "bCalmFollowers", j.calmFollowers);
		j.calmAllInteriors = ini.GetBoolValue("Jump", "bCalmAllInteriors", j.calmAllInteriors);
		j.calmLocations = SplitList(ini.GetValue("Jump", "sCalmLocations",
			"LocTypeHouse,LocTypePlayerHouse,LocTypeInn,LocTypeStore,LocTypeTemple,LocTypeCastle,LocTypeGuild,LocTypeShip,LocTypeJail"));
		j.combatJumps = ini.GetBoolValue("Jump", "bCombatJumps", j.combatJumps);
		j.route = ini.GetBoolValue("Jump", "bRoute", j.route);
		getf("fRouteMaxDist", j.routeMaxDist);
		getf("fRouteStuckSec", j.routeStuckSec);
		getf("fRouteStep", j.routeStep);
		j.routeMaxNodes = static_cast<int>(ini.GetLongValue("Jump", "iRouteMaxNodes", j.routeMaxNodes));
		getf("fGlideSpeed", j.glideSpeed);
		getf("fDriveTurnScale", j.driveTurnScale);
		j.raceMotion = ini.GetBoolValue("Jump", "bRaceMotion", j.raceMotion);
		j.routeStep = std::clamp(j.routeStep, 40.0f, 200.0f);
		j.routeMaxNodesNear = static_cast<int>(ini.GetLongValue("Jump", "iRouteMaxNodesNear", j.routeMaxNodesNear));
		j.routeNearCount = static_cast<int>(ini.GetLongValue("Jump", "iRouteNearCount", j.routeNearCount));
		j.jumpOnNavmesh = ini.GetBoolValue("Jump", "bJumpOnNavmesh", j.jumpOnNavmesh);
		j.routeMaxNodes = std::clamp(j.routeMaxNodes, 8, 6000);
		j.routeMaxNodesNear = std::clamp(j.routeMaxNodesNear, j.routeMaxNodes, 8000);
		getf("fGravity", j.gravity);
		getf("fUpPerHeight", j.upPerHeight);
		getf("fUpMin", j.upMin);
		getf("fUpMax", j.upMax);
		getf("fDownMax", j.downMax);
		getf("fDistPerHeight", j.distPerHeight);
		getf("fDistMin", j.distMin);
		getf("fDistMax", j.distMax);
		getf("fMaxActorHeight", j.maxActorHeight);
		getf("fMinSaving", j.minSaving);
		getf("fMinRatio", j.minRatio);
		getf("fMinSpeed", j.minSpeed);
		getf("fMaxSpeed", j.maxSpeed);
		getf("fClearance", j.clearance);
		getf("fCooldown", j.cooldown);
		getf("fTurnRateDeg", j.turnRateDeg);
		getf("fFollowRouteMin", j.followRouteMin);
		getf("fGlideAccel", j.glideAccel);
		getf("fDriveRise", j.driveRise);
		getf("fDriveFall", j.driveFall);
		getf("fPursuitStuckSec", j.pursuitStuckSec);
		getf("fFollowStuckSec", j.followStuckSec);
		getf("fFollowMinSaving", j.followMinSaving);
		getf("fFollowMinRatio", j.followMinRatio);
		j.followVault = ini.GetBoolValue("Jump", "bFollowVault", j.followVault);
		j.vault = ini.GetBoolValue("Jump", "bVault", j.vault);
		j.combatVault = ini.GetBoolValue("Jump", "bCombatVault", j.combatVault);
		j.parkourHandoff = ini.GetBoolValue("Jump", "bParkourHandoff", j.parkourHandoff);
		getf("fParkourHandoffSec", j.parkourHandoffSec);
		j.parkourHandoffSec = std::clamp(j.parkourHandoffSec, 0.0f, 5.0f);
		j.trail = ini.GetBoolValue("Jump", "bTrail", j.trail);
		getf("fTrailMaxAge", j.trailMaxAge);
		getf("fCombatVaultMinSaving", j.combatVaultMinSaving);
		getf("fCombatVaultMinRatio", j.combatVaultMinRatio);
		getf("fAttackDelayWhilePathing", j.attackDelayWhilePathing);
		getf("fVaultMaxFrac", j.vaultMaxFrac);
		getf("fVaultMaxDz", j.vaultMaxDz);
		getf("fFollowVaultMinSaving", j.followVaultMinSaving);
		getf("fFollowVaultMinRatio", j.followVaultMinRatio);
		getf("fFollowVaultMaxDz", j.followVaultMaxDz);
		getf("fPursuitMinRise", j.pursuitMinRise);
		getf("fPursuitMinDrop", j.pursuitMinDrop);
		getf("fMeleeRise", j.meleeRise);
		j.driveMode = static_cast<int>(ini.GetLongValue("Jump", "iDriveMode", j.driveMode));
		j.excludeRaces = SplitList(ini.GetValue("Jump", "sExcludeRaces",
			"Dragon,Alduin,Mammoth,Horker,Mudcrab,Slaughterfish,Centurion,Sphere,Wisp,Netch,Fish,Torchbug,Moth,Butterfly,Dragonfly,Hawk,Crow,Chicken,Cow,Goat"));
		j.noRouteRaces = SplitList(ini.GetValue("Jump", "sNoRouteRaces", "Dragon,Alduin,Slaughterfish,Fish,Wisp,Netch,Torchbug,Moth,Butterfly,Dragonfly,Hawk,Crow"));
		j.climbRaces = SplitList(ini.GetValue("Jump", "sClimbRaces", "Draugr,Falmer,Hagraven,Forsworn,Skeleton"));
		j.steepRaces = SplitList(ini.GetValue("Jump", "sSteepRaces", "Bear,Spider,Chaurus,Sabre,Goat,Troll"));
		getf("fSteepRise", j.steepRise);
		getf("fSteepCost", j.steepCost);
		getf("fWadeDepth", j.wadeDepth);
		getf("fWadeCost", j.wadeCost);
		getf("fVertMismatch", j.vertMismatch);
		getf("fGiantHeight", j.giantHeight);
		getf("fGiantDistMax", j.giantDistMax);
		getf("fCreatureJumpScale", j.creatureJumpScale);
		j.deepScan = ini.GetBoolValue("Jump", "bDeepScan", j.deepScan);
		j.deepScanAfter = std::clamp(static_cast<int>(ini.GetLongValue("Jump", "iDeepScanAfter", j.deepScanAfter)), 1, 10);
		getf("fDeepScanCell", j.deepScanCell);
		j.deepScanCell = std::clamp(j.deepScanCell, 16.0f, 64.0f);
		j.deepScanMaxCells = std::clamp(static_cast<int>(ini.GetLongValue("Jump", "iDeepScanMaxCells", j.deepScanMaxCells)), 32, 160);
		j.deepScanRays = std::clamp(static_cast<int>(ini.GetLongValue("Jump", "iDeepScanRays", j.deepScanRays)), 50, 2000);
		getf("fDeepScanPause", j.deepScanPause);
		getf("fDeepScanShareSec", j.deepScanShareSec);
		j.deepScanShareSec = std::clamp(j.deepScanShareSec, 0.0f, 600.0f);
		j.deepScanRetries = std::clamp(static_cast<int>(ini.GetLongValue("Jump", "iDeepScanRetries", j.deepScanRetries)), 1, 12);
		getf("fGiantUpPerHeight", j.giantUpPerHeight);
		getf("fGiantDistPerHeight", j.giantDistPerHeight);
		j.npcStartEvent = ini.GetValue("Jump", "sNpcStartEvent", j.npcStartEvent.c_str());
		j.npcLandEvent = ini.GetValue("Jump", "sNpcLandEvent", j.npcLandEvent.c_str());
		j.creatureStartEvent = ini.GetValue("Jump", "sCreatureStartEvent", "");
		j.creatureLandEvent = ini.GetValue("Jump", "sCreatureLandEvent", "");
		j.log = ini.GetBoolValue("Jump", "bLog", j.log);
		j.gravity = std::clamp(j.gravity, 300.0f, 3000.0f);
		j.minSpeed = std::clamp(j.minSpeed, 50.0f, 1500.0f);
		j.maxSpeed = (std::max)(j.maxSpeed, j.minSpeed);

		auto& an = d.anim;
		auto  anf = [&](const char* a_key, float& a_v) {
			a_v = static_cast<float>(ini.GetDoubleValue("Anim", a_key, a_v));
		};
		auto anb = [&](const char* a_key, bool& a_v) { a_v = ini.GetBoolValue("Anim", a_key, a_v); };
		anb("bEnabled", an.enabled);
		anb("bNpcs", an.npcs);
		anf("fRadius", an.radius);
		an.maxActors = std::clamp(static_cast<int>(ini.GetLongValue("Anim", "iMaxActors", an.maxActors)), 1, 100);
		anb("bClimb", an.climb);
		anb("bJumpPose", an.jumpPose);
		anf("fClimbMin", an.climbMin);
		anf("fClimbMaxFrac", an.climbMaxFrac);
		anf("fClimbApproach", an.climbApproach);
		anf("fClimbTail", an.climbTail);
		anf("fClimbRate", an.climbRate);
		anb("bHang", an.hang);
		anf("fHangTail", an.hangTail);
		anb("bLog", an.log);

		g_data = std::move(d);
		logger::info("Config loaded (enabled {})", g_data.enabled);

		// v36: the game's melee attack check waits fCombatAttackAnimationDrivenDelayTime (1.5s) before an NPC that is
		// still following its path may start an attack - the pause after a route reaches its target. Found in the
		// 1.6.1170 exe. Set here (every load, so it also restores the vanilla value when the INI says -1).
		if (auto* gs = RE::GameSettingCollection::GetSingleton()) {
			if (auto* s = gs->GetSetting("fCombatAttackAnimationDrivenDelayTime")) {
				static float vanilla = -1.0f;
				if (vanilla < 0.0f) {
					vanilla = s->data.f;
				}
				const float want = g_data.jump.attackDelayWhilePathing >= 0.0f ? (std::min)(g_data.jump.attackDelayWhilePathing, 5.0f) : vanilla;
				s->data.f = want;
				logger::info("fCombatAttackAnimationDrivenDelayTime = {:.2f} (game value {:.2f})", want, vanilla);
			}
		}
	}
}
