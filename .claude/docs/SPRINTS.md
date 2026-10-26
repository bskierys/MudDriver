# SPRINTS

Task format: `- [ ] **SN-XX** title (estimate)`, then **Done:** (what the agent verifies) and **User check:** (what you try by hand). Markers: 🔍 Research first · ⚠️ Note · _(requires SN-YY)_.

Detailed design for every task: [plans/2026-10-25-river-crossing.md](plans/2026-10-25-river-crossing.md) (Approach, Testing; Sprints 1–2) and [plans/2026-10-26-mud-winch-first.md](plans/2026-10-26-mud-winch-first.md) (slope rules, winch, on foot; Sprints 3–4).

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

## Sprint 2 — GAS + fuel + mud polish

**Status:** active, planned 2026-10-26.

**Goal.** Close Phase G and Phase 1. A single ASC on the PlayerState carries Condition and Fuel; fighting mud burns fuel; mud gets its state tag, debug, spray and trails.

**Sprint-wide notes**
- ⚠️ Rigs that spawn their own jeep have no controller, so no PlayerState and no ASC. Fuel and tag rigs need the jeep possessed by the player controller (see S2-09). Mud rigs that don't touch GAS stay as they are.
- ⚠️ Every task that changes the controller, game mode or jeep re-runs both rig suites (`Lvl_Gym_Mud` 16, `Lvl_Offroad` 8) headless. GAS must not change handling.
- ⚠️ Prototype first (CLAUDE.md §4): spray and trails take the simple path. No particle budgets or LODs; `stat unit` is only a sanity check at the playtest.

### GAS bootstrap (ADR-004, 005)

- [x] **S2-01** GAS modules + `B1GameplayTags` (1 h) <!-- done: 2026-10-26 -->
  Result: modules public in `MudDriver.Build.cs`, include path `MudDriver/GAS`. Tags in namespace `B1GameplayTags` (`State_Avatar_InVehicle`, …) via `UE_DEFINE_GAMEPLAY_TAG_COMMENT`. Full build passes. The tag-picker check moves to S2-05 (first GE BP).
  Link `GameplayAbilities`, `GameplayTags`, `GameplayTasks` in `MudDriver.Build.cs`. Native tags (`UE_DECLARE/DEFINE_GAMEPLAY_TAG`) in `GAS/B1GameplayTags.h/.cpp`: `State.Avatar.InVehicle`, `State.Avatar.OnFoot`, `Vehicle.State.InMud`, `Vehicle.State.OutOfFuel`, `Data.Fuel`, `Data.Damage`. Water/winch tags wait for their phases.
  **Done:** full build passes; the tags show in the tag picker on any GE BP.
  **User check:** —

- [x] **S2-02** `AB1PlayerState` + ASC + `UB1VehicleAttributeSet` (2.5 h) _(requires S2-01)_ <!-- done: 2026-10-26 -->
  Result: `Player/B1PlayerState` (ASC + set as default subobjects, Mixed mode; the ASC finds the set by outer in `InitializeComponent`). `GAS/B1AbilitySystemComponent` (empty), `GAS/B1VehicleAttributeSet` with defaults in its constructor (100/100/60/60). Damage goes through a meta attribute `Damage` (Lyra style, but visible to modifiers since we use a SetByCaller modifier, not an execution) that `PostGameplayEffectExecute` subtracts from Condition; `Data.Damage` SetByCaller targets it in S2-05. No attribute replication (single-player). Builds.
  `AB1PlayerState` owns `UB1AbilitySystemComponent` (thin subclass, Lyra style) and the attribute set: `Condition`, `MaxCondition`, `Fuel`, `MaxFuel` with `ATTRIBUTE_ACCESSORS`. Clamp in `PreAttributeChange` and `PostGameplayEffectExecute` (`Data.Damage` → Condition, `Data.Fuel` → Fuel, both clamped to [0, Max]). Defaults via an init GE or the set's constructor: 100 / 100 / 60 L / 60 L.
  🔍 Research first: Lyra's `ALyraPlayerState` ASC setup and `NetUpdateFrequency` defaults; how `InitAbilityActorInfo` behaves single-player when the avatar is a Chaos pawn.
  **Done:** builds.
  **User check:** —

