# SPRINTS

Task format: `- [ ] **SN-XX** title (estimate)`, then **Done:** (what the agent verifies) and **User check:** (what you try by hand). Markers: 🔍 Research first · ⚠️ Note · _(requires SN-YY)_.

Detailed design for every task: [plans/2026-10-25-river-crossing.md](plans/2026-10-25-river-crossing.md) (Approach, Testing).

**Sprint-wide notes**
- ⚠️ Live Coding only handles function-body edits. New classes, headers or UPROPERTY changes need a full build with the **editor closed**. The agent asks you to close it before such a build.
- ⚠️ The agent can't run console commands through MCP. `stat`, cheats and `ShowDebug` are user checks. Numeric checks go through `AB1GymTestRig`.
- Build command: `& "C:\Program Files\UE_5.8\Engine\Build\BatchFiles\Build.bat" MudDriverEditor Win64 Development "E:\GameDev\Projects\MudDriver\MudDriver.uproject" -waitmutex`

---

## Sprint 1 — Foundation + mud core

**Goal.** Prove the two biggest unknowns, the Chaos simulation hook and mud sinking, in a gym with agent-runnable tests.

### Housekeeping

- [x] **S1-01** Low-end render budget (done 2026-10-25)
  **Done:** Lumen, RT, VSM, distance fields and Substrate off in `DefaultEngine.ini` (ADR-011).
  **User check:** ✅ the user restarted the editor and is happy with the result.

- [x] **S1-02** Rename debug classes `Mud*` → `B1*` (1 h) <!-- done: 2026-10-25 -->
  `UMudImGuiSubsystem` → `UB1ImGuiSubsystem`, `UMudCheatManager` → `UB1CheatManager`, `MudImGui` → `B1ImGui`, file names too; update `MudDriverPlayerController.cpp` (CheatClass).
  ⚠️ Note: search `Config/*.ini` and Blueprints for the old class names first. If any asset references them, add `+ActiveClassRedirects`.
  **Done:** full build passes; `grep -r "UMud\|MudImGui" Source/` returns nothing.
  **User check:** the NetImGui menu bar and the `NetImGuiLaunch` cheat still work.

- [x] **S1-03** B1 log categories (30 min) <!-- done: 2026-10-25; verified in a headless -game run (pawn BeginPlay line) -->
  `B1Log.h/.cpp` with `LogB1Vehicle`, `LogB1Physics`, `LogB1Test`.
  **Done:** builds; a test log line appears in `LogsToolset` output during PIE.
  **User check:** —

### Wheel debug

- [x] **S1-04** Per-wheel NetImGui table + telemetry (2 h) _(requires S1-02, S1-03)_ <!-- done: 2026-10-25; B1Wheels line verified headless, CVar b1.Vehicle.Telemetry -->
  Extend "Vehicle Basics" with a table per wheel: contact, phys material, slip, skid, normalized suspension length, drive/brake torque. Write a 2 Hz telemetry line to `LogB1Vehicle`.
  🔍 Research first: `FWheelStatus` fields and `UChaosWheeledVehicleMovementComponent::GetWheelState` in 5.8.
  **Done:** builds; the telemetry line appears in the log during PIE with plausible values.
  **User check:** drive on `Slippery` vs `NonSlippery`: the table makes sense and is readable.

- [x] **S1-05** `ShowDebug B1Vehicle` (1.5 h) _(requires S1-04)_ <!-- done: 2026-10-25 -->
  Speed, gear, RPM and per-wheel slip/material as text over the jeep through `AHUD::OnShowDebugInfo`.
  🔍 Research first: registering a custom ShowDebug category in 5.8 (`AHUD::OnShowDebugInfo`, `ShouldDisplayDebug`).
  **Done:** builds.
  **User check:** `ShowDebug B1Vehicle` shows readable text over the jeep and toggles off again.

### Simulation hook (ADR-003)

- [x] **S1-06** Template constructors → `FObjectInitializer` (1 h) <!-- done: 2026-10-25; headless Lvl_Offroad: identical warning set -->
  `AMudDriverPawn`, `AMudDriverOffroadCar` and `AMudDriverSportsCar` take and forward `const FObjectInitializer&`.
  ⚠️ Note: every subclass of `AMudDriverPawn` must forward it, or the build breaks. Behaviour must not change.
  **Done:** full build passes; `Lvl_Offroad` PIE starts with no new warnings in the log.
  **User check:** drive `Lvl_Offroad`: the jeep handles exactly as before.

