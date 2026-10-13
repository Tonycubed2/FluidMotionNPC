#pragma once

// Reads the engine's current path for an actor (path follower -> BSPathingSolution) so JumpNav can look for
// shortcuts along it. Stripped from Fluid Motion's PathProbe: only the path reading is left, no telemetry.
namespace PathProbe
{
	struct PathNode
	{
		RE::NiPoint3 pos;
		RE::NiPoint3 tangent;
		bool         action = false;
	};
	struct PathGeometry
	{
		static constexpr std::uint32_t  kMaxNodes = 64;
		std::uint32_t                   count = 0;
		std::uint32_t                   total = 0;
		bool                            incomplete = false;
		std::array<PathNode, kMaxNodes> nodes{};
	};
	bool ReadPath(std::uintptr_t a_path, PathGeometry& a_out);

	void Install();
	void Refresh(const std::vector<RE::Actor*>& a_actors);

	struct PathRef
	{
		std::uintptr_t path = 0;
		float          progress = 0.0f;
		std::uint32_t  state = 0;
	};
	std::optional<PathRef> GetPathRef(RE::Actor* a_actor);
}