- [x] **S2-03** `AB1GameMode` + `AB1PlayerController` (2.5 h) _(requires S2-02)_ <!-- done: 2026-10-26 -->
  Result:
  - C++ `Player/B1GameMode` (abstract, `PlayerStateClass` = `AB1PlayerState`) and `Player/B1PlayerController` (abstract; `OnPossess` → `InitAbilityActorInfo(PS, pawn)`, `State.Avatar.InVehicle` loose tag count 1 for `AWheeledVehiclePawn`, else 0).
  - Template `AMudDriverPlayerController::OnPossess` uses `Cast` (was `CastChecked`) and binds respawn only for vehicles (`AddUniqueDynamic`). `Tick` collapses the vehicle UI when not driving and restores it (`SelfHitTestInvisible`) only if it was collapsed.
  - Assets in `/Game/B1/Player/`: `BP_B1PlayerController` (duplicate of `BP_OffRoadPlayerController`, reparented; IMCs/UI unchanged; `VehiclePawnClass` → `BP_B1Jeep`, the template respawned the template car) and `BP_B1GameMode` (duplicate of `BP_OffRoadGameMode`, reparented, controller → `BP_B1PlayerController`). `BP_B1GymGameMode` reparented onto `BP_B1GameMode`. `Lvl_Offroad` WorldSettings `DefaultGameMode` → `BP_B1GameMode`.
  - PIE `Lvl_Offroad`: inspector shows Condition 100, Fuel 60, MaxCondition 100, MaxFuel 60, Damage 0 on `B1PlayerState_0`; tags `State.Avatar.InVehicle`. Headless: gym 16/16, `Lvl_Offroad` 8/8 PASS, numbers as in Sprint 1.
  - The `LoadErrors` line for external actor `ZNZM1Q9PKXHV07GMT8Q6NN` in `Lvl_Offroad` predates this (in every log since 2026-10-25) → backlog.
  - `AB1GameMode : AOffroadGameMode` (keeps the `Player0` start pick) sets `PlayerStateClass` and `PlayerControllerClass`. `BP_B1GymGameMode` and the `Lvl_Offroad` game mode get reparented to it via MCP.
  - `AB1PlayerController : AMudDriverPlayerController`. `OnPossess` works for any pawn: the vehicle cast may be null, the vehicle UI shows only while driving, respawn binds only to vehicles. It calls `InitAbilityActorInfo(PlayerState, NewPawn)` and sets `State.Avatar.InVehicle` (loose tag) for vehicles.
  - Recommendation: subclass the template controller instead of rewriting it; the template keeps its input/UI code, we fix only the vehicle-only assumptions in the base `OnPossess` (null-safe cast).
  **Done:** builds; both rig suites PASS headless; in PIE, the GAS inspector toolset shows the ASC with `UB1VehicleAttributeSet` on the PlayerState.
  **User check:** drive `Lvl_Gym_Mud` and `Lvl_Offroad`: everything works as before (UI, respawn, flip reset, cheats).

- [x] **S2-04** Jeep ↔ ASC link (1 h) _(requires S2-03)_ <!-- done: 2026-10-26 -->
  Result: `AB1Jeep : IAbilitySystemInterface`, `TWeakObjectPtr` to the driver's PlayerState ASC set in `PossessedBy` (via `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState)`), never cleared. `UnPossessed` zeroes `State.Avatar.InVehicle`. Headless log: `BP_B1Jeep_C_0 ASC link: AbilitySystemComponent` (the PS subobject). Gym 16/16, `Lvl_Offroad` 8/8 PASS.
  `AB1Jeep` implements `IAbilitySystemInterface` through a `TWeakObjectPtr` to the PlayerState ASC, set in `PossessedBy` and kept after unpossess (ADR-004). `UnPossessed` clears the `State.Avatar.InVehicle` tag; vehicle state tags stay owned by the jeep.
  **Done:** builds; `UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Jeep)` returns the PlayerState ASC in PIE (logged once to `LogB1Vehicle`).
  **User check:** —

- [x] **S2-05** Damage/fuel GEs + cheats (1.5 h) _(requires S2-04)_ <!-- done: 2026-10-26 -->
  Result:
  - MCP can author GEs: `BlueprintTools.create(asset_type=GameplayEffect)` + `set_properties` on the CDO `Modifiers` (JSON: `Attribute{AttributeName, Attribute:"/Script/MudDriver.B1VehicleAttributeSet:Damage", AttributeOwner}`, `ModifierOp:"AddBase"`, `ModifierMagnitude{MagnitudeCalculationType:"SetByCaller", SetByCallerMagnitude{DataTag{TagName}}}`). The `Data.*` native tags resolved, which covers the S2-01 tag check.
  - `/Game/B1/GAS/GE_B1Damage_Impact` (Damage += SetByCaller `Data.Damage`), `GE_B1FuelDrain` and `GE_B1FuelAdd` (Fuel += SetByCaller `Data.Fuel`, signed litres; drain passes a negative value). All instant.
  - `UB1GASSettings` (`UDeveloperSettings`, Project Settings → B1 GAS, `DefaultGame.ini`) holds the three GE classes as soft refs; new module dep `DeveloperSettings`. `UB1AbilitySystemComponent::ApplySetByCallerEffectToSelf`.
  - Cheats on `UB1CheatManager`: `B1DamageVehicle`, `B1SetCondition` (damage = current − value, negative repairs), `B1AddFuel` (negative drains). Each logs before → after to `LogB1Vehicle`.
  - Headless `-game` with ExecCmds: Condition 100 → 75 → 90 (set) → 0 (damage 500, clamped); Fuel 60 → 10 → 60 (add 100, clamped to MaxFuel).
  `GE_B1Damage_Impact` (instant, SetByCaller `Data.Damage`), `GE_B1FuelDrain` / `GE_B1FuelAdd` (instant, SetByCaller `Data.Fuel`) in `/Game/B1/GAS/`. Cheats `B1DamageVehicle <amount>`, `B1SetCondition <value>`, `B1AddFuel <litres>` (negative drains).
  🔍 Research first: whether MCP can create a `UGameplayEffect` BP and set its modifiers array. Fallback: native GE subclasses (`UB1GE_Damage`, …) configured in the constructor.
  **Done:** builds; headless `-ExecCmds="EnableCheats,B1DamageVehicle 25"` logs Condition 100 → 75; the S2-01 tags show in the GE's tag picker.
  **User check:** `B1DamageVehicle 30`, `B1SetCondition 100`, `B1AddFuel -50`: the values change as expected (S2-06 window or `showdebug abilitysystem`).