- [x] **S1-07** `UB1VehicleMovementComponent` + `FB1VehicleSimulation` skeleton (2.5 h) _(requires S1-06)_ <!-- done: 2026-10-25; PT log line verified with S1-08 -->
  Override `CreatePhysicsVehicle()` to set `VehicleSimulationPT = MakeUnique<FB1VehicleSimulation>()` and then call `UChaosVehicleMovementComponent::CreatePhysicsVehicle()`, matching the 5.8 parent (`ChaosWheeledVehicleMovementComponent.h:862`). Add an empty `UpdateSimulation` override that calls Super.
  🔍 Research first: how the async input/output structs carry custom data to and from the PT simulation (`FChaosVehicleAsyncInput`, `FChaosVehicleAsyncOutput`). We'll need them for the mud and winch inputs.
  **Done:** builds; a one-time `LogB1Physics` line confirms our simulation is ticking on the physics thread.
  **User check:** —

- [x] **S1-08** `AB1Jeep` + `BP_B1Jeep` (1.5 h) _(requires S1-07)_ <!-- done: 2026-10-25; /Game/B1/Vehicles/BP_B1Jeep, 0 CDO diffs over 546 props; BP_OffRoadGameMode DefaultPawnClass → BP_B1Jeep; PIE PT line: game thread=0, dt=0.0333 -->
  `AB1Jeep : AMudDriverOffroadCar` that swaps in `UB1VehicleMovementComponent` via `SetDefaultSubobjectClass(VehicleMovementComponentName)`. `BP_B1Jeep` duplicated from `BP_OffroadCar_Pawn` and reparented through MCP.
  ⚠️ Note: check that the reparented BP keeps the torque/steering curves and wheel setups. Compare the CDO properties before and after with `ObjectTools.get_properties`.
  **Done:** builds; the BP compiles; PIE with `BP_B1Jeep` logs the S1-07 line.
  **User check:** drive `BP_B1Jeep`: it handles the same as the template jeep.

- [x] **S1-09** Test forces + cheat (2 h) _(requires S1-08)_ <!-- done: 2026-10-25; params on UB1VehicleMovementComponent.TestForceSettings; vehicle sleep disabled while active; headless -ExecCmds="EnableCheats,B1TestForce push" verified -->
  In `FB1VehicleSimulation`: (a) a constant sideways push, (b) a stiff one-sided spring-damper pulling toward a world anchor point (winch stand-in, capped force). Toggled from the game thread through the async input. Cheat `B1TestForce <off|push|spring>`.
  ⚠️ Note (S1-07 research): `UChaosVehicleMovementComponent::SetCurrentAsyncInputOutput` is non-virtual and always allocates the base `FChaosVehicleAsyncInput`/`Output`, so custom data can't ride in them. Channel instead: override `Update(DeltaTime)` (GT, runs while `CurAsyncInput` is set) to write a POD struct into a lock-guarded or triple buffer owned by `FB1VehicleSimulation`; read it in `UpdateSimulation` (PT). Output goes back the same way and is read in `ParallelUpdate`.
  **Done:** builds.
  **User check:** toggle `spring` and drive against it: no jitter or explosion. With `off`, handling is unchanged.

### Test rig

- [x] **S1-10** `AB1GymTestRig : AFunctionalTest` (3.5 h) _(requires S1-03, S1-08)_ <!-- done: 2026-10-25 -->
  Result: rig `B1Gym_FullThrottle5s` in `Lvl_Offroad` (spawns `BP_B1Jeep`, MinDistance 50 m). Baseline PASS: dist=88.01 vmax=23.79 peakOsc=4.135 avgDt=0.0167 frames=300. Per-wheel sink depth deferred to S1-18. Module split per ADR-013.
  Placed actor with: target jeep (or spawn class + transform), an input script (`TArray<{Duration, Throttle, Steer, Brake, Handbrake}>`), a forced frame cap (sets `t.MaxFPS` from code, restored after), metric collection (distance, per-wheel max sink depth, time-to-stuck, peak oscillation), pass thresholds, `bAutoRun` for PIE. One `LogB1Test` result line: `B1Test <name> PASS|FAIL key=value ...`.
  🔍 Research first: `AFunctionalTest` lifecycle (`PrepareTest`, `StartTest`, `FinishTest`), and feeding input to a Chaos pawn without a player (call `DoThrottle`/`DoSteering`, or the movement component setters, directly).
  ⚠️ Note: add the `FunctionalTesting` module to `MudDriver.Build.cs`, editor and development builds only.
  **Done:** builds; a 5 s full-throttle script in `Lvl_Offroad` logs a PASS line with the distance travelled.
  **User check:** place a rig in a level yourself, set `bAutoRun`, press Play: the jeep drives the script and the result shows in the log.

