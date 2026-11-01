# FluidMotionNPC — licensing notice

FluidMotionNPC is a modified version of **Fluid Motion: Pathfinding Overhaul**
Copyright (C) megakuutti — https://github.com/apinanaivot/FluidMotion-publicsource

Modifications Copyright (C) 2026 Tony Torres (tonycubed2).

This program is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, version 3 of the License.
It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See `LICENSE` for the
full text of the GNU General Public License version 3.

Complete corresponding source code: https://github.com/Tonycubed2/FluidMotionNPC

## Libraries

FluidMotionNPC links CommonLibSSE-NG (https://github.com/alandtse/CommonLibSSE-NG), licensed
GPL-3.0-or-later WITH Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source).
The exception text is in `CommonLibSSE-NG-EXCEPTIONS.md`. Other bundled libraries and their notices
are listed in `THIRD_PARTY_NOTICES.md`.

The climb, hang and jump animation clips (`FluidMotion_*.bin`) are from the original Fluid Motion
release by megakuutti and are distributed under the same terms.

## Changes from the original (October 2026)

- Removed: player movement, heading, camera and input changes; motion matching; player climbing;
  vanilla pathfinding code patches (SteerFix, CornerLimit, TurnFix, EndBrake); always-on pose effects;
  spider procedural legs and wall crawling; combat auto-attack; telemetry and debug probes; GMST
  overrides; debug overlay; crash logger; SKSE Menu Framework menu.
- `src/Locomotion.cpp`: rewritten as an NPC-only pose layer for climb/hang/jump clips; bone
  redirection only while an NPC is traversing.
- `src/PathProbe.*`: reduced to path reading only.
- `src/Mantle.cpp`, `src/JumpNav.cpp`: player and spider paths removed.
- `src/Config.*`, `src/Tuning.*`, `src/main.cpp`, `src/FrameHook.cpp`: reduced to the kept features;
  new INI name `FluidMotionNPC.ini`.
- Added addresses for SE 1.5.97 and VR 1.4.15; verified against AE 1.6.1170, 1.7.99 and 1.7.104.