- [x] **S2-06** NetImGui "Vehicle Status" + telemetry (2 h) _(requires S2-05)_ <!-- done: 2026-10-26 -->
  Result: `Debug/B1ImGuiVehicleStatus` (Debug → Vehicle Status): avatar name, Condition/Fuel bars, buttons Refill / +10 L / −10 L / Empty / Damage 10 / Repair, owned-tag list. Reads the first player's `AB1PlayerState`, so it works on foot too. Damage/fuel logic moved to `UB1AbilitySystemComponent::ApplyVehicleDamage/SetVehicleCondition/AddVehicleFuel`, shared by cheats and buttons. CVar `b1.Vehicle.StatusTelemetry` (default on): `B1Status <vehicle> cond= fuel= tags=` to `LogB1Vehicle` at 2 Hz, from the pawn telemetry timer, only for vehicles with an ASC (rig-spawned jeeps skip it). Headless: after `B1DamageVehicle 30`, `B1AddFuel -45` → `B1Status BP_B1Jeep_C_0 cond=70.0/100 fuel=15.00/60 tags=State.Avatar.InVehicle`. Gym 16/16, `Lvl_Offroad` 8/8 PASS. Window untested on the agent side (no NetImGui client).
  New window `Debug/B1ImGuiVehicleStatus` (Debug → Vehicle Status): Condition and Fuel bars, active tags on the ASC, buttons for add fuel / refill / damage 10 / repair. CVar `b1.Vehicle.StatusTelemetry`: `B1Status cond= fuel= tags=` to `LogB1Vehicle` at 2 Hz. Fuel rate fields join in S2-07.
  **Done:** builds; the telemetry line appears in a headless run with the CVar on.
  **User check:** the window reads well while driving, and the buttons work.

### Fuel (ADR-010)

- [x] **S2-07** `UB1VehicleTuningData` + fuel consumption (3 h) _(requires S2-05)_ <!-- done: 2026-10-26 -->
  Result: `Vehicle/B1VehicleTuningData` (`IdleRate` 6 L/h, `WorkRate` 240 L/h, `DrainInterval` 0.25 s; placeholder values for S2-15), asset `/Game/B1/Vehicles/DA_B1JeepTuning` on `BP_B1Jeep`. Research: 5.8 exposes no engine torque on the game thread; Chaos engine torque = throttle² × curve(RPM), so `UB1VehicleMovementComponent::GetNormalizedEnginePower` rebuilds it from the same throttle (rate control function) and `GetEngineRotationSpeed`, over the curve's peak torque×RPM. `AB1Jeep::Tick` accumulates and applies one drain GE per interval, only while possessed. Status window: power %, L/h, rolling 5 s L/100 m, editable rates + Save to asset (save code moved to `Debug/B1ImGuiAssetSave`, shared with the Mud window). `B1Status` gains `lph= l100= pow=`. Headless: 6.0 L/h at standstill, 20–50 L/h at 50 % throttle (pow 0.06–0.19).
  - Data asset `UB1VehicleTuningData` (`Fuel|Config`): `IdleRate`, `WorkRate` (L/h, readable for tuning), `DrainInterval` (0.25 s). Asset `/Game/B1/Vehicles/DA_B1JeepTuning`, referenced by `AB1Jeep`.
  - Jeep ticks: consumption += (`IdleRate` + `WorkRate`·`NormalizedEnginePower`)·dt, applies `GE_B1FuelDrain` every `DrainInterval`. `NormalizedEnginePower` = engine torque × ω / max power (from the torque curve), clamped [0, 1].
  - Vehicle Status window: live L/h, L/100 m (rolling), and editable rates with "save to asset" (same pattern as the Mud window). `B1Status` gets `lph=`.
  🔍 Research first: what the 5.8 Chaos engine exposes on the game thread for current torque (`FEngineSimModule`, `GetEngineTorque`?) vs throttle × torque curve(RPM) as a fallback. Engine off while the player is out is a Phase 4 concern: no consumption without a controller is enough for now.
  **Done:** builds; telemetry shows idle consumption at standstill and higher consumption at full throttle.
  **User check:** —

- [x] **S2-08** Out of fuel → no torque (1.5 h) _(requires S2-07)_ <!-- done: 2026-10-26 -->
  Result: Zeroing `ThrottleInput` in `UB1VehicleMovementComponent::Update` (before `Super`) picked: engine torque is throttle-scaled, so there's no idle creep to remove. `AB1Jeep` listens to the Fuel attribute change delegate (bound in `PossessedBy`, checked once there, removed in `UnPossessed`): at 0 sets the movement's fuel cut-off and the loose `Vehicle.State.OutOfFuel`, above 0 clears both; logs `out of fuel, throttle cut` / `refuelled, throttle back`. No consumption while cut off. Reverse is blocked too (Chaos drives reverse through `ThrottleInput`).
  At Fuel = 0 the jeep adds `Vehicle.State.OutOfFuel` and the movement component forces the throttle to 0 (brake, steering and handbrake still work). Refuelling above 0 removes the tag. Not a fail state.
  🔍 Research first: zeroing throttle in `UB1VehicleMovementComponent::Update` vs scaling engine max torque; pick the one that also stops the idle creep from the torque curve.
  **Done:** builds; covered by the S2-09 rig.
  **User check:** `B1AddFuel -100`, then floor it: the jeep doesn't move; `B1AddFuel 20` and it drives again.