- [x] **S1-11** Headless test runner script (1 h) _(requires S1-10)_ <!-- done: 2026-10-25 -->
  Result: `-nullrhi` steps physics fine. Headless PASS dist=85.71 (88.01 in PIE). Exit code comes from parsing `Test Completed. Result={...}` lines since the editor exits 0 regardless; no match exits 1.
  `Scripts/RunGymTests.ps1 [-Map <path>] [-Filter <name>]`: runs `UnrealEditor-Cmd.exe` with `Automation RunTests Project.Functional`, `-unattended -nullrhi -log`, then prints only the `LogB1Test` lines and the exit code.
  ⚠️ Note: it needs the editor closed, or at least no conflicting asset saves. Check that `-nullrhi` still steps physics. If it doesn't, fall back to `-RenderOffscreen`.
  **Done:** the S1-10 test runs headless and the script prints its PASS line.
  **User check:** run the script once yourself.

- [x] **S1-12** Physics stepping decision (3 h) _(requires S1-09, S1-10)_ <!-- done: 2026-10-25 -->
  Result: async fixed 1/60 + `MaxPhysicsDeltaTime=0.1` (the 1/30 default clamp was the main frame-rate dependency). Push drift 20 vs 120 fps −0.9 %, spring settles 3.00/3.04 s, no stretch growth. Settle limit raised to 3.5 s (user's call: 3.0 s is the scenario's physical settle time in every config). Spring damping default 3000 → 9000. Rigs `B1Step_Spring20/120`, `B1Step_Push20/120` in `Lvl_Offroad`. Numbers in HISTORY.md. Headless re-run with 3.5 s done during S1-13: PASS, settleTaut 3.04/3.00.
  Rig tests for the spring force at 20 and 120 fps. Pass: oscillation amplitude is non-growing and settles within 3 s. Plus sideways-push drift within 10 % between the two frame rates. Run in two configs: (a) async physics, fixed dt 1/60 (`bTickPhysicsAsync`, `AsyncFixedTimeStepSize`), (b) sync + substepping. Pick the winner, set it in `DefaultEngine.ini`, update ADR-003 to accepted.
  🔍 Research first: Chaos async physics settings in 5.8, and what changes for game-thread reads of vehicle state.
  ⚠️ Note: async physics adds a frame of latency to game-thread reads. Check that the template UI, camera and flip check still behave.
  **Done:** both configs measured and the numbers recorded in HISTORY.md; the winner passes; ADR-003 updated.
  **User check:** drive with the chosen mode at `t.MaxFPS 20` and at uncapped: handling feels the same and nothing jitters.

### Mud gym (ADR-012)

- [x] **S1-13** `B1Mud` collision channel + profile (1.5 h) _(requires S1-08)_ <!-- done: 2026-10-25 -->
  Result: the Chaos suspension trace is a `WorldDynamic` channel query with `WheelTraceCollisionResponses`, so `B1MudSurface` (object `B1Mud` = `ECC_GameTraceChannel1`, QueryOnly) blocks only `WorldDynamic`. The trace response is set in the `UB1VehicleMovementComponent` constructor (any B1 vehicle), not `AB1Jeep`. Rigs in `Lvl_Offroad` (folder `B1Tests/MudChannel`, platforms at Z≈5000): `B1Mud_WheelsOnPlane` endH=-0.30 (floor would be -1.30), `B1Mud_ChassisThroughRail` dist=10.25 peakOsc=0.077 (with the rail still blocking: dist=0). PIE and headless PASS. New rig metric `endH` + optional `MinEndHeight`.
  New object channel `B1Mud` and profile `B1MudSurface`: blocks only the wheel trace query, ignored by the vehicle chassis and pawns. `AB1Jeep` sets `WheelTraceCollisionResponses` to block `B1Mud`.
  🔍 Research first: which channel and responses the Chaos suspension trace uses (`WheelTraceCollisionResponses`, the trace channel in `PerformSuspensionTraces`), so the chassis really ignores the mud surface.
  **Done:** builds; a rig test shows the wheels resting on a `B1Mud` plane while the chassis passes through a raised `B1Mud` box without contact.
  **User check:** —

