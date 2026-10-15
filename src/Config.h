#pragma once

namespace Config
{
	struct Jump
	{
		bool  enabled = true;
		bool  shortcut = true;
		bool  pursuit = false;
		bool  follow = true;
		bool  followerJumps = true;
		bool  combatJumps = true;
		bool  route = true;
		float routeMaxDist = 2500.0f;
		float routeStuckSec = 1.0f;
		float routeStep = 90.0f;
		int   routeMaxNodes = 500;
		int   routeMaxNodesNear = 3000;
		int   routeNearCount = 3;
		bool  jumpOnNavmesh = false;
		float glideSpeed = 400.0f;
		float gravity = 900.0f;
		float upPerHeight = 1.15f;
		float upMin = 60.0f;
		float upMax = 180.0f;
		float downMax = 450.0f;
		float distPerHeight = 2.0f;
		float distMin = 150.0f;
		float distMax = 350.0f;
		float maxActorHeight = 300.0f;
		float minSaving = 300.0f;
		float minRatio = 1.6f;
		float minSpeed = 250.0f;
		float maxSpeed = 600.0f;
		float clearance = 12.0f;
		float cooldown = 1.5f;
		float followRouteMin = 350.0f;
		float turnRateDeg = 420.0f;
		float driveRise = 220.0f;
		float driveFall = 500.0f;
		bool  raceMotion = true;
		float driveTurnScale = 1.0f;
		float glideAccel = 700.0f;
		float pursuitStuckSec = 1.0f;
		float followStuckSec = 1.0f;
		float followMinSaving = 160.0f;
		float followMinRatio = 1.3f;
		bool  followVault = true;
		float followVaultMinSaving = 95.0f;
		float followVaultMinRatio = 1.5f;
		float followVaultMaxDz = 36.0f;
		float pursuitMinRise = 70.0f;
		float pursuitMinDrop = 120.0f;
		float meleeRise = 0.75f;
		int   driveMode = 0;
		std::vector<std::string> excludeRaces;
		std::vector<std::string> noRouteRaces;
		std::vector<std::string> climbRaces;
		std::vector<std::string> steepRaces;
		float steepRise = 130.0f;
		float steepCost = 1.4f;
		float wadeDepth = 0.45f;
		float wadeCost = 1.5f;
		float vertMismatch = 1.0f;
		float giantHeight = 220.0f;
		float giantDistMax = 700.0f;
		float giantUpPerHeight = 1.4f;
		float giantDistPerHeight = 2.6f;
		std::string npcStartEvent = "JumpDirectionalStart";
		std::string npcLandEvent = "JumpLand";
		std::string creatureStartEvent;
		std::string creatureLandEvent;
		bool        log = false;
	};

	// NPC traversal animation (climb / ledge hang / jump clips). Player is never affected.
	struct Anim
	{
		bool  enabled = true;
		bool  npcs = true;
		bool  jumpPose = true;
		float radius = 1800.0f;
		int   maxActors = 24;
		bool  climb = true;
		float climbMin = 38.0f;
		float climbMaxFrac = 1.0f;
		float climbApproach = 0.1f;
		float climbTail = 0.35f;
		float climbRate = 1.2f;
		bool  hang = true;
		float hangTail = 0.55f;
		bool  log = false;
	};

	struct Data
	{
		bool enabled = true;
		Jump jump;
		Anim anim;
	};

	const Data& Get();
	Data&       Mutable();
	void        Load();
}
