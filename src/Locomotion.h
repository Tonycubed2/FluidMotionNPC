#pragma once

#include "Config.h"

namespace Locomotion
{
	void Install();
	void Reload();
	void OnFrame(float a_delta);
	void Reset();

	struct ClimbPose
	{
		int          clip = 0;
		int          rig = 0;
		float        w = 0;
		float        t = 0;
		float        contact = 0;
		RE::NiPoint3 hand[2];
	};
	void          SetClimb(RE::FormID a_id, const ClimbPose* a_pose);
	bool          IsDriven(RE::FormID a_id);
	void          NoteDriven(RE::FormID a_id, float a_speed);
	// Walk/run speed the game's AI is asking this NPC's animation for (u/s), read from the speed channel.
	// -1 when unknown (NPC not among the nearby actors the pose layer samples, or no recent reading).
	float         CommandedSpeed(RE::FormID a_id);
	Config::Anim& Settings();
}