- [x] **S2-09** Fuel rigs (2.5 h) _(requires S2-08)_ <!-- done: 2026-10-26 -->
  Result: rig options `bPossessWithPlayer` and `StartFuel` (negative = full); the rig possesses its jeep with player 0 and, in `FinishTest`, unbinds the template respawn and re-possesses the previous pawn. Thresholds `MinFuelPer100m`/`MaxFuelPer100m`; line gains `fuelUsed= fuelPer100m= cond=`. Rigs: `B1Fuel_Firm` (X −45 m, Y 68 m, clear of the parked player jeep; 11 s at 50 % → 137 m, 0.085 L/100 m), `B1Fuel_Medium` (X 7 m on the medium lane, 4.5 s at 50 %, stays in mud → 15.8 m, 0.158 L/100 m, guard `MinFuelPer100m` 0.15), `B1Fuel_Empty` (0 L, 5 s full throttle → 0.08 m). **Medium is 1.86× firm, short of the 2× target:** at 50 % the jeep doesn't slip in medium, and engine power ignores load, so mud only costs more per metre by being slower. Raising `IdleRate` to ~10 L/h would reach 2×; left for S2-15. Gym 19/19 (one run had `B1Mud_RestartHeavy` failed by an MCP `server/discover` error log from a client reconnecting to the headless editor; its line was PASS and it passes alone), `Lvl_Offroad` 8/8, existing numbers unchanged.
  - Rig option `bPossessWithPlayer`: the rig's spawned jeep is possessed by player 0, so it gets the ASC. Fuel is refilled at script start.
  - New metrics `fuelUsed=` (L), `fuelPer100m=` and thresholds; `B1Test` line also prints `cond=`.
  - Rigs in `Lvl_Gym_Mud`: `B1Fuel_Firm` (50 % on firm, 100 m), `B1Fuel_Medium` (50 % across medium), `B1Fuel_Empty` (fuel 0, 5 s full throttle, disp < 1 m).
  - Pass: medium L/100 m ≥ 2× firm (checked by a summary line from the runner, or by a firm-baseline threshold on the medium rig).
  ⚠️ Note: PIE has one player 0, so rigs with `bPossessWithPlayer` run one per round.
  **Done:** all three PASS headless; both suites still PASS.
  **User check:** —

- [x] **S2-17** Mud strain fuel term + typed ImGui fields (added mid-sprint after the S2-07..09 playtest) <!-- done: 2026-10-26 -->
  Why: user playtest showed fast firm driving burned more than full throttle in mud (engine power ignores load), and the fuel drags had no typed input.
  Result: ADR-010 amended. Consumption = `IdleRate + WorkRate·power + MudWorkRate·MudStrain`, `MudStrain = throttle × clamp(avg sink / MudFullStrainDepth)`; `MudWorkRate` 300 L/h, `MudFullStrainDepth` 50 cm on `UB1VehicleTuningData`, editable in Vehicle Status (which also shows strain and average sink). `B1Status` gains `strain=`. `B1ImGui::DragFloatTyped` moved to `Debug/B1ImGuiWidgets` and used by both windows; rule `.claude/rules/imgui.md` makes it mandatory for tunables. Rig metric `fuelPerHour=` + `MinFuelPerHour`. New rigs `B1Fuel_FirmFull` (6 s full throttle, 74 L/h) and `B1Fuel_DeepFull` (7 s full throttle stuck in deep, 344 L/h, guard ≥ 150). `B1Fuel_Medium` now 0.37 L/100 m = 4.3× firm (guard back to the 2× target, 0.17). Gym 21/21, `Lvl_Offroad` 8/8.
  **User check:** full throttle in medium/deep burns visibly more (Vehicle Status L/h) than fast driving on firm; type values into the fuel fields.

### Mud polish (Phase 1)

- [x] **S2-10** `Vehicle.State.InMud` tag (1 h) _(requires S2-04)_ <!-- done: 2026-10-26 -->
  Result: `AB1Jeep::UpdateInMud` in `Tick` (only while possessed with an ASC): any wheel `bInContact` on a `UB1PhysicalMaterial_Mud` sets the loose tag, released 0.5 s after the last contact; `UnPossessed` clears it. `IsInMud()` getter. Rig: `inMud=` (s tagged) in the fuel metrics, threshold `MinInMudTime`, and an error log if the tag is still on the ASC after the rig re-possesses the old pawn. Headless: `B1Fuel_Medium` inMud=3.37 (guard `MinInMudTime` 2), `B1Fuel_DeepFull` 7.00, firm rigs 0, no leftover-tag error.
  The jeep adds the loose tag while any wheel touches mud and removes it 0.5 s after the last mud contact (no flicker on bumps). Removed on unpossess (ADR-004). Shows in the Vehicle Status tag list.
  **Done:** builds; a `bPossessWithPlayer` rig crossing medium logs `inMud=` (seconds tagged) > 0 and the tag is gone at the end.
  **User check:** —

- [ ] **S2-11** `B1Unstuck` cheat (1.5 h)
  Lifts the jeep 50 cm above the mud surface under it, zeroes linear/angular velocity, keeps yaw, and resets every wheel's `SinkDepth`/`SinkOffset` (new one-shot flag in `FB1VehicleSimInput`). Logs to `LogB1Physics`.
  **Done:** builds; headless run with `B1Unstuck` after `B1Mud_DeepFullThrottle`'s setup logs sink 0 on all wheels.
  **User check:** get stuck in deep, `B1Unstuck`: the jeep pops out level and can drive (it'll sink again from weight, that's expected).

- [ ] **S2-12** `ShowDebug B1Mud` (1 h)
  Text over the jeep: per wheel material, sink cm / max, mu, rr N; belly cm / N; `InMud` tag. Over each `AB1MudPatch` within 50 m: preset name, max depth, ramps. Same binding pattern as `B1VehicleShowDebug`.
  **Done:** builds.
  **User check:** `ShowDebug B1Mud` in the gym: readable over the jeep and the patches, toggles off.

