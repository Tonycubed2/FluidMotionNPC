// FluidMotionNPC - NPC terrain traversal only (jumps, routes over obstacles, climbing, ledge hangs).
// A stripped fork of "Fluid Motion: Pathfinding Overhaul" by megakuutti (GPL-3.0).

#include "Api.h"
#include "Config.h"
#include "FrameHook.h"
#include "JumpNav.h"
#include "Locomotion.h"
#include "PathProbe.h"
#include "Tuning.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Config::Load();
			Locomotion::Install();
			Locomotion::Reload();
			PathProbe::Install();
			FrameHook::Install();
			Tuning::Init();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Config::Load();
			Tuning::Reload();
			Locomotion::Reset();
			Locomotion::Reload();
			JumpNav::Reset();
			Api::Reset();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .logPattern = "[%H:%M:%S.%e] [%l] %v", .trampoline = true, .trampolineSize = 256 });

	const auto ver = a_skse->RuntimeVersion();
	logger::info("FluidMotionNPC (NPC terrain traversal) loaded, game runtime {}", ver.string());
	const bool checked = ver == SKSE::RUNTIME_SSE_1_5_97 || ver == SKSE::RUNTIME_SSE_1_6_1170 || ver == SKSE::RUNTIME_SSE_1_7_99 ||
	                     ver == SKSE::RUNTIME_SSE_1_7_104 || ver == SKSE::RUNTIME_VR_1_4_15;
	if (!checked) {
		logger::warn("FluidMotionNPC was checked against 1.5.97, 1.6.1170, 1.7.99, 1.7.104 and VR 1.4.15; on this runtime some hooks may refuse to install (see the lines below)");
	}

	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