- [x] **S1-14** `BP_B1MudPatch` (1.5 h) _(requires S1-13)_ <!-- done: 2026-10-25 -->
  Result: C++ `AB1MudPatch` (`Mud/`) builds both slabs from `Size` / `bHardBottom` / `HardBottomDepth` in `OnConstruction`; surface top = actor origin, 10 cm thick, profile via `SetCollisionProfileName`, `MudMaterial` → phys material override. `/Game/B1/Mud/BP_B1MudPatch` + `M_B1Mud` (`Color` vector param for per-depth tints in S1-15). Placed `B1MudTest_Patch` (8×8 m) in `Lvl_Offroad` and captured it; rig `B1Mud_PatchSurface` PASS in PIE, endH=-0.31 (hard bottom would be ≈-0.81). Headless run pending the next editor close.
  Actor with a mud surface mesh (`B1MudSurface` profile, mud phys material slot) and an optional hard-bottom mesh at a configurable depth below. Simple dark-brown unlit-ish material.
  ⚠️ Note (S1-13): a static mesh component with `bUseDefaultCollision=true` takes the mesh asset's collision and ignores the profile. Use `SetCollisionProfileName` (clears the flag) in C++, or untick it in the BP. Setting `BodyInstance.collisionProfileName` through MCP alone doesn't apply.
  **Done:** places through MCP; a viewport capture shows the patch.
  **User check:** —

- [x] **S1-15** `Lvl_Gym_Mud` greybox (2 h) _(requires S1-14)_ <!-- done: 2026-10-25 -->
  Result: `/Game/B1/Gym/Lvl_Gym_Mud` (copy of `VehicleBasic`, WP/OFPA, landscape and props removed; sky and lights kept). Ground is `WorldGridMaterial` cube tiles with tops at Z=0. Lanes run along +X, and the patches sit at X 10–30 m in gaps between the tiles: firm (Y 0), shallow 15 cm (Y 16), medium 35 cm (Y 29), deep 70 cm (Y 42), each 20×10 m. Pit at Y −19: 2 m deep, ramp in 8 m (14°), ramp out 12 m (9.5°), mud surface 60 cm over the floor (`bHardBottom` off). Tints `MI_B1Mud_Shallow/Medium/Deep`. `Spawn_1..5` PlayerStarts at X −12 m; `Spawn_1_Firm` has the `Player0` tag, which `AOffroadGameMode` always picks. `BP_B1GymGameMode` (child of `BP_OffRoadGameMode`) is the world override. PIE: jeep at (−12.0, 0.0, 0.03) m. No `MudMaterial` yet (S1-17). After the user's first look: a 30 m firm margin on every side (floor X −50..120, Y −60..80 m) plus 3 m perimeter walls (`Gym/Walls`, hidden in game, Custom collision with Camera ignored). Rig `B1Gym_WallStop` (full throttle into the north wall) PASS: disp=22.83 stuck=4.60 endH=−0.99.
  A flat ground with a firm 40 m strip, shallow/medium/deep patches (≥ 20 m each), a mud pit with a slope out, labelled spawn points, the gym game mode spawning `BP_B1Jeep`. Built through MCP.
  **Done:** a viewport capture of the layout; PIE spawns the jeep at the first spawn point.
  **User check:** walk or drive through the gym: is the layout good for testing, and can you tell the patches apart at a glance?

- [x] **S1-16** Gym smoke test (45 min) _(requires S1-11, S1-15)_ <!-- done: 2026-10-25 -->
  Result: rig `B1Gym_FirmSmoke` (folder `B1Tests`, X 35 m on the firm lane, 6 s at 50 % throttle, MinDistance 20, endH ≥ −1.5). PIE PASS dist=42.82; headless PASS dist=42.97 (avgDt 0.0216, the async fixed step keeps the result the same). `B1Gym_WallStop` headless PASS. `Lvl_Offroad` suite headless 8/8 PASS, which closes the pending `B1Mud_PatchSurface` headless run (endH=−0.30).
  A rig in `Lvl_Gym_Mud`: drive 20 m on the firm strip at 50 % throttle.
  **Done:** PASS in PIE (MCP) and headless (S1-11).
  **User check:** —