- [ ] **S2-13** Niagara mud spray (3 h)
  `NS_B1MudSpray`: GPU sprites, spawn rate ∝ slip speed × (sink depth > 0), velocity backward and up from the contact along the wheel spin direction, gravity, depth-buffer collision, short lifetime. One `UNiagaraComponent` per wheel on `AB1Jeep`, driven from `Tick` via user params (`SpawnRate`, `SpinDir`, `Color`). Mud wheels only; off on firm ground. Colour from a new `SprayColor` on `UB1PhysicalMaterial_Mud` (cheap stretch item).
  🔍 Research first: what the MCP Niagara toolset can author (emitter modules, user params). If it can't build the system, the agent writes the C++ side and you make the system by hand from a short spec.
  **Done:** builds; Simulate + `CaptureEditorImage` with a rig digging in medium shows spray on the mud wheels only.
  **User check:** floor it in medium: the spray reads as mud and scales with wheelspin; crawling barely sprays.

- [ ] **S2-14** Visual trails (ADR-008) (3.5 h)
  - Each `AB1MudPatch` owns a render target (512², world-mapped over `Size`) created at BeginPlay. `M_B1Mud` samples it: darker and slightly glossier where driven.
  - The jeep stamps a soft brush at each mud wheel's contact every ~10 cm travelled (`DrawMaterialToRenderTarget` with a brush material, or a canvas draw batch per frame), width from the wheel.
  - Trails don't fade and don't touch physics.
  🔍 Research first: per-patch render targets vs one RVT for all mud. Per-patch RT is the simple default; RVT only if per-patch has a blocker.
  **Done:** builds; a viewport capture after a rig drive through medium shows two clear tracks.
  **User check:** trails look acceptable; decide whether physics ruts are worth it (ADR-008 revisit → backlog or a new ADR).

### Close

- [ ] **S2-15** Fuel playtest + tuning (user + agent, 1 h) _(requires S2-06, S2-09)_
  You drive the gym and tune the rates in Vehicle Status. The agent writes the values into `DA_B1JeepTuning` and re-runs S2-09.
  **Done:** fuel rigs PASS with the tuned values.
  **User check:** sign-off on fuel pressure (Phase G).

- [ ] **S2-16** Phase 1 close playtest (user + agent, 1 h) _(requires S2-10..S2-14)_
  Mud checklist (ROADMAP Phase 1) plus spray, trails, `ShowDebug B1Mud`, `B1Unstuck`. You also check `stat unit` on the laptop (sanity only).
  **Done:** both suites PASS; ROADMAP Phase 1 and Phase G ticked.
  **User check:** sign-off on Phase 1.

### Backlog (found during the sprint)

- `Lvl_Offroad` load logs `LoadErrors: ... /Lvl_Offroad/B/9M/ZNZM1Q9PKXHV07GMT8Q6NN, a dependent package None was not available`. Present since at least 2026-10-25; rigs pass. Find the actor and fix or delete its broken reference. _(found in S2-03)_

---

## Sprint 3 — Mud + winch core (Phase 2, part 1)

**Status:** planned 2026-10-26, starts after Sprint 2 closes.

**Goal.** Prove mud + winch before any on-foot work. Mud learns slopes (slide when barely sunk, hold when deep, never climb unwinched), the winch pulls the jeep out of deep mud and up the muddy slope, and the user feels it at a checkpoint with the hook attached by cheat.

**Sprint-wide notes**
- ⚠️ The slope rules must not change flat ground. Every task touching `FB1VehicleSimulation` re-runs `Lvl_Gym_Mud` (21) and `Lvl_Offroad` (8) headless; numbers stay within 5 % of Sprint 2.
- ⚠️ `ActorTools.set_actor_transform` resets omitted fields: always pass location, rotation and scale when placing slopes and rotated patches.
- ⚠️ `Lvl_Gym_Winch` is WP/OFPA like `Lvl_Gym_Mud`: new actors need `save_assets([])` to land on disk; check `git status`.
- ⚠️ The agent can't drive or run cheats through MCP. Winch behaviour is verified by rigs with `WinchAnchor` / `WinchPull`; cheats, ImGui and ShowDebug are user checks.

### Slopes (ADR-001 amendment)

- [ ] **S3-01** `Lvl_Gym_Winch` base + test slope (3 h)
  Duplicate `Lvl_Gym_Mud`'s setup (floor tiles, hidden walls, sky, `BP_B1GymGameMode`), strip the lanes. Add one test slope pair at 20°: a firm slope (rotated cube tiles) and a medium-mud slope (rotated `BP_B1MudPatch` with `PM_B1Mud_Medium`, own hard bottom, ramp only at the foot), each ~30 m long with a 20 m flat run-up and a 10 m flat top. Spawns at each run-up.
  ⚠️ Note: `AB1MudPatch` builds with the actor rotation (`B1MudPatch.cpp:88`), but the hard-ground cap trace is vertical and `MaxSinkDepth` long. On a slope the bottom is `depth / cos θ` away, so lengthen the trace if needed.
  **Done:** builds; viewport capture of the slopes; PIE spawns the jeep at the run-up; a rig dropped on the mud slope shows `sink=` > 0 (mud is read on the rotated patch).
  **User check:** drive both slopes once as they are: describe what the jeep does on the mud slope today.

