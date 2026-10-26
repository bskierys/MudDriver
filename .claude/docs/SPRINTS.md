# SPRINTS

Task format: `- [ ] **SN-XX** title (estimate)`, then **Done:** (what the agent verifies) and **User check:** (what you try by hand). Markers: 🔍 Research first · ⚠️ Note · _(requires SN-YY)_.

Detailed design for every task: [plans/2026-10-25-river-crossing.md](plans/2026-10-25-river-crossing.md) (Approach, Testing).

**Sprint-wide notes**
- ⚠️ Live Coding only handles function-body edits. New classes, headers or UPROPERTY changes need a full build with the **editor closed**. The agent runs `Scripts/BuildEditor.ps1`, which closes the editor, builds and reopens it. You only step in if a save prompt blocks the close.
- ⚠️ The agent can't run console commands through MCP. `stat`, cheats and `ShowDebug` are user checks. Numeric checks go through `AB1GymTestRig`.
- Build command: `& "C:\Program Files\UE_5.8\Engine\Build\BatchFiles\Build.bat" MudDriverEditor Win64 Development "E:\GameDev\Projects\MudDriver\MudDriver.uproject" -waitmutex`

---

## Sprint 1 — Foundation + mud core

**Status:** ✅ closed 2026-10-26, signed off by the user after the mud playtest.

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

- [x] **S1-17** `UB1PhysicalMaterial_Mud` + presets (1 h) _(requires S1-14)_ <!-- done: 2026-10-25 -->
  Result: `Mud/B1PhysicalMaterial_Mud` (category `Mud|Config`). Units: depth in cm, `SinkRate` in cm/s at full wheelspin, `RiseRate` in cm per m rolled with grip; curves take normalized depth; rolling resistance is a fraction of wheel load; belly drag is a fraction of vehicle weight per m past clearance. Base `Friction` is the zero-depth grip: Chaos already feeds it to the tire (`SetSurfaceFriction`), so S1-20's curve multiplies on top. Presets (Friction / MaxSink / Sink / Rise / friction curve / RR curve): Shallow 0.6/15/10/8/1→0.8/0.03→0.12, Medium 0.5/35/20/5/1→0.6/0.05→0.3, Deep 0.45/70/30/2/1→0.45/0.08→0.5. The pit uses Deep. Verified: B1Wheels telemetry shows `PM_B1Mud_Shallow/Medium/Deep` on all four wheels of jeeps dropped on each patch. MCP can't create physical material assets: the user made `PM_B1Mud_Medium` by hand and the agent duplicated it.
  Params: `MaxSinkDepth`, `SinkRate`, `RiseRate`, friction curve and rolling-resistance curve vs normalized depth, `BellyDragCoefficient`. Assets `PM_B1Mud_Shallow`, `_Medium`, `_Deep`, assigned to the gym patches.
  **Done:** builds; the per-wheel table (S1-04) shows the mud material on the patches.
  **User check:** —

- [x] **S1-18** Per-wheel `SinkDepth` state (3 h) _(requires S1-07, S1-17)_ <!-- done: 2026-10-25 -->
  Result: `FB1VehicleSimulation::UpdateSinkDepth`, per wheel on mud:
  - `+= SinkRate·spin·dt` and `−= RiseRate·rolled m·(1−spin)`, clamped to [0, MaxSinkDepth];
  - `spin = clamp(|slip speed| / FullSinkSlipSpeed, 0, 1)` (new material param, default 5 m/s);
  - non-mud contact → 0; airborne keeps its depth.

  The game thread copies params for every mud material the wheels have touched (`KnownMudMaterials`) into `FB1VehicleSimInput::MudMaterials`; the physics thread matches `TraceResult.PhysMaterial` by weak-pointer equality, with no UObject reads. Output comes back via the lock-guarded `FB1VehicleSimOutput` → `GetWheelSinkDepth`. Telemetry gets `sink=` per wheel; the rig gets `sink=` (max per wheel). Rig `B1Mud_SinkMedium` (2 s full throttle from the medium patch entry) headless: sink=7.4/8.8/0.0/0.0. Telemetry rises 4.7→6.8 / 4.8→7.8 cm. The rears grip under weight transfer (slip 0) so they don't dig in; that should change once S1-20 lowers grip with depth. Gym 3/3 and `Lvl_Offroad` 8/8 PASS headless.
  In `FB1VehicleSimulation`: read the contact material per wheel, update `SinkDepth` from slip (sink) and rolling traction (rise), clamp, and output it through the async output for debug.
  ⚠️ Note: don't let the physics thread read the UObject while ImGui edits it on the game thread. Copy the mud params into the async input each frame, keyed by material.
  **Done:** builds; a rig at full throttle on medium mud logs a rising `SinkDepth` per wheel.
  **User check:** —

