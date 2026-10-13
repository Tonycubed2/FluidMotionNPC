# FluidMotionNPC — NPC terrain traversal only

A stripped fork of **Fluid Motion: Pathfinding Overhaul** by megakuutti
(https://github.com/apinanaivot/FluidMotion-publicsource, GPL-3.0). Only the NPC terrain traversal is kept:

- **Jump shortcuts**: NPCs jump fences, ledges and gaps when that cuts a long detour off their path.
- **Follow / pursuit jumps**: followers stuck above or below the player jump to them (pursuit for melee enemies is off by default).
- **Routing**: when the navmesh can't reach the target, the NPC routes on foot over steep ground, through shallow water, with jumps and climbs.
- **Climbing and ledge hangs**: NPCs in a hurry climb waist-to-chest high obstacles, and grab and pull up onto ledges after a jump, using the original mocap clips.

## Removed

- Everything that touches the player: movement acceleration, heading, camera, input, motion matching, player climbing.
- Vanilla pathfinding patches: SteerFix, CornerLimit, TurnFix, EndBrake (raw-offset code patches).
- Always-on pose effects: lean, duck, step-over, head lead, landing crouch, stop settle, wall touch, stride/clip-speed hook.
- Spider procedural legs and spider wall crawling.
- Combat auto-swing.
- Telemetry, the path/angle/flee probes, GMST dump/overrides, debug overlay, crash logger, the SKSE Menu Framework menu.

## Compatibility changes

- Bone redirection (graph output into dummy nodes, re-applied with the climb pose) is only installed on an NPC while it
  is jumping or climbing, and removed about 30 frames after. The original kept it on every NPC within 1800 units.
- No hooks on the player, camera, input, MovementHandler, hkbClipGenerator, path steering, corner speed or turn code.
- Hooks left: PlayerCharacter::Update (tick only), Character::UpdateAnimation, ActorSpeedChannel, the NPC animation job
  (byte-checked), and the path follower update (read-only, to find NPC paths).
- Logging is off by default.

## Requirements

SKSE64, Address Library for SKSE Plugins. Built and checked against Skyrim AE 1.6.1170. No SKSE Menu Framework needed.

## Install

Copy `FluidMotionNPC.dll`, `FluidMotionNPC.ini` and the `FluidMotion_*.bin` clip files into `Data/SKSE/Plugins`.
Do not run it together with the original FluidMotion.dll.

## Build

Visual Studio 2022 (Desktop development with C++), xmake 3.x, git.
Put CommonLibSSE-NG in `lib/commonlibsse-ng`, then `xmake f -p windows -m releasedbg` and `xmake`.

## License

GPL-3.0, same as the original (see LICENSE). If you distribute a build you must also provide this source.