- [ ] **S3-02** Rig `maxH` + slope baseline (2 h) _(requires S3-01)_
  New rig metric `maxH` (peak height above the rig) and threshold `MaxPeakHeight`. Baseline rigs on the test slopes, measured **before** any rule change: firm 50 %, medium 30/50/100 % (15 s from the run-up), medium idle from mid-slope (10 s). Record numbers in HISTORY.md.
  **Done:** builds; baseline numbers recorded; existing suites still pass.
  **User check:** —

- [ ] **S3-03** Net-force standstill rule (2.5 h) _(requires S3-02)_
  Rolling resistance (`ApplyWheelFrictionForces`) and belly drag (`ApplyBellyDrag`) judge "push" from the net force along the ground: tire force + gravity along the ground (+ winch later, S3-10), not `GetForceFromFriction().X` alone. On flat ground gravity along the ground is 0, so nothing changes there.
  ⚠️ Note: at rest on a slope the tire force is mostly the tire holding against gravity; the old rule applied resistance downhill. Gravity along the ground per wheel = weight share × (gravity · ground forward); for belly drag use the whole-body component on the ground plane.
  **Done:** builds; both flat suites within 5 % of Sprint 2; the medium idle rig no longer accelerates downhill from the resistance itself (compare with the S3-02 baseline).
  **User check:** —

- [ ] **S3-04** Slope hold by sink depth (3 h) _(requires S3-03)_
  `SlopeHoldVsSinkDepth` (`FRuntimeFloatCurve` over absolute cm, fraction of wheel load) on `UB1VehicleMovementComponent` (`Mud|Config`), copied to the sim. Per mud wheel: a force that cancels the net downhill force along the ground up to `curve(SinkDepth) × wheel load`, never more, so it can't push the jeep uphill. Start curve: 0 at 0–15 cm, 1 at 30 cm. Mud window: editable curve keys (typed fields per `imgui.md`) and a per-wheel `hold` column; `B1Mud` telemetry gets `hold=`.
  **Done:** builds; flat suites unchanged (the term is 0 on flat ground); medium idle on the test slope slides; a jeep dug in to ≥ 30 cm on the slope holds.
  **User check:** on the medium slope, stop and let go: the jeep slides back slowly. Floor it until it digs in, let go: it stays. Edit the curve live and feel the change.

- [ ] **S3-05** Slope angle + final slope lanes (2.5 h) _(requires S3-04)_
  Pick θ: firm climbs at 50 %, medium fails at 30/50/100 %. If no θ in 15–30° does both, stop and raise a per-material slope grip factor (ADR-001 fallback) with the user. Build three lanes at θ: firm, medium, deep (`PM_B1Mud_Deep`), each with run-up and top plateau, and an anchor marker 8 m past each mud top. Record θ, lengths and anchor spots in ARCHITECTURE.md (Winch gym).
  **Done:** viewport capture; θ and layout in ARCHITECTURE.md.
  **User check:** the slopes read clearly; θ looks like a hill you'd expect a jeep to climb on dry ground.

- [ ] **S3-06** Slope rig suite, unwinched (2.5 h) _(requires S3-05)_
  - `B1Slope_FirmClimb`: 50 % from the run-up, reaches the top (`endH` ≥ top − 0.5 m).
  - `B1Slope_Medium30/50/100`: 15 s from the run-up, `maxH` ≤ top − 1 m.
  - `B1Slope_MediumIdle`: mid-slope, zero throttle 10 s, slides ≥ 1 m, `vmax` ≤ 1.5 m/s.
  - `B1Slope_DeepIdle`: mid-slope, zero throttle 10 s, slides ≥ 0.1 m, `lastDisp` ≤ 5 cm over the last 5 s.
  - `B1Slope_MediumDugIn`: 3 s full, then 10 s at 50 %, gains < 0.5 m height.
  ⚠️ Note: rigs on the same lane collide in PIE; run in rounds, one per lane.
  **Done:** all PASS headless; flat suites PASS.
  **User check:** drive the three slopes: firm is easy, medium is impossible however careful you are, deep slides then sticks.

### Winch core (ADR-007 amendment)

- [ ] **S3-07** Winch + anchor components, tree, cable visual (3 h) _(requires S3-01)_
  `UB1WinchComponent` (scene component on `AB1Jeep`, at a `WinchPoint` on the front bumper): attached anchor, rest length, `MaxCableLength` (40 m), `MaxPullForce` (40 kN), `ReelInSpeed` (1 m/s), stiffness, damping; category `Winch|Config`. `UB1WinchAnchorComponent` (scene component). `BP_B1WinchTree`: a placeholder trunk mesh + anchor. Cable drawn with `UCableComponent` from the winch point to the anchor while attached. Cheats `B1WinchAttachNearest` (nearest anchor within `MaxCableLength`, rest length = current distance), `B1WinchDetach`. No force yet.
  🔍 Research first: `CableComponent` in 5.8 (enable the plugin in `MudDriver.uproject`, module dep, attaching the end to a moving component).
  ⚠️ Note: new plugin + new classes = full build with the editor closed (`Scripts/BuildEditor.ps1`).
  **Done:** builds; Simulate + capture shows the cable from the jeep to a tree after an attach (set via MCP on the component).
  **User check:** `B1WinchAttachNearest` near a tree: the cable appears; `B1WinchDetach` removes it.