### Mud core (ADR-001, 002, 012)

- [ ] **S1-17** `UB1PhysicalMaterial_Mud` + presets (1 h) _(requires S1-14)_
  Params: `MaxSinkDepth`, `SinkRate`, `RiseRate`, friction curve and rolling-resistance curve vs normalized depth, `BellyDragCoefficient`. Assets `PM_B1Mud_Shallow`, `_Medium`, `_Deep`, assigned to the gym patches.
  **Done:** builds; the per-wheel table (S1-04) shows the mud material on the patches.
  **User check:** —

- [ ] **S1-18** Per-wheel `SinkDepth` state (3 h) _(requires S1-07, S1-17)_
  In `FB1VehicleSimulation`: read the contact material per wheel, update `SinkDepth` from slip (sink) and rolling traction (rise), clamp, and output it through the async output for debug.
  ⚠️ Note: don't let the physics thread read the UObject while ImGui edits it on the game thread. Copy the mud params into the async input each frame, keyed by material.
  **Done:** builds; a rig at full throttle on medium mud logs a rising `SinkDepth` per wheel.
  **User check:** —

- [ ] **S1-19** Sinking via suspension trace offset (3 h) _(requires S1-18)_
  Override `PerformSuspensionTraces`: on mud, offset the hit point down by `SinkDepth`, ramped in over the first ~0.5 m of the patch.
  ⚠️ Note: watch for suspension pops at patch edges and when `SinkDepth` changes quickly. Rate-limit the offset change.
  **Done:** builds; a viewport capture shows the jeep sitting visibly lower in deep mud; a rig records no suspension spike > threshold at the patch entry.
  **User check:** drive in and out of the patches: does the jeep sink believably, with no pops?

- [ ] **S1-20** Traction + rolling resistance vs depth (3 h) _(requires S1-18)_
  Per wheel: friction multiplier from the curve, and a rolling-resistance force opposing wheel velocity at the contact.
  **Done:** builds; rig: full throttle on medium mud digs in within 2–4 s.
  **User check:** —

- [ ] **S1-21** Belly drag (2 h) _(requires S1-19, S1-20)_
  Analytic drag when the average `SinkDepth` > ground clearance, growing with the excess.
  **Done:** builds; rig: full throttle in deep mud ends stuck (< 0.5 m/s after 5 s).
  **User check:** —

- [ ] **S1-22** NetImGui "Mud" window + telemetry (3 h) _(requires S1-18)_
  Per-wheel depth, slip, friction, resistance, plus belly drag. Live-edit the presets' params, with a "save to asset" button. 2 Hz telemetry line to `LogB1Physics`.
  **Done:** builds; the telemetry line appears during the rig run.
  **User check:** edit `SinkRate` live while driving: the effect is immediate and saving persists it.

- [ ] **S1-23** Mud rig test suite (2.5 h) _(requires S1-16, S1-21)_
  - full throttle, medium: stuck within 2–4 s
  - ≤ 50 % throttle crosses a 20 m medium strip
  - deep: stuck
  - stuck + zero throttle: creep < 5 cm in 10 s (Chaos low-speed check)
  - patch-entry pop check
  **Done:** all PASS in PIE and headless; the numbers are recorded in HISTORY.md.
  **User check:** —

- [ ] **S1-24** Mud playtest + tuning pass (user + agent, 1–2 h) _(requires S1-22, S1-23)_
  You playtest `Lvl_Gym_Mud` and tune live. The agent writes the tuned values into the presets and re-runs S1-23 (thresholds get adjusted if your feel disagrees). You also check `stat unit` on the laptop.
  **Done:** the suite passes with the tuned values.
  **User check:** ✅ sign-off: "mud rewards driving slowly with intent".

---

## Sprint 2 — GAS + fuel + mud polish (provisional, detailed at sprint start)

Scope from ROADMAP Phase G and the rest of Phase 1: GAS bootstrap (`AB1GameMode`, `AB1PlayerController`, `AB1PlayerState`, single ASC), `UB1VehicleAttributeSet`, fuel (ADR-010), Vehicle Status window and cheats, `Vehicle.State.InMud`, `B1Unstuck`, `ShowDebug B1Mud`, Niagara spray, visual trails, Phase 1 close playtest.
