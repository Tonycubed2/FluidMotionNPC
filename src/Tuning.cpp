#include "Tuning.h"
#include "Config.h"
#include "JumpNav.h"

namespace Tuning
{
	namespace
	{
		std::atomic<Mode> g_mode{ Mode::kFluid };

		void Apply()
		{
			g_mode = Config::Get().enabled ? Mode::kFluid : Mode::kVanilla;
			JumpNav::enabled = g_mode == Mode::kFluid && Config::Get().jump.enabled;
		}
	}

	void Init() { Reload(); }

	void Reload()
	{
		Apply();
		logger::info("Mode: {}", ModeLabel());
	}

	Mode GetMode() { return g_mode; }

	const char* ModeLabel() { return g_mode == Mode::kFluid ? "fluid" : "vanilla"; }
}