- [ ] **S3-08** Winch tension in the simulation (3 h) _(requires S3-07)_
  Game thread sends anchor world position, rest length, pull and params through `FB1VehicleSimInput`. `FB1VehicleSimulation` applies a one-sided spring-damper, capped at `MaxPullForce`, with `AddForceAtPosition` at the winch point (its own path; `EB1TestForce` stays). Rest length shrinks at `ReelInSpeed × pull` while taut. The body stays awake while attached (same rule as the test force). Output: tension N, current length. Loose `Vehicle.State.Winching` while pull > 0 (new native tag).
  ⚠️ Note: the winch point moves with the body; compute it on the physics thread from the body transform, not from a game-thread position a frame old.
  **Done:** builds; a rig with a fixed anchor and full pull on firm ground drags a braked jeep toward the anchor; tension never exceeds the cap.
  **User check:** —

- [ ] **S3-09** Winch pull input (1.5 h) _(requires S3-08)_
  `IA_B1WinchPull` (analog, axis1D: trigger / key) and `IA_B1WinchRelease` (detach) in the vehicle mapping context; the jeep binds them natively (like throttle), pull goes to `UB1WinchComponent`.
  🔍 Research first: whether `DataAssetTools.create` makes `UInputAction` assets (it's a `UDataAsset` subclass) and how to add a mapping to the template IMC through MCP. Fallback: you add the two mappings by hand from a spec.
  **Done:** builds; the IMC asset lists both mappings.
  **User check:** attach by cheat, hold the pull key/trigger: the cable tightens and pulls; analog trigger pulls proportionally.

- [ ] **S3-10** Winch counts as push in the drag caps (1.5 h) _(requires S3-03, S3-08)_
  Add the winch force (projected on the ground plane / wheel forward) to the net push in the rolling-resistance and belly-drag caps, so a belly-stuck jeep can be pulled out.
  **Done:** builds; a rig stuck in deep (dig-in script) moves under full pull at zero throttle, and the drag force is visible in telemetry.
  **User check:** —

### Debug + rigs

- [ ] **S3-11** Rig winch support + metrics (2.5 h) _(requires S3-08)_
  `AB1GymTestRig`: optional `WinchAnchor` actor (attached at script start, rest length = distance), `WinchPull` per input step. Metrics `tensionMax`, `tensionOsc` (tension amplitude over the last step), `anchorDistMax` and thresholds.
  **Done:** builds; the S3-08 rig uses the new fields and prints the metrics.
  **User check:** —

- [ ] **S3-12** NetImGui "Winch" window + telemetry (2.5 h) _(requires S3-08)_
  Debug → Winch: attached anchor, length / max, tension bar vs cap, pull %, `Winching` tag, per-wheel net push vs drag; live-edit of the winch params (typed fields, save to asset per `imgui.md`); Attach nearest / Detach buttons. CVar `b1.Winch.Telemetry`: `B1Winch <vehicle> anchor= len= rest= tension= pull=` to `LogB1Physics` at 2 Hz while attached.
  **Done:** builds; the telemetry line appears in a headless winch rig run.
  **User check:** the window reads well while winching; tune `MaxPullForce` live.

- [ ] **S3-13** `ShowDebug B1Winch` (1 h) _(requires S3-08)_
  Over the jeep: tension / cap, length / max, pull. Over each anchor within 50 m: name, distance, in range or not. Same binding pattern as `B1VehicleShowDebug`.
  **Done:** builds.
  **User check:** `ShowDebug B1Winch` readable, toggles off.

- [ ] **S3-14** Deep strip lane + anchors (1.5 h) _(requires S3-05, S3-07)_
  Deep (XL) patch with ramps in `Lvl_Gym_Winch`, `BP_B1WinchTree` ~10 m past the far ramp; trees at the S3-05 anchor spots on the medium and deep slopes.
  **Done:** viewport capture; ARCHITECTURE.md lists the anchors.
  **User check:** —

- [ ] **S3-15** Winch rig suite (3 h) _(requires S3-06, S3-10, S3-11, S3-14)_
  - `B1Winch_Stability20/120`: full pull against a jeep stuck in deep at `MaxFPS` 20 and 120: `tensionMax` ≤ cap + 5 %, `tensionOsc` non-growing, `peakOsc` ≤ 0.5.
  - `B1Winch_CableLimit`: drive away from the anchor at 50 %: `anchorDistMax` ≤ `MaxCableLength` + 0.5 m.
  - `B1Winch_DeepStrip`: dig in, then full pull + 50 %: `endH` back at firm height past the far ramp in ≤ 20 s.
  - `B1Winch_SlopeMedium`: from the run-up into the slope, full pull + 50 %: top in ≤ 30 s.
  - `B1Winch_SlopeDeep`: same on the deep slope, ≤ 45 s.
  **Done:** all PASS headless; slope and flat suites PASS.
  **User check:** —

### Close

- [ ] **S3-16** Mud + winch checkpoint playtest (user + agent, 1–2 h) _(requires S3-09, S3-12, S3-15)_
  You get stuck in the deep strip and on the medium and deep slopes, attach by cheat, and winch out and up from the car. Tune winch params and the hold curve live; the agent writes them back and re-runs S3-06 and S3-15.
  **Done:** both winch and slope suites PASS with the tuned values; ADR-007 → accepted.
  **User check:** sign-off: "mud + winch works". On foot starts only after this.

---

## Sprint 4 — On foot (Phase 2, part 2)

**Status:** outlined 2026-10-26; refine with `/sprint` after the Sprint 3 checkpoint.

**Goal.** Close Phase 2: the player gets out, carries the hook to a tree, gets back in and winches out; a log can be winched off a lane.

**Sprint-wide notes**
- ⚠️ Possession changes re-run both flat suites and the slope/winch suites headless.
- ⚠️ Rigs with `bPossessWithPlayer` run one per round in PIE (single player 0).

- [ ] **S4-01** Ability set + input config (ADR-014) (3.5 h)
  `UB1AbilitySet` (abilities with input tags + effects), granted once on the PlayerState ASC (`AB1PlayerState` or `AB1GameMode` hook). `UB1InputConfig` (input action → `InputTag.*`). `AB1PlayerController` forwards pressed/released to `UB1AbilitySystemComponent`, which activates matching specs. A no-op test ability bound to a key logs activation.
  🔍 Research first: Lyra's `ULyraAbilitySet`, `ULyraInputConfig` and `ULyraAbilitySystemComponent::AbilityInputTagPressed`; the minimum we need without `ULyraInputComponent`.
  **Done:** builds; headless `-game` with a cheat that injects the input tag logs the test ability's activation; ADR-014 → accepted.
  **User check:** press the test key in PIE: the activation shows in the log.

- [ ] **S4-02** `AB1Character` + on-foot input (3 h) _(requires S4-01)_
  First-person `ACharacter`: capsule, camera, walk/look/jump with Enhanced Input, `IMC_B1OnFoot`. `BP_B1Character` in `/Game/B1/Player/`. Implements `IAbilitySystemInterface` through the PlayerState ASC.
  **Done:** builds; a character placed in `Lvl_Gym_Winch` and possessed via a debug cheat walks in PIE (user check).
  **User check:** walking and looking feel fine; walking through mud stands on the hard bottom.

- [ ] **S4-03** Mapping-context swap on possession (1.5 h) _(requires S4-02)_
  `AB1PlayerController::OnPossess` removes the previous pawn's mapping contexts and adds the new one's (vehicle vs on-foot); the vehicle UI already hides on foot.
  ⚠️ Note: the template adds its contexts in `BeginPlay`; move vehicle contexts to possession without breaking `Lvl_Offroad` respawn.
  **Done:** builds; both flat suites PASS.
  **User check:** —

- [ ] **S4-04** Seat attach + engine off outside (2 h) _(requires S4-02)_
  Seat socket on `BP_B1Jeep`. While driving the character is hidden, collision off, attached to the seat. Engine off on exit: throttle cut (same path as out-of-fuel), no fuel drain, `Vehicle.State.EngineOff`; restart on enter.
  ⚠️ Note: an unpossessed jeep on a slope must still slide/hold per the slope rules; keep the sim running (don't let it sleep while on mud slope).
  **Done:** builds; a headless run shows no fuel drain while the engine is off.
  **User check:** —

- [ ] **S4-05** `GA_B1EnterExitVehicle` + cheats (3 h) _(requires S4-01, S4-03, S4-04)_
  Exit: place the character at the driver door (fallback spots if blocked), unhide, possess it, ASC avatar → character, `State.Avatar.OnFoot`. Enter: within range of the jeep, possess it, avatar → jeep. Tag-gated per ADR-004. Cheats `B1ExitVehicle`, `B1EnterVehicle`.
  **Done:** builds; headless `-game` script with 10 × exit/enter: no errors or ensures, fuel unchanged while outside, avatar tags correct after each (logged).
  **User check:** get out and in on firm ground, in mud and on the slope; camera and controls switch cleanly.

- [ ] **S4-06** `GA_B1CarryWinchHook` (3.5 h) _(requires S3-07, S4-05)_
  On foot at the jeep's front: take the hook; the cable runs from the winch point to the character's hand; the character can't walk past `MaxCableLength` (leash). Interact near an anchor attaches (rest length = distance); interact at the jeep puts the hook back. Detach from the car via `IA_B1WinchRelease`.
  **Done:** builds; a headless cheat sequence (take, teleport near the tree, attach) logs the attachment; S3-15 rigs still PASS.
  **User check:** carry the hook up the slope to the tree, attach, walk back, get in, winch up.

- [ ] **S4-07** Interaction prompt + cable from hand (1.5 h) _(requires S4-06)_
  Minimal UMG/ShowDebug-style prompt: "Take hook", "Attach", "Return hook" when in range. Cable end follows a hand socket on the first-person camera.
  **Done:** builds.
  **User check:** you always know what the interact key will do.

- [ ] **S4-08** Winchable log (3 h) _(requires S3-08, S3-14)_
  `BP_B1WinchLog`: simulating trunk mesh with `UB1WinchAnchorComponent`. The opposite cable force is applied in the log's async physics tick (`bAsyncPhysicsTickEnabled`), reading the winch's published tension and direction. Log lane in `Lvl_Gym_Winch`. Rig `B1Winch_Log`: braked jeep, full pull, log moves ≥ 3 m toward the jeep.
  🔍 Research first: `AActor::AsyncPhysicsTickActor` in 5.8 and reading game-thread data safely from it. Fallback: game-thread `AddForce` on the log only.
  **Done:** rig PASS headless.
  **User check:** winch the log off the lane; it slides and rolls believably.

- [ ] **S4-09** Phase 2 playtest (user + agent, 1–2 h) _(requires S4-05..S4-08)_
  Full loop on foot in `Lvl_Gym_Winch`: get stuck, get out, carry the hook, attach, get in, winch out; same on both slopes; log lane. All suites headless.
  **Done:** all suites PASS; ROADMAP Phase 2 ticked.
  **User check:** sign-off on Phase 2.