- [x] **S1-19** Sinking via suspension trace offset (3 h) _(requires S1-18)_ <!-- done: 2026-10-25 -->
  Result: `FB1VehicleSimulation::PerformSuspensionTraces` traces from a copy of the traces with each start raised by the wheel's `SinkOffset`. Chaos reads `Distance` as from the real start, so the hit lands exactly `SinkOffset` deeper; `Location`/`ImpactPoint` move down to match, clamped to the real sweep length (full droop). Raising the start keeps the sweep above the 10 cm mud slab when the chassis is low. `SinkOffset` follows `SinkDepth` with a 50 cm/s rate limit. No distance-based entry ramp: depth always starts at 0 on mud entry, so the rate limit covers exits, landings and material changes.
  - Visual (Simulate + `CaptureEditorImage`): jeep on the deep patch, front wheels down in the mud, z −35.8 cm vs +2.6 cm on firm ground.
  - New rigs in `Lvl_Gym_Mud`: `B1Mud_ExitMediumFull` (full throttle from just before the medium patch; exits with the right front 25–29 cm deep at 50–60 km/h): PASS, peakOsc=0.204 vs 0.16 on firm, limit 0.5. `B1Mud_CrossMedium` (50 %, for S1-23; no slip, so no sink): peakOsc=0.163. `B1Mud_SinkDeepStop` (1.5 s full throttle then brake). `B1Mud_SinkMedium` now ends endH −1.04 (was −0.98).
  - Found: an unloaded wheel at full droop (open diff, one wheel spinning at 16–19 m/s) keeps spinning and sinks to `MaxSinkDepth`. Sinking doesn't depend on wheel load yet → backlog. Brake at standstill engages reverse in the template, which matters for rig scripts.
  - **Rework after the user's playtest** ("only fronts sink", "should sink from weight at any speed", "too little sinking"):
    - Weight sets a static floor `StaticSinkDepth = MaxSinkDepth·StaticSinkRatio (0.45)·load`, reached at `SettleRate` (25 cm/s) at any speed. Wheelspin digs below it (`SinkRate·spin·load`); rolling with grip climbs back only to the floor.
    - Load = wheel load / average load of the wheels in contact (Chaos's reported load sits around 0.6× `RestingForce` at rest), capped at 1.5. An unloaded wheel no longer digs (closes the S1-19 backlog item).
    - Presets: SinkRate 15/30/45, RiseRate 4/2/1, FullSinkSlipSpeed 3 m/s.
    - `SinkOffset` limits only rising, now at 30 cm/s (50 gave peakOsc 0.608 on the fast exit).
    - `ChaosVehiclesCore` is now a private dependency (needed for `Setup()`).
  - Headless after the rework (gym 6/6, `Lvl_Offroad` 8/8 PASS), sink per wheel F/F/R/R:
    - User check ✅: "The mud sinking feels better now."
    - CrossMedium (50 %): sink=16/16/20/20, peakOsc 0.361.
    - SinkMedium (2 s full throttle): 35/35/24/24, endH −1.28 (30 cm lower).
    - ExitMediumFull: 35/35/23/23, peakOsc 0.410 (limit 0.5).
    - SinkDeepStop: 70×4, endH −1.63; the chassis rests on the hard bottom.
  Override `PerformSuspensionTraces`: on mud, offset the hit point down by `SinkDepth`, ramped in over the first ~0.5 m of the patch.
  ⚠️ Note: watch for suspension pops at patch edges and when `SinkDepth` changes quickly. Rate-limit the offset change.
  **Done:** builds; a viewport capture shows the jeep sitting visibly lower in deep mud; a rig records no suspension spike > threshold at the patch entry.
  **User check:** drive in and out of the patches: does the jeep sink believably, with no pops?

- [x] **S1-20** Traction + rolling resistance vs depth (3 h) _(requires S1-18)_ <!-- done: 2026-10-25 -->
  Result: `FB1VehicleSimulation::ApplyWheelFrictionForces` override.
  - **Traction:** before the base call, each mud wheel's `FrictionMultiplier` is scaled by `FrictionVsDepth`, then restored. The base call sets `SurfaceFriction` from the material itself, so the multiplier is the only hook.
  - **Rolling resistance:** after the base call. Size is `RollingResistanceVsDepth` × the wheel's share of vehicle weight (mass·g / wheels in contact × relative load). It acts along the steered wheel forward at the contact point.
    - It opposes motion; below 1 cm/s it opposes the tire's push, like static friction.
    - It's capped at the tire's push + 0.4 × the stopping force, so a stuck jeep with spinning wheels sits still instead of creeping or jittering.
  - Curves are copied to the physics thread as `FRichCurve`s in `FB1MudSimParams`; relative loads are shared with sinking.
  - **Speed drag:** `SpeedDragCoefficient` × normalized depth × (wheel speed m/s)² is added to the resistance coefficient. It bites hard at speed and barely matters below ~10 km/h.
  - **Climbing out:** `SinkOffset` rise is limited to max(30 cm/s, 0.5 × wheel speed), i.e. 0.5 cm per cm rolled, so a fast exit doesn't keep driving sunk into firm ground.
  - Telemetry has `mu=` (curve value) and `rr=` (N, including speed drag) per wheel.
  - First pass (stuck-within-2–4-s target, medium RR up to 0.55 W, `SinkRate` 8) failed the user's playtest:
    - slow driving into medium got stuck for good, at any throttle;
    - deep at full speed barely sank or slowed;
    - after leaving deep the jeep stayed sunk into the firm tiles.
  - Causes:
    - From a standstill, 40 % throttle gives only ~290 Nm per wheel (~570 N at the tire, engine near idle). That was below medium's static rolling resistance at resting depth.
    - Deep had no speed-dependent drag.
    - The 30 cm/s rise limit spans metres at speed.
  - **Design change (user's feel over the old criterion):** medium is a slog, not a trap. Grip at full depth stays above rolling resistance, and full-throttle wheelspin digs to full depth and caps speed via drag. Full stops are for deep (belly drag, S1-21). Rolling resistance values are now in the real-world mud range.
  - Presets (friction curve end / RR curve / SpeedDrag; other params as S1-19):
    - Shallow 0.8 / 0.02→0.08 / 0.006;
    - Medium 0.5 / 0.02→0.12 / 0.018, `SinkRate` back to 30;
    - Deep 0.4 / 0.03→0.25 / 0.03, `SettleRate` 25→60.
  - New rigs in `Lvl_Gym_Mud` (`B1Tests`), headless results:
    - `B1Mud_DigInMedium`: 4 s full throttle from rest in medium, ≤ 15 m. dist 13.6, vmax 4.4 m/s (~16 km/h, wheels spinning at full depth).
    - `B1Mud_RestartMedium`: 6 s at 40 % from rest in medium, ≥ 5 m. dist 10.9.
    - `B1Mud_RecoverMedium`: 2 s full, then 6 s at 35 %, ≥ 6 m. dist 19.2.
    - `B1Mud_CrawlIntoMedium`: 14 s at 30 % from firm, ≥ 25 m. dist 25.7, crosses.
    - `B1Mud_DeepRunUp`: full throttle with a 22 m run-up into deep, ≤ 42 m (the patch end). Enters at 66 km/h and stops inside at 40.1 m, sunk to 70 cm.
    - `B1Mud_ExitDeep`: 50 % from 4 m before the deep exit, ≥ 8 m, endH ≥ −1.05. dist 10.4, endH −1.01 (back at firm height).
  - CrossMedium (50 %) vmax 6.5 m/s, faster than full throttle. Gym 12/12 and `Lvl_Offroad` 8/8 PASS headless.
  - peakOsc on mud exits is higher with the faster climb-out: ExitDeep 0.92, ExitMediumFull 0.38 (limit 0.5). Exit at 5 m/s from 35–47 cm deep is a real step bump. For the user's playtest.
  - **Follow-up (user request): four sizes S/M/L/XL.**
    - Shallow (S) 15→25 cm (patch `HardBottomDepth` 25).
    - New Heavy (L) 50 cm: `PM_B1Mud_Heavy` / `MI_B1Mud_Heavy`, values between medium and deep: Friction 0.47, SettleRate 40, SinkRate 38, RiseRate 1.5, friction curve 1→0.45, RR 0.025→0.18, SpeedDrag 0.024.
    - Gym: Heavy takes the old deep slot (Y 42); Deep (XL) and its label, spawn and rigs move to Y 55. Ground re-cut: `G_Start_Deep`, `G_End_Deep`, `G_Edge_Deep`, `G_Margin_East` shrunk to Y 63–80. Labels/spawns renumbered (4 Heavy, 5 Deep, 6 Pit) and labels show the size.
    - New rig `B1Mud_RestartHeavy` (40 % from rest, ≥ 5 m): dist 6.3, vmax 1.7 m/s. Gym 13/13, `Lvl_Offroad` 8/8 PASS headless; the moved deep rigs are unchanged.
  Per wheel: friction multiplier from the curve, and a rolling-resistance force opposing wheel velocity at the contact.
  **Done:** builds; rig: full throttle on medium mud digs in within 2–4 s. → replaced by the bog-down / restart / recover rigs above.
  **User check:** drive slowly into medium, stop, and drive on with a little throttle; floor it in medium; hit deep at speed; drive out of deep.

- [x] **S1-21** Belly drag (2 h) _(requires S1-19, S1-20)_ <!-- done: 2026-10-26 -->
  Result: `FB1VehicleSimulation::ApplyBellyDrag`, after rolling resistance.
  - Excess = average `SinkDepth` over all wheels − `BellyClearance` (new on `UB1VehicleMovementComponent`, `Mud|Config`, 45 cm). Drag = mean `BellyDragCoefficient` of the mud wheels × excess (m) × vehicle weight, at the centre of mass against ground velocity.
  - Same rules as rolling resistance: at standstill it opposes the tires' summed push; capped at push + 0.4 × stopping force, so it never reverses the jeep.
  - Clearance: a one-time `LogB1Physics` line measures the physics body's underside over firm ground: 60–64 cm. 45 cm is axle height, so only Heavy (dug to 50) and Deep reach it; weight alone (static floor 0.45 × max) never does.
  - Telemetry `belly=<cm>/<N>`; rig line gets `belly=` (max N) and `vend=` (last speed), new threshold `MaxEndSpeed`.
  - New rig `B1Mud_DeepFullThrottle` (7 s full throttle from rest in deep, stuck ≤ 5 s, vend ≤ 0.5, dist ≤ 15): headless PASS, stuck 0.85 s, vend 0.00, belly 7259 N. DeepRunUp belly 7355 N. Gym 14/14 and `Lvl_Offroad` 8/8 PASS headless.
  - Found: MCP tools didn't register in this session (ECONNREFUSED at session start). The agent talks to `127.0.0.1:8001/mcp` with plain JSON-RPC from PowerShell instead.
  Analytic drag when the average `SinkDepth` > ground clearance, growing with the excess.
  **Done:** builds; rig: full throttle in deep mud ends stuck (< 0.5 m/s after 5 s).
  **User check:** —

- [x] **S1-22** NetImGui "Mud" window + telemetry (3 h) _(requires S1-18)_ <!-- done: 2026-10-26 -->
  Result: `Debug/B1ImGuiMud` (`B1ImGui::DrawMud`, menu Debug → Mud).
  - Player vehicle: speed, belly depth and drag, live `BellyClearance`, and a per-wheel table: material, sink bar (fill = depth / the material's max), slip, friction multiplier, rolling resistance N.
  - Presets: every loaded `UB1PhysicalMaterial_Mud` gets a collapsing header with all scalars plus the curve keys (normalized depth, value). Edits apply on the next frame, because the component copies params every frame. An edited preset shows `*`; "Save to asset" (editor builds only) writes the package with `UPackage::SavePackage` and logs to `LogB1Physics`.
  - Telemetry: CVar `b1.Mud.Telemetry` writes `B1Mud <vehicle> v= belly=<cm>/<N> | w<i> mat= sink= slip= mu= rr=` to `LogB1Physics` at 2 Hz, only while a wheel touches mud or has depth. The mud fields moved out of the `B1Wheels` line.
  - Verified headless: `B1Mud_SinkMedium` logs the line with sink rising 1.7→34.6 cm. The ImGui window and the save button are untested (no NetImGui client on the agent side) → user check.
  Per-wheel depth, slip, friction, resistance, plus belly drag. Live-edit the presets' params, with a "save to asset" button. 2 Hz telemetry line to `LogB1Physics`.
  **Done:** builds; the telemetry line appears during the rig run.
  **User check:** edit `SinkRate` live while driving: the effect is immediate and saving persists it.

- [x] **S1-23** Mud rig test suite (2.5 h) _(requires S1-16, S1-21)_ <!-- done: 2026-10-26 -->
  Result: the suite maps onto 16 rigs in `Lvl_Gym_Mud`, all PASS in PIE and headless (numbers in HISTORY.md).
  - New rig metric `lastDisp` (horizontal displacement during the last input step) and threshold `MaxLastStepDisplacement`.
  - Bogs down: `B1Mud_DigInMedium` now 5–15 m in 4 s (13.6 m; firm full throttle covers ~50 m).
  - Crosses at ≤ 50 %: `B1Mud_CrossMedium` runs 10 s, ≥ 32 m (60.5 m, past the patch end at 30 m). Restart: `RestartMedium`/`RestartHeavy`, `CrawlIntoMedium`.
  - Deep stuck: `DeepFullThrottle`, `DeepRunUp`, `SinkDeepStop`.
  - Creep: new `B1Mud_CreepDeep` (3 s full throttle, then 10 s idle; lastDisp 0.008 m) and `B1Mud_CreepMedium` (10 s idle in medium; 0.002 m). Limit 0.05 m.
  - Pops: entry checks `CrawlIntoMedium` ≤ 0.5 (0.39) and `DeepRunUp` ≤ 0.8 (0.60). Exit checks `ExitMediumFull` ≤ 0.5 (0.38) and `CrossMedium` ≤ 0.65 (0.57: climbing out at 50 % from ~20 cm deep, same kind of bump as ExitDeep's 0.92; for the user's playtest).
  - PIE: rigs on the same lane collide when run together, so PIE runs go in rounds with one rig per lane (8 rounds, `bAutoRun` toggled over MCP, never saved as true).
  - full throttle, medium: bogs down (speed capped well below firm), not stuck (changed in S1-20 after the user's playtest)
  - ≤ 50 % throttle crosses a 20 m medium strip; restarts from a stop in medium at low throttle
  - deep: stuck
  - stuck + zero throttle: creep < 5 cm in 10 s (Chaos low-speed check)
  - patch-entry pop check
  **Done:** all PASS in PIE and headless; the numbers are recorded in HISTORY.md.
  **User check:** —

#### Review follow-ups (user review, 2026-10-26)

- [x] **S1-25** NetImGui: type-in values (0.5 h) _(requires S1-22)_ <!-- done: 2026-10-26 -->
  Result: `DragFloatTyped` (drag + 70 px text field, typed values clamped to the drag range) for all preset scalars and belly clearance; curve keys get an `InputFloat2`. Builds; window untested on the agent side.
  Every drag in the Mud window (preset scalars, curve keys, belly clearance) gets a text field next to it, so values can be typed exactly.
  **Done:** builds.
  **User check:** type a `SinkRate` value; it applies like a drag does.

- [x] **S1-26** Engine RPM in "Vehicle Basics" (0.5 h) <!-- done: 2026-10-26 -->
  Result: speed line now shows gear and throttle %; an RPM bar (`GetEngineRotationSpeed` / `GetEngineMaxRotationSpeed`) sits under it. Builds.
  RPM (with max), gear and throttle, next to speed, so wheelspin in mud is visible as revs without speed.
  **Done:** builds.
  **User check:** floor it in medium: RPM climbs while speed stays low.

- [x] **S1-27** Chase camera: full orbit, delayed weak recenter, further back (1 h) <!-- done: 2026-10-26 -->
  Result: `AMudDriverPawn` `Camera` properties: `CameraYawRate` 1.5 (look input multiplier), `CameraRecenterDelay` 1 s, `CameraRecenterSpeed` 0.75 (template: 1.0, no delay). First pass (2 s / 0.5) felt like no pull at all to the user; retuned, and PIE shows 120° → 44° in 1.2 s. Only non-zero look input resets the delay. Yaw is normalized, so a full orbit recenters the short way. Back arm 650→850 cm, socket Z 150→180. The old 30–45° limit was the template's recenter pulling against the input every frame. Builds; PIE capture shows the new distance. Suite rerun with S1-28.
  Look input can turn the camera all the way around the jeep, a bit faster than now. Recentering starts only after a delay without look input, and is slower. The camera sits further back. All values are `Camera` properties on the pawn.
  **Done:** builds; the rig suite still passes (camera doesn't touch physics).
  **User check:** orbit to the side of the jeep in mud, let go: the camera stays a moment, then drifts back slowly.

- [x] **S1-28** Mud patch edge ramps (2.5 h) _(requires S1-19)_ <!-- done: 2026-10-26 -->
  Result: `AB1MudPatch` `EdgeRamps` + `bRampPlusX/MinusX/PlusY/MinusY` + `EdgeRampLength` (300 cm). The sim caps sinking by a hard-ground trace under each mud wheel (ADR-012 note). Gym strips have ramps on all four edges; the 9 rigs that start in mud moved 2 m further in (X 1200/1300 → 1400/1500) so they still start at full depth.
  - Gym 16/16 and `Lvl_Offroad` 8/8 PASS headless. Exit bumps drop: ExitDeep peakOsc 0.92 → 0.54, CrossMedium 0.57 → 0.37, CrawlIntoMedium 0.39 → 0.24.
  - **DeepRunUp changed meaning:** it used to stop at 40 m because the jeep hit the vertical exit wall, not because of the mud. With the ramp it enters deep at 66 km/h, is down to ~20 km/h in ~1.5 s, and drives out the far side (dist 57, peakOsc 1.03 on the ramp). Threshold now `maxPeakOscillation` 1.2 only. Whether a run-up should carry through 20 m of deep is a tuning call for S1-24.
  - ExitMediumFull peakOsc 0.379 → 0.502 (limit 0.5 → 0.55): full-throttle entry down the ramp while digging; same level as DigInMedium (0.47).
  Real puddles climb back out at the edges instead of ending in a step. Per patch edge (+X, −X, +Y, −Y) the hard bottom can ramp up to the surface over `EdgeRampLength`; edges without a ramp stay open, so patches can be placed edge to edge. Sinking is capped by the hard ground under each wheel (an extra trace that ignores the mud surface), so it works the same when the landscape is the bottom.
  **Done:** builds; gym patches get ramps where the jeep drives out; the rig suite passes (thresholds updated where the exit bumps drop).
  **User check:** drive out of deep and heavy over a ramp: the climb-out is smoother and easier than the old step.

- [x] **S1-24** Mud playtest + tuning pass (user + agent, 1–2 h) _(requires S1-22, S1-23)_ <!-- done: 2026-10-26 -->
  Result: user signed off on the mud physics as-is; no preset changes. Review notes became S1-25..S1-28. The deep run-up carrying through 20 m (S1-28) is accepted with the current tuning.
  You playtest `Lvl_Gym_Mud` and tune live. The agent writes the tuned values into the presets and re-runs S1-23 (thresholds get adjusted if your feel disagrees). You also check `stat unit` on the laptop.
  **Done:** the suite passes with the tuned values.
  **User check:** ✅ sign-off: "mud rewards driving slowly with intent".

### Backlog (found during the sprint)

- ~~Sinking ignores wheel load.~~ Fixed in the S1-19 rework: static depth and digging both scale with relative wheel load. _(found in S1-19)_

---

## Sprint 2 — GAS + fuel + mud polish (provisional, detailed at sprint start)

Scope from ROADMAP Phase G and the rest of Phase 1: GAS bootstrap (`AB1GameMode`, `AB1PlayerController`, `AB1PlayerState`, single ASC), `UB1VehicleAttributeSet`, fuel (ADR-010), Vehicle Status window and cheats, `Vehicle.State.InMud`, `B1Unstuck`, `ShowDebug B1Mud`, Niagara spray, visual trails, Phase 1 close playtest.
