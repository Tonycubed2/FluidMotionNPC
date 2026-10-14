#include "FrameHook.h"
#include "Api.h"
#include "JumpNav.h"
#include "Locomotion.h"

namespace FrameHook
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// Per-frame tick on the main thread. Only used as a clock; the player's own update is passed through untouched.
		//
		// The step time is the real time since the previous tick, capped by the game's frame time (so slow-time
		// effects still slow us down). If the hooked function ever runs more than once per frame (VR, other mods),
		// the extra calls just split the frame's time instead of multiplying every route/jump/climb speed.
		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);

				static Clock::time_point last = Clock::now();
				const auto               now = Clock::now();
				const float              wall = std::chrono::duration<float>(now - last).count();
				last = now;

				float      dt = (std::min)(wall, RE::GetSecondsSinceLastFrame());
				auto*      ui = RE::UI::GetSingleton();
				auto*      main = RE::Main::GetSingleton();
				const bool paused = (ui && ui->GameIsPaused()) || (main && main->GetRuntimeData().freezeTime);
				if (paused || !(dt > 0.0f)) {
					dt = 0.0f;
				}

				// One diagnostic line: ticks per second vs frames per second, measured once in the first unpaused 5 s.
				static int   ticks = 0;
				static float span = 0.0f, frameSpan = 0.0f;
				static bool  reported = false;
				if (!reported && !paused) {
					++ticks;
					span += wall;
					frameSpan = (std::max)(frameSpan, a_delta);
					if (span >= 5.0f) {
						reported = true;
						logger::info("FrameHook: {} ticks in {:.1f} s ({:.0f}/s); game frame time {:.4f} s", ticks, span, ticks / span,
							RE::GetSecondsSinceLastFrame());
					}
				}

				JumpNav::OnFrame(dt);
				Locomotion::OnFrame(dt);
				Api::RunFrame(dt);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		// Actor::Update is slot 0xAD on SE/AE and 0xAF on VR (VR inserts two virtuals before it).
		PlayerUpdate::func = vtbl.write_vfunc(REL::Module::IsVR() ? 0xAF : 0xAD, PlayerUpdate::thunk);
		logger::info("FrameHook: per-frame tick installed");
	}
}
