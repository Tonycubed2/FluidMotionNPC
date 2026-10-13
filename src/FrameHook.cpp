#include "FrameHook.h"
#include "JumpNav.h"
#include "Locomotion.h"

namespace FrameHook
{
	namespace
	{
		// Per-frame tick on the main thread. Only used as a clock; the player's own update is passed through untouched.
		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);
				float      dt = RE::GetSecondsSinceLastFrame();
				auto*      ui = RE::UI::GetSingleton();
				auto*      main = RE::Main::GetSingleton();
				const bool paused = (ui && ui->GameIsPaused()) || (main && main->GetRuntimeData().freezeTime);
				if (paused) {
					dt = 0.0f;
				}
				JumpNav::OnFrame(dt);
				Locomotion::OnFrame(dt);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = vtbl.write_vfunc(0xAD, PlayerUpdate::thunk);
		logger::info("FrameHook: per-frame tick installed");
	}
}
