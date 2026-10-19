# Reverse-engineering notes (Skyrim SE 1.6.1170)

Findings from analysing an unpacked `SkyrimSE.exe` 1.6.1170 in Ghidra 12.1.4 (October 2026), with function names
from Address Library `versionlib-1-6-1170-0.bin` + CommonLibSSE-NG. Offsets are RVAs (image base 0x140000000).
The game exe and the Address Library file are NOT in this repo - only the scripts and what was learned.

## How to rebuild the Ghidra project

1. Unpack the Steam exe with Steamless, get `versionlib-1-6-1170-0.bin` from Address Library.
2. `python3 decode_versionlib.py versionlib-1-6-1170-0.bin ae1170.tsv` (ID -> offset table)
3. `python3 names.py <CommonLibSSE-NG checkout>` (reads `ae1170.tsv`, writes `names1170.tsv`: ~8000 named addresses)
4. Headless import + analysis (about 8 minutes on 2 CPUs with FastOptions):
   `GHIDRA_HEADLESS_MAXMEM=5G analyzeHeadless proj sky -import SkyrimSE.exe -scriptPath . -preScript FastOptions.java`
5. Names: `analyzeHeadless proj sky -process SkyrimSE.exe -noanalysis -scriptPath . -postScript ApplyNames.java names1170.tsv`
6. Queries (read-only, see `run.sh`): `Decomp.java out.c <rva...>`, `Refs.java out.txt <rva...>`,
   `Syms.java out.txt <substring...>`, `FieldUse.java out.txt <getterRva,...> <disp...>`

## Verified on 1.6.1170

- Hooks used by FluidMotionNPC are correct: NPC animation job site ID 41370+0x13E (`lea rdx` -> ID 41453),
  Character vtable slot 0x7D = UpdateAnimation, Actor slot 0xAD = Update, ActorSpeedChannel slot 1,
  MovementAgentPathFollowerStandard vtable[1] slot 2 = path follower update (0x11b0870).

## Animation / locomotion

- The exe never sends `moveStart` / `moveStop` - those strings do not exist in it. Walk/run start inside the
  behaviour graph from the values the engine feeds every frame (`Speed` via ActorSpeedChannel, Direction, ...).
- Graph -> engine: when the graph raises `bForceIdleStop`, the engine calls `AIProcess::StopCurrentIdle(actor, true)`
  (0x695d30). That is the engine's way to end an idle; FluidMotionNPC's BreakIdle uses it (v36).
- Never send `IdleForceDefaultState` to armed NPCs: it resets the graph to unarmed and attacks are refused
  afterwards (Macsut v35).
- `Actor::IsInJumpState` reads graph variable `bInJumpState`; it stays true for the whole normal landing animation.
- `Actor::IsMoving` (0x69c420) reads the AI movement speed, not the animation.
- `SpeedSampled` is a graph output (the speed the locomotion animation actually plays); logged in SLIDEGUARD lines.

## Path follower state (follower + 0x118; path + 0x128, progress + 0x100)

| state | meaning |
|---|---|
| 3 | turning toward the path start |
| 4 | starting the path |
| 13 | following the path |
| 8, 10 | path end reached, keep last direction |
| 9 | path INCOMPLETE - target unreachable, walked as far as possible |
| 11, 12 | finished |
| 0 | idle |

Path: node array at +0x90 (stride 0x48), node count at +0xA0, incomplete flag at +0xA8.

## Combat

- Melee attack selection (0x8a28b0): while the NPC `IsPathing`, no attack starts for
  `fCombatAttackAnimationDrivenDelayTime` seconds (vanilla 1.5) -> the pause after a route arrives.
  FluidMotionNPC sets it from INI `fAttackDelayWhilePathing` (default 0.5, -1 = leave).
- Attack check range: `fCombatAttackCheckTargetRangeDistance` (512) + reach; attacks with a stamina cost are refused at 0 stamina.
- `Actor::EvaluatePackage(false,false)` only sets a "re-check packages" flag - useless for combat.

## Stamina

- Sprint stamina drain (0x6a2970, cost = (fSprintStaminaWeightMult * weight + fSprintStaminaWeightBase) * dt *
  fSprintStaminaDrainMult) only applies to the player and the player's horse. NPCs never lose stamina from sprinting.
- Graph variable `IsSprinting` is synced back to the actor's sprint flag every frame.

## Useful addresses (1.6.1170 RVA)

FixedStrings singleton 0x20f6370 (GetSingleton 0x14ef60; fields per CommonLib FixedStrings.h),
Actor::IsPathing 0x693e10, Actor::StopMoving 0x6934e0, Actor::GetAttackChance 0x8dcd30,
Actor::EvaluatePackage 0x66ccf0, game settings: fCombatAttackAnimationDrivenDelayTime value at 0x1ffe550,
fSprintStaminaDrainMult value at 0x2005790.
