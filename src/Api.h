#pragma once

// Internal side of the public API (see include/FluidMotionNPC_API.h for what other plugins see).
namespace Api
{
	// True while another plugin has claimed this actor (FMNPC_Claim): no jumps, routes, climbs or hangs start on it,
	// and any route it is on is dropped.
	bool  Claimed(RE::FormID a_id);
	// Turn-rate cap another plugin set for this actor (deg/s), or 0.
	float MaxTurn(RE::FormID a_id);
	// Another mod is animating this actor right now (SkyParkour / Follower Parkour graph flags). FluidMotionNPC waits.
	bool  ExternalBusy(RE::Actor* a_actor);

	// Called by the hooks.
	void RunPostGraph(RE::Actor* a_actor, int a_src);  // after an actor's animation graph updated (0 = worker job, 1 = main thread)
	void RunFrame(float a_dt);                         // once per frame on the main thread, after FluidMotionNPC's own work
	void Reset();
}
