#pragma once

// v40: optional in-game settings page for SKSE Menu Framework (Nexus 120352). Does nothing when it isn't installed.
namespace Menu
{
	void Register();      // kDataLoaded, after Config::Load
	void OnGameLoaded();  // after a save load re-read the INI
	void RunFrame();      // main thread, every frame: applies changes made in the menu
}
