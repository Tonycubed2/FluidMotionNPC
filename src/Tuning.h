#pragma once

namespace Tuning
{
	enum class Mode
	{
		kVanilla,
		kFluid
	};

	void        Init();
	void        Reload();
	Mode        GetMode();
	const char* ModeLabel();
}
