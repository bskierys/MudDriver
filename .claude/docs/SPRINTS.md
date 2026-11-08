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

**Status:** ✅ closed 2026-10-27, signed off by the user after the fuel/HUD/spray playtest and the refactor checks.

**Goal.** Close Phase G and Phase 1. A single ASC on the PlayerState carries Condition and Fuel; fighting mud burns fuel; mud gets its state tag, debug, spray and trails. Mud and fuel become components that can be copied into another project (added after /validate).

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

- [x] **S2-11** `B1Unstuck` cheat (1.5 h) <!-- done: 2026-10-26 -->
  Result: `UB1VehicleMovementComponent::Unstuck()` (shared by the cheat and rigs): object trace for `B1Mud` 3 m above/below the actor, teleports level (yaw kept) so the lowest wheel bottom (wheel bone − radius) is 50 cm above the surface (no mud → +50 cm), zeroes velocities. The one-shot is a counter `SinkResetSerial` in `FB1VehicleSimInput` (input is latest-wins, so a bool could be dropped between physics steps); the sim zeroes `SinkDepth`/`SinkOffset` once per new value and echoes it in the output, then the component logs `B1Unstuck ...: sim reset applied, sink=` to `LogB1Physics`. Rig input step flag `bUnstuck`. New rig `B1Mud_UnstuckDeep` (deep lane, 4 s full → unstuck + 1 s idle → 3 s at 50 %): `sink before 68.6 cm avg`, lifted 123 cm, `sink=0.0/0.0/0.0/0.0`, then 3.4 m in the last step (was stuck). No thresholds (smoke rig). Gym 22/22, `Lvl_Offroad` 8/8.
  Fix after user playtest (jeep sank right back): unstuck assist window. For `UnstuckAssistDuration` (4 s) after `Unstuck()`, the sim adds `UnstuckAssistForce` (15 kN) × (raw throttle − raw brake) along the flattened heading, via `FB1VehicleSimInput::UnstuckAssistForce`; both on the movement component (`Mud|Unstuck`). Logs `assist over, sink= avg`. `B1Mud_UnstuckDeep` last step 3.4 → 14.2 m, sink 17 cm at the end of the assist. Gym 22/22, `Lvl_Offroad` 8/8.
  Lifts the jeep 50 cm above the mud surface under it, zeroes linear/angular velocity, keeps yaw, and resets every wheel's `SinkDepth`/`SinkOffset` (new one-shot flag in `FB1VehicleSimInput`). Logs to `LogB1Physics`.
  **Done:** builds; headless run with `B1Unstuck` after `B1Mud_DeepFullThrottle`'s setup logs sink 0 on all wheels.
  **User check:** get stuck in deep, `B1Unstuck`: the jeep pops out level and can drive (it'll sink again from weight, that's expected). ✅ signed off 2026-10-26 (with the assist window).

- [x] **S2-12** `ShowDebug B1Mud` (1 h) <!-- done: 2026-10-26 -->
  Result: `Debug/B1MudShowDebug` (`DrawVehicle`, `DrawPatch`). Jeep: `AMudDriverPawn::DrawShowDebug` made virtual, `AB1Jeep` overrides it; orange lines start at the B1Vehicle anchor and run down (both categories can be on): `InMud yes|no|n/a (no ASC)`, belly cm / N, per wheel material, sink / material max cm, mu, rr N, `(air)`. Patches: each `AB1MudPatch` binds `AHUD::OnShowDebugInfo` in BeginPlay and draws cyan text 1.5 m above its origin when the viewer's camera is within 50 m: label + material, max sink, static %, size, hard bottom depth, ramps (`+X -X …`, length). Builds; gym 22/22. A `-game` run with `ShowDebug B1Mud` showed no errors, but the camera faced the sky, so readability is unchecked on the agent side.
  Text over the jeep: per wheel material, sink cm / max, mu, rr N; belly cm / N; `InMud` tag. Over each `AB1MudPatch` within 50 m: preset name, max depth, ramps. Same binding pattern as `B1VehicleShowDebug`.
  **Done:** builds.
  **User check:** `ShowDebug B1Mud` in the gym: readable over the jeep and the patches, toggles off. ✅ signed off 2026-10-26.

- [x] **S2-13** Niagara mud spray (3 h) <!-- done: 2026-10-26 -->
  Result: research: the MCP Niagara toolset builds systems end to end (create from template, add emitter/modules, user vars, linked/HLSL stack inputs, renderer data, stack-issue fixes); MaterialTools builds the material graph. `/Game/B1/FX/NS_B1MudSpray` (Fountain emitter, GPU, fixed bounds): `SpawnRate` ← `User.SpawnRate`; Add Velocity cone axis ← `User.SprayVelocity`, speed = `length(User.SprayVelocity)`, 25°; colour ← `User.Color`; 0.4–0.9 s, 10–25 cm sprites; gravity, drag 1; GPU depth-buffer collision (restitution 0.05, friction 0.9, **Kill Occluded Particles off**, or particles born behind the tire die). `M_B1MudSpray`: masked, lit, base colour = particle colour, round SphereMask cutout. C++ `Mud/UB1MudSprayComponent` on `AB1Jeep` (Niagara as a private module dep): one Niagara component per wheel, created lazily; per tick, on a mud wheel in contact with sink > 0 and |slip| > 0.5 m/s: rate = 60/s per m/s of excess slip (max 400), velocity 250–900 cm/s backward (by drive-torque sign) and 35° up, spawned 20 cm above the visible mud surface and behind the tread (the hit is sunk, ADR-012). Uses `FWheelStatus::SlipMagnitude`, the same slip the sink model uses. CVar `b1.Mud.Spray`. `SprayColor` on `UB1PhysicalMaterial_Mud`, set per preset (lighter for Shallow, darker for Deep). `B1Mud` telemetry gains `spray=` per wheel. The spec's `SpinDir` param became `SprayVelocity` (direction plus slip-scaled speed). Simulate + `CaptureEditorImage` with `B1Mud_DigInMedium`: brown clumps thrown back off the slipping wheels; telemetry `spray=160..210` on medium, `0` on firm. Gym 22/22, `Lvl_Offroad` 8/8.
  `NS_B1MudSpray`: GPU sprites, spawn rate ∝ slip speed × (sink depth > 0), velocity backward and up from the contact along the wheel spin direction, gravity, depth-buffer collision, short lifetime. One `UNiagaraComponent` per wheel on `AB1Jeep`, driven from `Tick` via user params (`SpawnRate`, `SpinDir`, `Color`). Mud wheels only; off on firm ground. Colour from a new `SprayColor` on `UB1PhysicalMaterial_Mud` (cheap stretch item).
  🔍 Research first: what the MCP Niagara toolset can author (emitter modules, user params). If it can't build the system, the agent writes the C++ side and you make the system by hand from a short spec.
  **Done:** builds; Simulate + `CaptureEditorImage` with a rig digging in medium shows spray on the mud wheels only.
  **User check:** floor it in medium: the spray reads as mud and scales with wheelspin; crawling barely sprays. ✅ signed off 2026-10-26.

- [x] **S2-14** Visual trails (ADR-008) (3.5 h) <!-- done: 2026-10-26 -->
  Result: per-patch RT, no blocker found (RVT not needed). Each `AB1MudPatch` creates a 512² R8 render target at BeginPlay (game worlds only), puts it on a dynamic instance of its surface material as `TrailMask`, and registers with `UB1MudTrailSubsystem` (`UTickableWorldSubsystem`, Game/PIE). `M_B1Mud`: UV = surface object-space XY / 100 + 0.5 (World→Local transform; handles rotation, no patch params), mask lerps base colour to 0.45× and roughness 1 → 0.4. `UB1MudTrailComponent` on `AB1Jeep` sends each mud wheel's contact path (width = `WheelWidth` × 1.3) as segments; the subsystem splits them into stamps every 5 cm and draws one additive canvas batch per patch per frame with a procedural soft brush (64², 0.5 per stamp). No fade, no physics. CVar `b1.Mud.Trails`. Simulate + `CaptureEditorImage` after `B1Mud_CrossMedium`: two solid dark tracks along the lane. Gym 22/22, `Lvl_Offroad` 8/8.
  - Each `AB1MudPatch` owns a render target (512², world-mapped over `Size`) created at BeginPlay. `M_B1Mud` samples it: darker and slightly glossier where driven.
  - The jeep stamps a soft brush at each mud wheel's contact every ~10 cm travelled (`DrawMaterialToRenderTarget` with a brush material, or a canvas draw batch per frame), width from the wheel.
  - Trails don't fade and don't touch physics.
  🔍 Research first: per-patch render targets vs one RVT for all mud. Per-patch RT is the simple default; RVT only if per-patch has a blocker.
  **Done:** builds; a viewport capture after a rig drive through medium shows two clear tracks.
  **User check:** trails look acceptable; decide whether physics ruts are worth it (ADR-008 revisit → backlog or a new ADR). ❌ 2026-10-26: trails look flat, no depth (worst on Heavy/Deep). No physics ruts wanted, but they must look 3D → S2-14b.

- [x] **S2-14b** Deformed-looking ruts (4–6 h) _(requires S2-14)_ <!-- done: 2026-10-26 -->
  Result: `AB1MudPatch` builds `MudVisual` (`UProceduralMeshComponent`, new private module dep; grid at `VisualGridSpacing` 10 cm, capped at 400 cells per side, UV0 spans the patch, no collision, no shadow) in `OnConstruction` with `MudSurface`'s material; `MudSurface` is hidden and stays for collision. The trail target is now R16F (R8's 0.4 cm steps striped the normals) and stores rut depth / `TrailDepthRange` (100 cm). Stamps blend toward the wheel's sink depth (translucent, 0.5 per stamp; canvas has no max blend, so a shallower wheel partly fills an older rut); wheels sunk < 2 cm leave no mark. Rut width = tire width × 1.2 + depth × 1.5 (deep ruts get sloped walls, not trenches); brush flat floor, falloff 0.3–1. `M_B1Mud`: UV0, `TrailMask` texture object, Custom HLSL nodes for height (−depth + lip = saturate(4-tap blur − depth) × `LipFactor` 0.35 at `LipRadiusCm` 30), WPO = vertex normal × height, a world-space normal from the height gradient (step `TrailTexel` 1.5 texels, local cm → world), and wet darkening ×0.6 over the first 8 cm. MID params `TrailDepthRange`, `PatchSizeX/Y` set by the patch. Captures after `B1Mud_CrossMedium` + `B1Mud_DeepRunUp`: wide sloped ruts with lips on Deep, grooves with lips on Medium from a chase-height angle. Gym 22/22, `Lvl_Offroad` 8/8.
  Visual only (ADR-008 amendment). Builds on the S2-14 trail render targets.
  - Stamps write the wheel's sink depth (normalized to the patch's mud `MaxSinkDepth`), max-blended, so Deep digs deep ruts and Shallow barely marks.
  - The patch gets a dense visual surface (subdivided plane, ~10 cm grid) for vertex displacement; the 10 cm slab stays for collision and is hidden.
  - `M_B1Mud`: World Position Offset pushes vertices down by rut depth and raises a lip beside the rut (mud pushed aside); normals rebuilt from the rut height gradient, so walls shade without shadow maps (ADR-011); wet darkening kept.
  - Ruts don't recover when the sink model does.
  **Done:** builds; both suites PASS; a viewport capture after rigs drive through medium and deep shows ruts with visible depth and lips.
  **User check:** ruts on Heavy/Deep look dug into the mud from the chase camera. ✅ signed off 2026-10-26.

### Portability (added 2026-10-26 after /validate)

Why now, not backlog: mud and fuel must be copyable into another project with a different movement component and fuel attribute, and mud surfaces must later come from PCG, not just rectangles. Sprint 3 (slope hold, standstill rule, winch) edits the same sim functions, so splitting after it means untangling winch from mud.

- ⚠️ Pure refactors: behaviour must not change. Each task re-runs `Lvl_Gym_Mud` (22) and `Lvl_Offroad` (8) headless; numbers stay within 1 % (fixed step, so they should match). Record before/after in HISTORY.md.
- ⚠️ New `.cpp`/`.h` files: `BuildEditor.ps1` drops `Makefile.bin`; header changes need a full build, not Live Coding.
- ⚠️ Target: `Mud/` and `Fuel/` never include `AB1Jeep`, `UB1VehicleMovementComponent`, `UB1AbilitySystemComponent`, `UB1VehicleAttributeSet` or `B1GameplayTags`. The host vehicle reaches them through small interfaces.

- [x] **S2-18** Mud surface contract ADR (A1) (0.5 h) <!-- done: 2026-10-27 -->
  Result: ADR-015 in DECISIONS.md; ARCHITECTURE Mud section opens with the contract.
  New ADR: a mud surface is any primitive that (1) has the `B1MudSurface` profile (object `B1Mud`, blocks only `WorldDynamic`), (2) reports a `UB1PhysicalMaterial_Mud`, and (3) has something blocking `WorldDynamic` below it within `MaxSinkDepth`. Depth rule: mud sits over a depression. A surface flush on the landscape gives ~0 cm sink, because the hard-ground cap finds the landscape right under it. `AB1MudPatch` is one producer; spline meshes, modelling-tools blobs and, later, PCG output (mud surface at terrain height plus the terrain lowered under it, e.g. a landscape patch) are others. ARCHITECTURE Mud section states the contract.
  **Done:** ADR in DECISIONS.md, ARCHITECTURE updated.
  **User check:** read the ADR; the authoring model matches how you want to place mud.

- [x] **S2-19** Chaos-free mud model + `FB1MudSimulation` (P1) (4 h) <!-- done: 2026-10-27 -->
  Result: `Mud/Sim/B1MudWheelModel` (namespace, `FB1MudWheelParams`, `FB1MudSinkInput`; only `Curves/RichCurve.h`), `Mud/Sim/B1MudSimTypes.h` (`FB1MudSimParams : FB1MudWheelParams` + weak material, `FB1MudSimInput/Output`), `Mud/Sim/B1MudSimulation` with its own input/output locks. Host hooks: `BeginStep`, `Pre/PostSuspensionTraces`, `Pre/PostFriction`, `EndStep` (6 calls). The mud sim takes the host as `UChaosWheeledVehicleSimulation&` and calls its public `AddForce*` directly, so no add-force callback. `FB1VehicleSimOutput` is gone; `FB1VehicleSimInput` is test force only. Gym 22/22, `Lvl_Offroad` 8/8, all metrics within 1 % of the pre-refactor run (`FullThrottle5s` on firm also moves 0.4 % run to run, so that's the noise floor).
  - `Mud/Sim/B1MudWheelModel`: plain C++, no Chaos or UObject types. Sink update (settle, dig, rise, clamp incl. hard ground), friction scale, rolling resistance coefficient, belly drag limit. POD in (slip m/s, road speed, relative load, dt, params, hard ground depth) and out.
  - `Mud/Sim/FB1MudSimulation`, a member of `FB1VehicleSimulation` (composition, not inheritance). Owns `SinkDepth`, `SinkOffset`, `HardGroundDepth`, `RelativeLoad`, the mud params and output, the sink reset serial and the unstuck assist force. Hooks the host sim calls: `PreSuspensionTraces` (raised traces), `PostSuspensionTraces` (hit push-down, hard-ground trace), `PreFriction` / `PostFriction` (friction multiplier, rolling resistance, belly drag through an add-force callback), `Step`.
  - Mud input/output split out of `FB1VehicleSimInput/Output` into `FB1MudSimInput/Output`. `FB1VehicleSimulation` keeps only the test force and forwards to the mud hooks.
  **Done:** builds; both suites unchanged; `B1MudWheelModel.h` has no Chaos include; the host-side mud code in `FB1VehicleSimulation` is ~5 hook calls.
  **User check:** —

- [x] **S2-20** `UB1MudVehicleComponent` (P2, P3) (4 h) _(requires S2-19)_ <!-- done: 2026-10-27 -->
  Result: `Mud/B1MudVehicleComponent` (`MudVehicle` on `AB1Jeep`) owns `KnownMudMaterials`, `BellyClearance`, `Unstuck()` + assist settings, the clearance log and all mud getters, plus `GetWheelMud(i)` (contact + mud cast, used by InMud, spray, trail, ShowDebug, Mud window, telemetry). `Mud/B1MudSimHost.h` (`IB1MudSimHost`, implemented by `UB1VehicleMovementComponent`): sim pointer, wheel count/state/location/radius/width, effective throttle, raw drive input. No component tick: it binds `FPhysScene_Chaos::OnPhysSceneStep`, which fires on the game thread right after `OnPhysScenePreTick` (where Chaos refreshes `WheelStatus` and runs `Update`). A TG_PrePhysics tick or a PreTick binding (delegates broadcast in reverse, so it ran before Chaos) read last frame's wheel materials, learned new mud a frame late and moved `DeepRunUp` 57.0 → 54.5 m. Spray/trail cache the component in BeginPlay; rig `sink=`/`belly=`/`bUnstuck` read it. ADR-001 amended. Gym 22/22, `Lvl_Offroad` 8/8 within run-to-run noise of the baseline.
  - New actor component on `AB1Jeep` (`Mud|Components`). Owns `KnownMudMaterials` + param gathering, `BellyClearance`, `Unstuck()` and its assist settings, and the mud getters (sink, friction scale, rolling resistance, belly, average sink). Adds `GetWheelMud(i)` (null when airborne or off mud), the single "is this wheel on mud" query that replaces the 6 `Cast<UB1PhysicalMaterial_Mud>` copies.
  - Reaches the sim through `IB1MudSimHost`, a UInterface on the movement component: `FB1MudSimulation*`, wheel count, wheel state, wheel radius/width, effective throttle. Pushes input from its own tick (the buffer is latest-wins) and reads output post-physics, so the host doesn't need to override `Update` for mud.
  - Consumers move to the component, cached in BeginPlay (no per-tick `FindComponentByClass`): spray, trail, `ShowDebug B1Mud`, Mud ImGui window, `B1Unstuck` cheat, `B1Mud` telemetry, rig metrics (`sink=`, `belly=`, `bUnstuck`).
  - Amend ADR-001 (slopes): `SlopeHoldVsSinkDepth` goes on this component, not on `UB1VehicleMovementComponent`.
  **Done:** builds; both suites unchanged; no `UB1VehicleMovementComponent` reference in `Mud/` or the mud debug files except the `IB1MudSimHost` implementation.
  **User check:** drive the gym: sinking, spray, ruts, `ShowDebug B1Mud`, the Mud window and `B1Unstuck` all behave as before.

- [x] **S2-21** InMud tag + mud strain on the mud component (P4) (1.5 h) _(requires S2-20)_ <!-- done: 2026-10-27 -->
  Result: `UB1MudVehicleComponent` ticks `UpdateInMud` (pawn controlled + ASC from `UAbilitySystemGlobals`), `InMudTag` / `MudTagReleaseDelay` (0.5 s) / `MudFullStrainDepth` (50 cm) are properties, cleared on `ReceiveControllerChangedDelegate` and in EndPlay. `InMudTag` is set in `AB1Jeep`'s constructor (not the BP), so the project tag stays out of `Mud/`. `GetMudStrain()` on the component; `MudFullStrainDepth` removed from `UB1VehicleTuningData` (the asset's saved 50 is ignored on load), now a live field in Vehicle Status without Save (component value). `ShowDebug B1Mud` reads the ASC from the owner and `InMudTag`. `B1Fuel_Medium` 0.367 L/100 m, `inMud=3.38`; `B1Fuel_DeepFull` 343.5 L/h; no leftover-tag error. Gym 22/22, `Lvl_Offroad` 8/8.
  - `UpdateInMud` / `SetInMud` move from `AB1Jeep`. ASC through `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner)`. Tag from `UPROPERTY FGameplayTag InMudTag` (set to `Vehicle.State.InMud` on `BP_B1Jeep`), release delay a property. Cleared when the owner pawn's controller changes (`ReceiveControllerChangedDelegate`), so the jeep doesn't have to call it.
  - `GetMudStrain()` = effective throttle × clamp(average sink / `MudFullStrainDepth`). `MudFullStrainDepth` moves here from `UB1VehicleTuningData`.
  **Done:** builds; `B1Fuel_Medium` `inMud=` and fuel numbers unchanged; no leftover-tag error after re-possess.
  **User check:** —

- [x] **S2-22** `UB1FuelConsumptionComponent` (2.5 h) _(requires S2-21)_ <!-- done: 2026-10-27 -->
  Result: `Fuel/B1FuelConsumptionComponent` (`FuelConsumption` on `AB1Jeep`): consumption, L/100 m window, out-of-fuel tag + `OnOutOfFuelChanged`; binds the fuel change delegate on the owner's ASC lazily in its tick and unbinds on controller change. `Fuel/B1FuelConsumptionData` (CoreRedirect from `UB1VehicleTuningData` in `DefaultEngine.ini`; `MudWorkRate` became `LoadRates["Mud"]` = 300, the asset stored only defaults). `Fuel/B1EngineLoadSource.h` (movement) and `Fuel/B1FuelLoadSource.h` (mud component, load `FuelLoadName` "Mud" = strain). Decision: rate keyed by name in the fuel data. Deviation: `Mud/` includes the one interface-only header `B1FuelLoadSource.h`; fuel includes nothing from mud. Making the jeep forward the strain would avoid that but puts fuel code on the jeep. Attribute/tags defaulted in `AB1Jeep`'s constructor; `Data` = `DA_B1JeepTuning` and `DrainEffect` = `GE_B1FuelDrain` set on `BP_B1Jeep` via MCP. `LogB1Fuel` in `Fuel/B1FuelLog.h`. Vehicle Status lists each load and its rate field generically; `B1Status` reads the component (`strain=` is load "Mud"). Fuel rigs unchanged (Medium 0.367 L/100 m, FirmFull 74.0, DeepFull 343.5 L/h, Empty 0). Gym 22/22, `Lvl_Offroad` 8/8. ADR-010 amended.
  - `ConsumeFuel`, the fuel change delegate / out-of-fuel handling and the L/100 m window move from `AB1Jeep` into `Fuel/UB1FuelConsumptionComponent`.
  - No tie to this project's GAS classes: `FGameplayAttribute FuelAttribute`, drain GE class, SetByCaller tag and `OutOfFuelTag` are properties (`BP_B1Jeep`: `Fuel`, `GE_B1FuelDrain`, `Data.Fuel`, `Vehicle.State.OutOfFuel`). Works on any `UAbilitySystemComponent` found from the owner.
  - Loads come through interfaces so fuel and mud don't include each other: engine power + throttle from `IB1EngineLoadSource` (on `UB1VehicleMovementComponent`), mud strain from `IB1FuelLoadSource` (on `UB1MudVehicleComponent`). 🔍 Decide where the mud rate lives: per-source rate keyed by name in the fuel data (recommended) or the source returning L/h.
  - Out of fuel: the component broadcasts `OnOutOfFuelChanged(bool)`; `AB1Jeep` binds it and sets the movement's fuel cut-off (the cut-off stays vehicle-specific).
  - `UB1VehicleTuningData` (all fuel fields) becomes `UB1FuelConsumptionData` in `Fuel/` with a CoreRedirect, so `DA_B1JeepTuning` keeps working.
  - Vehicle Status window, `B1Status` telemetry and fuel rig metrics read the component. ADR-010 amended.
  **Done:** builds; all fuel rigs PASS with unchanged numbers; `AB1Jeep` has no fuel code beyond the cut-off binding.
  **User check:** Vehicle Status fuel section, rate edits and "Save to asset" still work.

- [x] **S2-23** Trail receivers off `AB1MudPatch` (A2) (2 h) _(requires S2-18)_ <!-- done: 2026-10-27 -->
  Result: `Mud/B1MudTrailSurfaceComponent` (`TrailSurface` on `AB1MudPatch`): `Initialize(SurfacePrimitive, Size)` in game worlds creates the R16F target (`Resolution` 512), the MID on the primitive's slot 0 (`TrailMask`, `TrailDepthRange`, `PatchSizeX/Y`), a world box (rect transformed, ±`TrailDepthRange` in Z) and registers; unregisters in EndPlay. `WorldToTrailUV` maps local XY / Size + 0.5. `UB1MudTrailSubsystem` registers surfaces, tests the world box before the UV, and includes only the surface header. The patch calls `TrailSurface->Initialize(MudVisual, Size)` after building the grid. Simulate + `CaptureEditorImage` with `B1Mud_CrossMedium` (bAutoRun toggled, restored): two sloped ruts on Medium. Gym 22/22, `Lvl_Offroad` 8/8.
  `UB1MudTrailSurfaceComponent` owns the render target, the MID params and `WorldToTrailUV` over a local XY rect. `AB1MudPatch` adds one and sets its size. `UB1MudTrailSubsystem` registers components, not patches, with a world-bounds prefilter before the UV test (ready for many PCG surfaces). The wheel → subsystem side stays world-space as it is.
  **Done:** builds; capture after `B1Mud_CrossMedium` shows ruts as before; the subsystem doesn't include `B1MudPatch.h`.
  **User check:** —

- [x] **S2-24** `UB1MudSettings`, `LogB1Mud`, folder layout (P5, P6) (2 h) _(requires S2-20, S2-23)_ <!-- done: 2026-10-27 -->
  Result: `Mud/B1MudSettings.h` (Config=Game, `DefaultGame.ini`): `MudObjectChannel`, `SurfaceProfileName`, `SpraySystem` (soft; the spray component's `ConstructorHelpers` path is gone and an unset `SpraySystem` falls back to it). `Physics/B1CollisionChannels.h` deleted. ⚠️ resolved: `WheelTraceCollisionResponses` is set in `UB1VehicleMovementComponent::OnRegister`, not the constructor; `FB1MudSimulation` copies the channel in its constructor (game thread); the patch sets the profile in its constructor and again in `OnConstruction`. `LogB1Mud` (`Mud/B1MudLog.h`) for all mud code and the `B1Mud` telemetry line (231 lines in the gym run, none left on `LogB1Physics`). `B1MudShowDebug` / `B1ImGuiMud` moved to `Mud/Debug/`; include paths `Mud/Debug`, `Fuel` added, `Physics` removed. Gym 22/22, `Lvl_Offroad` 8/8 within noise; no spray/host/fuel warnings.
  - `UB1MudSettings : UDeveloperSettings` (Project Settings → B1 Mud, `DefaultGame.ini`): mud object channel, surface profile name, spray Niagara system as a soft ref (replaces the `ConstructorHelpers` path). Replaces the `B1CollisionChannels.h` macros.
    ⚠️ Note: `WheelTraceCollisionResponses` is set in the movement constructor (CDO time); confirm the settings CDO is loaded from config by then, or set the response in `OnRegister`/`CreatePhysicsVehicle`.
  - `LogB1Mud` in `Mud/B1MudLog.h`, used by all mud code (the `B1Mud` telemetry line too), so `Mud/` doesn't need `B1Log.h`.
  - Layout: `Mud/` runtime, `Mud/Sim/`, `Mud/Debug/` (`B1MudShowDebug`, `B1ImGuiMud`); `Fuel/` for the fuel component and data.
  **Done:** builds; both suites pass; no `ECC_GameTraceChannel1` outside the settings default, no `LogB1Physics` in `Mud/`; ARCHITECTURE modules/log list updated.
  **User check:** Project Settings → B1 Mud shows the channel and spray asset.

- [x] **S2-25** Migration guide + keep-it-current rule (1.5 h) _(requires S2-22, S2-23, S2-24)_ <!-- done: 2026-10-27 -->
  Result: `.claude/docs/MIGRATION.md` (files, cross-folder includes, Build.cs/plugins, ini, content, host interfaces + sim hook snippet, GAS property table, debug wiring, surfaces, verification) and `.claude/rules/portability.md`; both in CLAUDE.md, imgui rule scope widened to `Mud/Debug/`. Include check: the only project headers crossing out of `Mud/`/`Fuel/` are `B1FuelLoadSource.h` (Mud → Fuel) and `B1ImGuiWidgets.h` / `B1ImGuiAssetSave.h` (Mud/Debug → Debug), as listed in the guide.
  - `.claude/docs/MIGRATION.md`: copying mud and fuel into another project. Folders and files; Build.cs modules and plugins; `DefaultEngine.ini` channel + profile and `DefaultGame.ini` settings; content (`M_B1Mud`, `NS_B1MudSpray`, `M_B1MudSpray`, `PM_B1Mud_*`, `MI_B1Mud_*`, `BP_B1MudPatch`, `GE_B1FuelDrain`); what the host vehicle implements (`IB1MudSimHost`, `IB1EngineLoadSource`, the `FB1MudSimulation` hook calls in its Chaos sim subclass, with a snippet); GAS wiring (tags, fuel attribute, drain GE on the components); debug wiring (ImGui window registration, ShowDebug); the `B1` prefix; verification (mud rigs or a smoke drive).
  - Rule `.claude/rules/portability.md` (applyTo `Source/MudDriver/Mud/**`, `Source/MudDriver/Fuel/**`): no includes of project-specific classes from these folders; any change that adds a host requirement, setting, asset or dependency updates MIGRATION.md in the same task. Listed in CLAUDE.md's docs and rules tables.
  **Done:** guide and rule exist; the includes in `Mud/` and `Fuel/` match the guide's dependency list.
  **User check:** read the guide: it matches what you'd do in the other project.

### Playtest feedback (added 2026-10-27, S2-15 drive)

- [x] **S2-26** Fuel tank 20 L (0.25 h) <!-- done: 2026-10-27 -->
  Result: `UB1VehicleAttributeSet` inits Fuel/MaxFuel to 20. `B1Status` shows `fuel=20.00/20`; fuel rigs PASS unchanged (rates are per hour). Also fixed a unity-build break from cf7c2cc: `CmPerM` / `NewtonsToUnreal` / `GravityCmPerS2` were defined in anonymous namespaces of three .cpp files that land in one blob (adaptive unity hid it while they were uncommitted). They're now public in `B1MudWheelModel`; verified with `Build.bat ... -DisableAdaptiveUnity`.
  `MaxFuel` and starting `Fuel` 60 → 20 L, so consumption is visible in presentations and tests.
  **Done:** builds; fuel rigs PASS; Vehicle Status shows 20 / 20 L.
  **User check:** —

- [x] **S2-27** HUD fuel bar (1.5 h) <!-- done: 2026-10-27 -->
  Result: `UMudDriverUI::UpdateFuel(Fuel, MaxFuel)` fills optional bound widgets `FuelBar` (percent; fill hue 120° → 0° between full and `FuelLowFraction` 0.15) and `FuelText` (`%.0f/%.0f L`). `AB1PlayerController::Tick` feeds it from the PlayerState ASC. The template HUD was duplicated to `/Game/B1/UI/WBP_B1VehicleHUD` (`BP_B1PlayerController.VehicleUIClass` points at it); a `FuelBox` column between gear and speed holds `FuelBarSize` (220×18) → `FuelBar` and `FuelText` (Roboto Light 15, like the unit label). PIE capture: green full bar with `20/20 L` between gear and speed.
  A bar + text (`20/20 L`) next to speed and gear in the vehicle HUD; colour goes green (full) → red (almost empty). Reads the Fuel/MaxFuel attributes from the player's ASC.
  **Done:** builds; capture in PIE shows the bar next to speed/gear.
  **User check:** the bar drains while driving and turns red near empty.

- [x] **S2-28** Mud spray looks like mud (2 h) <!-- done: 2026-10-27 -->
  Result: every `PM_B1Mud_*` `SprayColor` = (0.13, 0.075, 0.035) linear. Heavy's old 0.26/0.15/0.07 read pale tan in sun once the sprites were shaded. `M_B1MudSpray`: Custom node `SphereNormal` (flattened dome normal from sprite UV, ×0.6 in XY) → Normal, Roughness 0.2, Specular 0.6, so each sprite shades like a wet clump instead of a flat disc. The flat dark discs were what read as smoke; `ScaleColor` only fades alpha, which the masked material ignores. `NS_B1MudSpray` sprite size 10–25 → 10–28 cm. Simulate captures with `B1Mud_DigInMedium` (`bAutoRun` + `SettleTime` toggled and restored; StartPIE blocks ~2 s, so capture right after it): dark brown glossy clumps thrown back off the wheels.
  Spray reads as smoke, worst when dark. All presets spray brown (Heavy's colour), and the particles look wetter and heavier: glossier, less transparent, bigger clumps.
  **Done:** capture in Simulate with a digging rig shows brown, wet-looking clumps.
  **User check:** the spray reads as mud on every preset.

### Close

- [x] **S2-15** Fuel playtest + tuning (user + agent, 1 h) _(requires S2-06, S2-09, S2-22)_ <!-- done: 2026-10-27 -->
  Result: user drove and kept the rates (6 / 240 L/h, `Mud` 300 L/h, 0.25 s); feedback became S2-26..S2-28 (20 L tank, HUD fuel bar, spray look). Fuel rigs PASS: Firm 0.085 L/100 m, Medium 0.368 (4.3×), FirmFull 74 L/h, DeepFull 343.5 L/h, Empty 0 m. ✅ signed off 2026-10-27.
  You drive the gym and tune the rates in Vehicle Status. The agent writes the values into `DA_B1JeepTuning` and re-runs S2-09.
  **Done:** fuel rigs PASS with the tuned values.
  **User check:** sign-off on fuel pressure (Phase G).

- [x] **S2-16** Phase 1 close playtest (user + agent, 1 h) _(requires S2-10..S2-14b, S2-18..S2-25)_ <!-- done: 2026-10-27 -->
  Result: user checked the refactor (S2-18..S2-25), the HUD fuel bar and the new spray: all good. Both suites PASS (gym 22/22, `Lvl_Offroad` 8/8). ROADMAP Phase 1 and Phase G ticked. ✅ signed off 2026-10-27.
  Mud checklist (ROADMAP Phase 1) plus spray, trails, `ShowDebug B1Mud`, `B1Unstuck`. You also check `stat unit` on the laptop (sanity only).
  Drive `Lvl_Gym_Swamp` too (large mixed mud, lake, hills, basin); S2-15 fuel tuning can use it as well.
  **Done:** both suites PASS; ROADMAP Phase 1 and Phase G ticked.
  **User check:** sign-off on Phase 1.

### Backlog (found during the sprint)

- ~~`Lvl_Offroad` load logs `LoadErrors: ... /Lvl_Offroad/B/9M/ZNZM1Q9PKXHV07GMT8Q6NN, a dependent package None was not available`.~~ Gone with the map (2026-10-27): its rigs moved to `Lvl_Gym_Core` and `Lvl_Offroad` was deleted. _(found in S2-03)_

---

## Sprint 3 — Mud + winch core (Phase 2, part 1)

**Status:** closed 2026-10-27 (S3-16 sign-off).

**Goal.** Prove mud + winch before any on-foot work. Mud learns slopes (slide when barely sunk, hold when deep, never climb unwinched), the winch pulls the jeep out of deep mud and up the muddy slope, and the user feels it at a checkpoint with the hook attached by cheat.

**Sprint-wide notes**
- ⚠️ After S2-19..S2-22, mud forces live in `FB1MudSimulation` / `B1MudWheelModel` and mud config on `UB1MudVehicleComponent`; slope and winch-drag-cap code goes there, not into `FB1VehicleSimulation`. Keep `MIGRATION.md` current (S2-25 rule).
- ⚠️ The slope rules must not change flat ground. Every task touching `FB1VehicleSimulation` or `FB1MudSimulation` re-runs `Lvl_Gym_Mud` (21) and `Lvl_Offroad` (8) headless; numbers stay within 5 % of Sprint 2.
- ⚠️ `ActorTools.set_actor_transform` resets omitted fields: always pass location, rotation and scale when placing slopes and rotated patches.
- ⚠️ `Lvl_Gym_Winch` is WP/OFPA like `Lvl_Gym_Mud`: new actors need `save_assets([])` to land on disk; check `git status`.
- ⚠️ The agent can't drive or run cheats through MCP. Winch behaviour is verified by rigs with `WinchAnchor` / `WinchPull`; cheats, ImGui and ShowDebug are user checks.

### Slopes (ADR-001 amendment)

- [x] **S3-01** `Lvl_Gym_Winch` base + test slope (3 h) <!-- done: 2026-10-27 -->
  Result: `/Game/B1/Gym/Lvl_Gym_Winch`, a duplicate of `Lvl_Gym_Mud` with lanes, pit, rigs and ground tiles removed. Floor `G_Floor`: one 1 m tile, X −50..120, Y −60..80 m; walls, lighting and `BP_B1GymGameMode` kept.
  - Slopes (`Gym/Slope`) at 20°, 30 m long, foot at X 20 m, top at X 48.19 m / Z 10.26 m: `Slope_Firm` (1 m slab, Y −5..5) and `Mud_Slope_Medium` (`Gym/Mud`, Y 15..25, `PM_B1Mud_Medium`, hard bottom 35 cm, ramp only on −X). `Slope_Plateau` spans both lanes (10 m, Y −5..25) with a 1.5 m `Slope_Kerb` at its end. Run-ups X 0..20 m. No CDO/code changes.
  - Spawns `Spawn_1_FirmSlope` (`Player0`, X 2 m, Y 0) and `Spawn_2_MudSlope` (Y 20 m); labels `1 FIRM 20 DEG`, `2 MEDIUM 20 DEG`. PIE spawns the jeep on the firm run-up.
  - Rig `B1Slope_DropMedium` (mid mud slope, 1 m up along the normal, pitch 20°, 3 s handbrake) PIE PASS: sink=15.4/15.4/23.6/23.6, so mud reads on the rotated patch. The jeep slides ~1 m during the settle (no slope hold yet).
  - The ⚠️ note didn't apply: the hard-ground trace runs along the suspension direction (≈ the slope normal), not vertically, so a 35 cm bottom stays 35 cm away. `trace_world` confirms the bottom 37.2 cm below the surface vertically (35 / cos 20°).
  Duplicate `Lvl_Gym_Mud`'s setup (floor tiles, hidden walls, sky, `BP_B1GymGameMode`), strip the lanes. Add one test slope pair at 20°: a firm slope (rotated cube tiles) and a medium-mud slope (rotated `BP_B1MudPatch` with `PM_B1Mud_Medium`, own hard bottom, ramp only at the foot), each ~30 m long with a 20 m flat run-up and a 10 m flat top. Spawns at each run-up.
  ⚠️ Note: `AB1MudPatch` builds with the actor rotation (`B1MudPatch.cpp:88`), but the hard-ground cap trace is vertical and `MaxSinkDepth` long. On a slope the bottom is `depth / cos θ` away, so lengthen the trace if needed.
    - User check ✅ (today's behaviour on the medium slope): "goes up with more difficulty because of reduced traction, but it is easy to climb"; "when stopped, the car stays in place, no sliding down". Medium at 20° is the opposite of the S3-05 target (must fail) and the S3-04 premise (idle slides). S3-02's baseline has to show whether the hold comes from the idle brake or from tire grip, before S3-03/04 are designed further.
  **Done:** builds; viewport capture of the slopes; PIE spawns the jeep at the run-up; a rig dropped on the mud slope shows `sink=` > 0 (mud is read on the rotated patch).
  **User check:** drive both slopes once as they are: describe what the jeep does on the mud slope today.

- [x] **S3-02** Rig `maxH` + slope baseline (2 h) _(requires S3-01)_ <!-- done: 2026-10-27 -->
  Result: rig `maxH=` + `MaxPeakHeight` (toggle `bCheckPeakHeight`) and `bDisableVehicleSleep`. Six `B1Slope_Base_*` rigs in `Lvl_Gym_Winch` (`B1Tests/Slope`); player spawns moved to X −10 m so run-up rigs don't hit the player jeep. Numbers in HISTORY.md: firm 50 % and medium 100 % top out, medium 50 % reaches 8.7 of 9.3 m, medium 30 % stalls at 1.8 m; medium idle holds with sleep on and off. Holding comes from Chaos's auto-brake + sleep and from medium's grip (~0.39) beating tan 20° (0.36). Flat suites run with S3-03.
  New rig metric `maxH` (peak height above the rig) and threshold `MaxPeakHeight`. Baseline rigs on the test slopes, measured **before** any rule change: firm 50 %, medium 30/50/100 % (15 s from the run-up), medium idle from mid-slope (10 s). Record numbers in HISTORY.md.
  **Done:** builds; baseline numbers recorded; existing suites still pass.
  **User check:** —

- [x] **S3-03** Net-force standstill rule (2.5 h) _(requires S3-02)_ <!-- done: 2026-10-27 -->
  Result: `FB1MudSimulation::ApplyRollingResistance` uses net push = tire force − weight share × wheel-forward Z (gravity along the steered ground forward from the hit normal); `ApplyBellyDrag` adds mass·g projected on the average wheel-hit ground plane to the summed tire push.
  - Keep-awake: Chaos sleeps below 10 cm/s on slopes up to 30°, so a jeep on a muddy slope could never slide. The sim outputs `MudGroundUpZ` (lowest hit normal Z under mud wheels); `UB1MudVehicleComponent` calls the new `IB1MudSimHost::SetMudKeepAwake` when it's steeper than `KeepAwakeSlope` (3°). `UB1VehicleMovementComponent` sets `SleepSlopeLimit` to 1 (restores on false) and wakes a sleeping body; it leaves `SleepThreshold` to the test force. MIGRATION.md updated.
  - Headless: gym 22/22 and `Lvl_Offroad` 8/8 PASS within 1 % of Sprint 2 (`s224*.log`); winch gym 7/7. `B1Slope_Base_MediumIdle` now stays awake and still holds (lastDisp 0.050 vs 0.022): at 20° locked-wheel grip holds it, and resistance no longer pushes it downhill.
  Rolling resistance (`ApplyWheelFrictionForces`) and belly drag (`ApplyBellyDrag`) judge "push" from the net force along the ground: tire force + gravity along the ground (+ winch later, S3-10), not `GetForceFromFriction().X` alone. On flat ground gravity along the ground is 0, so nothing changes there.
  ⚠️ Note: at rest on a slope the tire force is mostly the tire holding against gravity; the old rule applied resistance downhill. Gravity along the ground per wheel = weight share × (gravity · ground forward); for belly drag use the whole-body component on the ground plane.
  **Done:** builds; both flat suites within 5 % of Sprint 2; the medium idle rig no longer accelerates downhill from the resistance itself (compare with the S3-02 baseline).
  **User check:** —

- [x] **S3-04** Slope hold by sink depth (3 h) _(requires S3-03)_ <!-- done: 2026-10-27 -->
  Result:
  - `SlopeHoldVsSinkDepth` on `UB1MudVehicleComponent` (linear 0 @ 24 cm → 1 @ 30 cm; medium rests at 14–24 cm), copied to the sim. `B1MudWheelModel::GetSlopeHold` acts per mud wheel along the wheel forward, only on mud steeper than `KeepAwakeSlope` (3°):
    - at rest it cancels the net downhill push up to curve × weight share;
    - sliding downhill (ground speed, since a braked wheel's RoadSpeed reads ~0) it applies min(limit + `SlopeSlideDrag` (0.6 of load at ≥ 1 m/s), push + stop share);
    - zero while climbing.
    - It runs before rolling resistance, which sees the remainder.
  - Why the idle jeep never slid (found while testing):
    - Chaos's engine braking (900 rpm × 0.2 = 180 Nm brake torque at zero throttle) locks a near-stopped wheel. User decision: with no input on a muddy slope, `PreFriction` zeroes the mud wheels' brake torque.
    - Chaos's suspension anti-slide pushes straight up below 10 mph on slopes ≤ 45°. Off project-wide (ADR-016, `DefaultEngine.ini`).
  - Mud window: `Hold N` column, live curve keys (drag + typed, `sink cm`), slide drag and slope angle fields. Telemetry `hold=`; rig `hold=` (peak sum).
  - Headless: flat suites unchanged (hold=0 everywhere; gym 22/22 within 1 %). With the anti-slide off, `Lvl_Offroad` 8/8 PASS but moves: FullThrottle5s 85.3 → 89.0 m, Push drift 7.6 → 0.7 m (frame-rate pair still agrees).
  - At 20° on the final lanes: medium idle slides 4.45 m at 0.45 m/s; dug in to 35 cm it holds (gain 0.2 m).
  - **Reworked after your S3-06 feedback (2026-10-27):** "dug deep, I'm stuck, can't go up nor reverse". The hold curve and slide drag are removed. Instead, every mud wheel on a slope gets `SlopePull` (0.5) × gravity along the ground as an extra force down the slope, always on. The brake release, keep-awake and anti-slide off all stay. Mud window: `Slope pull` field + `Pull N` column; telemetry/rig `pull=`.
  `SlopeHoldVsSinkDepth` (`FRuntimeFloatCurve` over absolute cm, fraction of wheel load) on `UB1MudVehicleComponent` (`Mud|Config`, per the S2-20 ADR-001 amendment), copied to `FB1MudSimulation`. Per mud wheel: a force that cancels the net downhill force along the ground up to `curve(SinkDepth) × wheel load`, never more, so it can't push the jeep uphill. Start curve: 0 at 0–15 cm, 1 at 30 cm. Mud window: editable curve keys (typed fields per `imgui.md`) and a per-wheel `hold` column; `B1Mud` telemetry gets `hold=`.
  **Done:** builds; flat suites unchanged (the term is 0 on flat ground); medium idle on the test slope slides; a jeep dug in to ≥ 30 cm on the slope holds.
  **User check:** on the medium slope, stop and let go: the jeep slides back slowly. Floor it until it digs in, let go: it stays. Edit the curve live and feel the change.

- [x] **S3-05** Slope angle + final slope lanes (2.5 h) _(requires S3-04)_ <!-- done: 2026-10-27 -->
  Result:
  - **θ = 20°, 40 m long.** With the anti-slide on, every angle in 20–30° let medium climb at 50 %. With it off, firm 50 % rolls back at 25° (maxH 8.9 of 11.7) and 30° (6.3 of 14.0), so 20° is the steepest that works. At 30 m, medium 100 % topped out on run-up momentum (16.8 m/s at the foot); at 40 m it stops at 8.65 of 12.68.
  - Lanes: firm (Y 0), medium (Y 20), deep (Y 40, `Mud_Slope_Deep`, hard bottom 70, ramp 400 at the foot). Plateau Y −5..45 with kerb. Anchor posts `Anchor_Medium` / `Anchor_Deep` 8 m past each mud top. `Spawn_3_DeepSlope` and `Label_3_DeepSlope` added. Viewport capture checked; layout in ARCHITECTURE.md (Winch gym).
  Pick θ: firm climbs at 50 %, medium fails at 30/50/100 %. If no θ in 15–30° does both, stop and raise a per-material slope grip factor (ADR-001 fallback) with the user. Build three lanes at θ: firm, medium, deep (`PM_B1Mud_Deep`), each with run-up and top plateau, and an anchor marker 8 m past each mud top. Record θ, lengths and anchor spots in ARCHITECTURE.md (Winch gym).
  **Done:** viewport capture; θ and layout in ARCHITECTURE.md.
  **User check:** the slopes read clearly; θ looks like a hill you'd expect a jeep to climb on dry ground.

- [x] **S3-06** Slope rig suite, unwinched (2.5 h) _(requires S3-05)_ <!-- done: 2026-10-27 -->
  Result: 7/7 PASS headless (`B1Tests/Slope`; plateau +12.68 m above a run-up rig). FirmClimb endH 12.69; Medium30/50/100 maxH 0.22/3.19/8.65 (limit 11.68); MediumIdle dist 4.45, vend 0.45; MediumDugIn maxH −0.69 (limit −0.39); DeepIdle dist 1.05, lastDisp 0.001 (settle 0.2 s so the slide is measured). The base rigs became these; `MediumIdleAwake` and `DropMedium` were removed. Gym 22/22 and `Lvl_Offroad` 8/8 PASS.
  **After the slope-pull rework:** 8/8 PASS headless, gym 22/22 PASS.
  - FirmClimb 12.69.
  - Medium30/50/100 maxH −0.19/1.88/5.64; they end lower than they started.
  - MediumDugIn slides back at 50 % (maxH −2.00).
  - New `B1Slope_MediumDugInReverse` (3 s full, 6 s reverse): backs out, 88 m.
  - MediumIdle and DeepIdle (now "slides ≥ 1 m") roll to the bottom: 42.8 m at up to 7.2 m/s, and 33.1 m at up to 5.5 m/s.
  ✅ First user check (2026-10-27): the hold trapped a dug-in jeep (fixed above); the rest was not reported.
  ✅ User check (repeat, 2026-10-27): user tuned `SlopePull` to 0.3 on `BP_B1Jeep` and likes the effect. Signed off.
  **User check (repeat):** in `Lvl_Gym_Winch`:
  1. Medium: you keep sliding down, you can't climb, and dug in you can still reverse out.
  2. Idle on medium/deep: the roll-back speed feels right. Tune `Slope pull` live in the Mud window.
  3. Firm lane: still an easy climb.
  4. `Lvl_Offroad` and the swamp: hills feel right with Chaos's anti-slide off.
  - `B1Slope_FirmClimb`: 50 % from the run-up, reaches the top (`endH` ≥ top − 0.5 m).
  - `B1Slope_Medium30/50/100`: 15 s from the run-up, `maxH` ≤ top − 1 m.
  - `B1Slope_MediumIdle`: mid-slope, zero throttle 10 s, slides ≥ 1 m, `vmax` ≤ 1.5 m/s.
  - `B1Slope_DeepIdle`: mid-slope, zero throttle 10 s, slides ≥ 0.1 m, `lastDisp` ≤ 5 cm over the last 5 s.
  - `B1Slope_MediumDugIn`: 3 s full, then 10 s at 50 %, gains < 0.5 m height.
  ⚠️ Note: rigs on the same lane collide in PIE; run in rounds, one per lane.
  **Done:** all PASS headless; flat suites PASS.
  **User check:** drive the three slopes: firm is easy, medium is impossible however careful you are, deep slides then sticks.

### Winch core (ADR-007 amendment)

- [x] **S3-07** Winch + anchor components, tree, cable visual (3 h) _(requires S3-01)_ <!-- done: 2026-10-27 -->
  Result:
  - `Source/MudDriver/Winch/`: `UB1WinchComponent` (scene component = winch point, `Winch` on `AB1Jeep` at mesh-relative (230, 0, 60), the nose; `Winch|Config` MaxCableLength 4000 cm, MaxPullForce 40 kN, ReelInSpeed 100 cm/s, CableStiffness 200 kN/m, CableDamping 20 kN·s/m; `Winch|Visual` width 7 cm (2 cm was barely visible in PIE, user check), segments, material; `BP_B1Jeep` uses red `/Game/B1/Winch/MI_B1WinchCable` (child of the engine `BasicShapeMaterial`), user request). `AttachTo` refuses anchors past `MaxCableLength`, rest = current distance, and spawns a fresh `UCableComponent` (no collision, 16 solver iterations + stiffness, since the default single iteration sagged it through the floor); `Detach` destroys it. `UB1WinchAnchorComponent::FindNearest` (object iterator, same world).
  - `AB1WinchTree` (root, `Trunk` mesh, `Anchor` at 80 cm) → `/Game/B1/Winch/BP_B1WinchTree` (Cylinder 0.5 × 0.5 × 6 = 50 cm × 6 m).
  - Cheats `B1WinchAttachNearest`, `B1WinchDetach` (log to `LogB1Physics`). `CableComponent` plugin + private module dep.
  - Added `StartAnchor` (`Winch|Debug`, EditInstanceOnly): a placed jeep hooks that actor's anchor at BeginPlay, since MCP can't call functions. Simulate in `Lvl_Gym_Mud` with a temporary jeep + tree (not saved): "winch attached …, rest 13.06 m", capture shows the cable from the nose to the trunk.
  `UB1WinchComponent` (scene component on `AB1Jeep`, at a `WinchPoint` on the front bumper): attached anchor, rest length, `MaxCableLength` (40 m), `MaxPullForce` (40 kN), `ReelInSpeed` (1 m/s), stiffness, damping; category `Winch|Config`. `UB1WinchAnchorComponent` (scene component). `BP_B1WinchTree`: a placeholder trunk mesh + anchor. Cable drawn with `UCableComponent` from the winch point to the anchor while attached. Cheats `B1WinchAttachNearest` (nearest anchor within `MaxCableLength`, rest length = current distance), `B1WinchDetach`. No force yet.
  🔍 Research first: `CableComponent` in 5.8 (enable the plugin in `MudDriver.uproject`, module dep, attaching the end to a moving component).
  ⚠️ Note: new plugin + new classes = full build with the editor closed (`Scripts/BuildEditor.ps1`).
  **Done:** builds; Simulate + capture shows the cable from the jeep to a tree after an attach (set via MCP on the component).
  **User check:** `B1WinchAttachNearest` near a tree: the cable appears; `B1WinchDetach` removes it.

- [x] **S3-08** Winch tension in the simulation (3 h) _(requires S3-07)_ <!-- done: 2026-10-27 -->
  Result:
  - `FB1WinchSimInput` in `FB1VehicleSimInput` (anchor, winch point in the body frame, attach serial + initial rest, pull, params). `UB1WinchComponent` ticks in PrePhysics, sends it through `UB1VehicleMovementComponent::SetWinchInput` and reads `FB1WinchSimOutput` (tension, rest, length, world force) back. The movement keeps the body awake while attached or with a test force (`RefreshForceKeepAwake`).
  - `FB1VehicleSimulation::UpdateWinch` runs before the base step from the body handle (pose this step; velocity at the point = V + W × r): rest shrinks at `ReelInSpeed × Pull` down to `MinRestLength` (150 cm), including slack, but the drum stalls once the spring force reaches `MaxPullForce`. Tension = clamp(k·stretch − c·closing speed, 0, cap), `AddForceAtPosition` at the winch point after the base step. New module dep `Chaos` (handle accessors).
  - Loose `Vehicle.State.Winching` (`WinchingTag`, set in `AB1Jeep`) on the driver's ASC while attached with pull > 0, only when the pawn is controlled. `Pull` is editable on placed jeeps (`Winch|Debug`); cable visual length = rest length. Detach logs rest start → end and peak tension.
  - Simulate in `Lvl_Gym_Mud` (temporary actors, not saved), full pull: jeep 12.7 m from a tree reeled to 1.5 m and parked at the trunk (peak 8.5 kN); jeep against the north wall with the tree behind it stalled at 40 000 N (cap).
  Game thread sends anchor world position, rest length, pull and params through `FB1VehicleSimInput`. `FB1VehicleSimulation` applies a one-sided spring-damper, capped at `MaxPullForce`, with `AddForceAtPosition` at the winch point (its own path; `EB1TestForce` stays). Rest length shrinks at `ReelInSpeed × pull` while taut. The body stays awake while attached (same rule as the test force). Output: tension N, current length. Loose `Vehicle.State.Winching` while pull > 0 (new native tag).
  ⚠️ Note: the winch point moves with the body; compute it on the physics thread from the body transform, not from a game-thread position a frame old.
  **Done:** builds; a rig with a fixed anchor and full pull on firm ground drags a braked jeep toward the anchor; tension never exceeds the cap.
  **User check:** —

- [x] **S3-09** Winch pull input (1.5 h) _(requires S3-08)_ <!-- done: 2026-10-27 -->
  Result: `DataAssetTools.create` makes `UInputAction`s. `/Game/B1/Input/IA_B1WinchPull` (Axis1D) and `IA_B1WinchToggle` (bool; user decision: one toggle that attaches the nearest anchor or releases, instead of release only). Appended to `IMC_Vehicle_Default` with a ProgrammaticToolset script (41 → 45, Negate modifiers kept): pull = E / Gamepad D-pad Up, toggle = Q / D-pad Down (user's pad layout, so pull is digital on the pad). `AB1Jeep` binds them natively (`WinchPullAction` / `WinchToggleAction`, set on `BP_B1Jeep`): pull Triggered/Completed → `SetPull`, toggle Started → `ToggleAttachNearest`.
  `IA_B1WinchPull` (analog, axis1D: trigger / key) and `IA_B1WinchRelease` (detach) in the vehicle mapping context; the jeep binds them natively (like throttle), pull goes to `UB1WinchComponent`.
  🔍 Research first: whether `DataAssetTools.create` makes `UInputAction` assets (it's a `UDataAsset` subclass) and how to add a mapping to the template IMC through MCP. Fallback: you add the two mappings by hand from a spec.
  **Done:** builds; the IMC asset lists both mappings.
  **User check:** attach by cheat, hold the pull key/trigger: the cable tightens and pulls; analog trigger pulls proportionally.

- [x] **S3-10** Winch counts as push in the drag caps (1.5 h) _(requires S3-03, S3-08)_ <!-- done: 2026-10-27 -->
  Result: portable hook `FB1MudSimulation::SetExternalForce` (world N, reset in `BeginStep`; MIGRATION.md updated). `FB1VehicleSimulation` passes the winch force before the base step. Rolling resistance adds each mud wheel's weight share of the force along the wheel forward to its net push; belly drag adds the force projected on the ground plane.
  - Effect found while testing: before this, mud barely resisted the winch (at standstill the cap was the tire push, ~0; moving, push + 0.4 × stop force), so the winch dragged the jeep almost freely. Now mud resists it at full strength.
  - Simulate in `Lvl_Gym_Mud` (temporary, not saved): jeep settled in deep (35–45 cm), zero throttle, full pull to a tree 21 m away → pulled out at 3.6 km/h (reel speed), `rr=` 500–700 N per wheel in the `B1Mud` telemetry, peak tension 8.7 kN. Belly stayed 0 (static sink < 45 cm clearance); the dug-in belly case needs the S3-11 rig (S3-15 `B1Winch_DeepStrip`).
  - Headless (`s310_*.log`): gym 22/22, `Lvl_Offroad` 8/8 (FullThrottle5s 89.0 m, same as before), winch gym 8/8 PASS.
  Add the winch force (projected on the ground plane / wheel forward) to the net push in the rolling-resistance and belly-drag caps, so a belly-stuck jeep can be pulled out.
  **Done:** builds; a rig stuck in deep (dig-in script) moves under full pull at zero throttle, and the drag force is visible in telemetry.
  **User check:** —

### Debug + rigs

- [x] **S3-11** Rig winch support + metrics (2.5 h) _(requires S3-08)_ <!-- done: 2026-10-27 -->
  Result: rig `WinchAnchor` (actor with a `UB1WinchAnchorComponent`; attached at script start, error if out of reach), `WinchPull` per step, detach at finish. Rig line `tensionMax= tensionOsc= anchorDistMax= rest=`; thresholds `MaxTension`, `MaxTensionOscillation` (range in the last step), `MaxAnchorDistance`. New `B1Winch_FirmPull` (`B1Tests/Winch`, X 0 Y −50 m, tree `Tree_FirmPull` 20 m ahead): 12 s handbrake + full brake + full pull → dragged 11.8 m at 1 m/s, tensionMax 18.6 kN, PASS headless (`s311.log`). Firm rigs sit at endH ≈ −0.98 (rig 1 m above the floor).
  `AB1GymTestRig`: optional `WinchAnchor` actor (attached at script start, rest length = distance), `WinchPull` per input step. Metrics `tensionMax`, `tensionOsc` (tension amplitude over the last step), `anchorDistMax` and thresholds.
  **Done:** builds; the S3-08 rig uses the new fields and prints the metrics.
  **User check:** —

- [x] **S3-12** NetImGui "Winch" window + telemetry (2.5 h) _(requires S3-08)_ <!-- done: 2026-10-27 -->
  Result: `B1ImGui::DrawWinch` (`Debug/B1ImGuiWinch`, Debug → Winch) on the player pawn's winch: anchor or nearest anchor + distance, Attach nearest / Detach, length / max and tension / cap bars, rest, pull %, `Vehicle.State.Winching` on/off, a per-wheel table (net push, drag, slope pull) + belly drag, and typed fields for every `Winch|Config` float. Edits are live on the jeep; "Save to asset" copies them to the BP's component template (archetype) and saves `BP_B1Jeep`. New mud sim output `NetPush` per wheel (`UB1MudVehicleComponent::GetWheelNetPush`, what rolling resistance caps against; no host change). CVar `b1.Winch.Telemetry` → `B1Winch <vehicle> anchor= len= rest= tension= pull=` on `LogB1Physics` at 2 Hz while attached (from `AMudDriverPawn::LogTelemetry`); seen in the headless `B1Winch_FirmPull` run (`s312.log`).
  Debug → Winch: attached anchor, length / max, tension bar vs cap, pull %, `Winching` tag, per-wheel net push vs drag; live-edit of the winch params (typed fields, save to asset per `imgui.md`); Attach nearest / Detach buttons. CVar `b1.Winch.Telemetry`: `B1Winch <vehicle> anchor= len= rest= tension= pull=` to `LogB1Physics` at 2 Hz while attached.
  **Done:** builds; the telemetry line appears in a headless winch rig run.
  **User check:** the window reads well while winching; tune `MaxPullForce` live.

- [x] **S3-13** `ShowDebug B1Winch` (1 h) _(requires S3-08)_ <!-- done: 2026-10-27 -->
  Result: `Debug/B1WinchShowDebug` (category `B1Winch`), called from `AB1Jeep::DrawShowDebug`. Above the winch point (+1 m): anchor label, tension / cap kN, length / max m, rest, pull %; "not attached" otherwise. For the viewer's own pawn only, each anchor within 50 m of the winch: label, distance, hooked / in range (green) / out of range (grey). Builds; not run (user check).
  Over the jeep: tension / cap, length / max, pull. Over each anchor within 50 m: name, distance, in range or not. Same binding pattern as `B1VehicleShowDebug`.
  **Done:** builds.
  **User check:** `ShowDebug B1Winch` readable, toggles off.

- [x] **S3-14** Deep strip lane + anchors (1.5 h) _(requires S3-05, S3-07)_ <!-- done: 2026-10-27 -->
  Result: `Mud_DeepStrip` (X 10..30 m, Y −30, deep, 70 cm bottom, 300 cm ramps all round) in a gap cut by splitting `G_Floor` into four tiles; `Tree_DeepStrip` at X 40 m; `Tree_MediumSlope` / `Tree_DeepSlope` replace the plateau posts; `Spawn_4_DeepStrip` + label. Traces: floor 0, strip bottom −70 cm, ramps in between. Viewport capture checked; layout in ARCHITECTURE.md (Winch gym). ⚠️ The slope trees are ~48 m from the slope foot, past the 40 m cable, so slope winching starts from ~10 m up the slope.
  Deep (XL) patch with ramps in `Lvl_Gym_Winch`, `BP_B1WinchTree` ~10 m past the far ramp; trees at the S3-05 anchor spots on the medium and deep slopes.
  **Done:** viewport capture; ARCHITECTURE.md lists the anchors.
  **User check:** —

- [x] **S3-15** Winch rig suite (3 h) _(requires S3-06, S3-10, S3-11, S3-14)_ <!-- done: 2026-10-27 -->
  Result: 8/8 PASS headless (`s315_winch.log`), slope 8/8 (`s315_winchgym.log`), `Lvl_Gym_Mud` 22/22, `Lvl_Offroad` 8/8 (FullThrottle5s 89.0 m, unchanged). Rigs in `B1Tests/Winch`:
  - `Stability20/120` (strip X 15 m; 3 s full, 7 + 3 s full pull, zero throttle): tensionMax 21.1 kN, tensionOsc (last 3 s) 1.02 / 1.03 kN, peakOsc 0.64 / 0.65, same at both rates. Limits: tension 42 kN, tensionOsc 2 kN, peakOsc 0.7, not 0.5: the no-winch deep rigs (`B1Mud_DeepFullThrottle`, `SinkDeepStop`) show 0.655 from the drop into deep at script start.
  - `CableLimit` (firm lane, 38.3 m from `Tree_FirmPull`, 8 s at 50 % away): anchorDistMax 38.31 (limit 40.5). Trivial: rest = distance at attach, so the jeep can't move.
  - `CableSnatch` (measurement only, no thresholds): 3 s reverse toward the tree, then 50 % away. Hits the end at speed and overruns to 42.7 m (rest 38.3, max 40), since tension is capped at `MaxPullForce` even while the cable is being stretched. See backlog.
  - `DeepStrip` (dig in 3 s, then 20 s 50 % + full pull): dist 24.7, endH −0.99 (firm), belly peak 7.4 kN.
  - `SlopeMedium` / `SlopeDeep` (on the slope 15 m up from the foot, the trees are out of reach from the run-up; 50 % + full pull): endH 7.93 in 30 s / 8.09 in 45 s (plateau ≈ 8.1), tension hits the 40 kN cap.
  - `FirmPull` (S3-11).
  - `B1Winch_Stability20/120`: full pull against a jeep stuck in deep at `MaxFPS` 20 and 120: `tensionMax` ≤ cap + 5 %, `tensionOsc` non-growing, `peakOsc` ≤ 0.5.
  - `B1Winch_CableLimit`: drive away from the anchor at 50 %: `anchorDistMax` ≤ `MaxCableLength` + 0.5 m.
  - `B1Winch_DeepStrip`: dig in, then full pull + 50 %: `endH` back at firm height past the far ramp in ≤ 20 s.
  - `B1Winch_SlopeMedium`: from the run-up into the slope, full pull + 50 %: top in ≤ 30 s.
  - `B1Winch_SlopeDeep`: same on the deep slope, ≤ 45 s.
  **Done:** all PASS headless; slope and flat suites PASS.
  **User check:** —

### Backlog (found during the sprint)

- **Winch snatch overrun** (S3-15, decide at S3-16): tension is clamped to `MaxPullForce` (40 kN) in every case, so a jeep driving into a taut cable at ~7 m/s stretches it 4.4 m past rest (`B1Winch_CableSnatch`, 42.7 m vs the 40 m `MaxCableLength`). Options: cap only the drum's reel-in force and let a stretched cable hold with full stiffness (or a higher hold cap); or accept it (a slipping drum brake). → Accepted as drum slip (user, 2026-10-27); recorded in S4-01.

### Close

- [x] **S3-16** Mud + winch checkpoint playtest (user + agent, 1–2 h) _(requires S3-09, S3-12, S3-15)_ <!-- done: 2026-10-27 -->
  Result: you playtested and saved `BP_B1Jeep` winch defaults from the Winch window: `MaxPullForce` 60 kN (was 40), `MaxCableLength` 80000 cm (was 4000; trees hooked at 49–52 m in the playtest). The rig tension limit is now cap + 5 % = 63 kN. Headless with the tuned values: winch gym 16/16 (slope 8 + winch 8, `s316_winchgym.log`), `Lvl_Gym_Mud` 22/22, `Lvl_Offroad` 8/8. Slope winching now hits the 60 kN cap; medium reaches the plateau (endH 7.95), and so does deep (8.09). Snatch overrun 41.2 m (rest 38.3). ADR-007 → accepted. ✅ Sign-off: you asked to proceed after the playtest.
  You get stuck in the deep strip and on the medium and deep slopes, attach by cheat, and winch out and up from the car. Tune winch params and the hold curve live; the agent writes them back and re-runs S3-06 and S3-15.
  **Done:** both winch and slope suites PASS with the tuned values; ADR-007 → accepted.
  **User check:** sign-off: "mud + winch works". On foot starts only after this.

---

## Sprint 4 — On foot (Phase 2, part 2)

**Status:** refined 2026-10-27 after the Sprint 3 checkpoint; S4-14..S4-19 added 2026-10-28 after the S4-09 check (fallen tree). Start with `/work`.

**Goal.** Close Phase 2: the player gets out, carries the hook to a tree, gets back in and winches out; a log can be winched off a lane.

**Sprint-wide notes**
- ⚠️ Suites: `Lvl_Gym_Core` (8, replaces `Lvl_Offroad`), `Lvl_Gym_Mud` (22), `Lvl_Gym_Winch` (slope 8 + winch 8). Possession changes re-run all of them headless.
- ⚠️ Rigs with `bPossessWithPlayer` run one per round in PIE (single player 0).
- ⚠️ Agent checks for on-foot work go through rigs (S4-02 adds ability-input steps), not console cheats. Cheats are for you.
- ⚠️ Already in place, don't rebuild: `AB1PlayerController::OnPossess` accepts any pawn and sets `State.Avatar.InVehicle` (S2-03); the fuel components only act while the pawn is controlled; `IB1MudSimHost::SetMudKeepAwake` keeps a jeep awake on mud slopes (S3-03).
- User decisions (2026-10-27): `MaxCableLength` 80 m; the jeep is parked with the handbrake when you get out, but still slides on a muddy slope; snatch overrun accepted as drum slip.
- User decisions after the round 1 critique ([PLAN.md](PLAN.md), 2026-10-27):
  - On-foot interact goes through a Lyra-style interaction scan (S4-11, ADR-017).
  - At full cable length, a sliding jeep drags the character.
  - Getting in while carrying the hook stows it.
  - S4-08 adds a NetImGui "Abilities" panel.
- Design rules from the critique:
  - `State.Avatar.*` stays in `AB1PlayerController::OnPossess`, keyed on pawn type.
  - Enter/exit abilities are instant and thin: the controller does the swap, and the cheats call the same functions.
  - The character follows possession: possessing a jeep seats it there.
  - A carried hook is visual only and never reaches the sim.
- Out of scope: animations, hook-in-hand mesh, unpowered winch free-spool (powered reel-out is S4-13), character slowed in mud, swimming. If the sprint runs long, S4-09 (log) is the task to slip.
- User additions (2026-10-27): S4-13 (reel the cable in and out, detach on its own key, keyboard rebind) and S4-12 (cable lies on the ground and the mud). Both come before S4-07.

- [x] **S4-01** Winch housekeeping (0.5 h) <!-- done: 2026-10-27 -->
  Result: `BP_B1Jeep` already had `MaxCableLength` 8000 cm. `B1Winch_CableLimit` and `CableSnatch` moved to X 95.5 m, Y −50 m (77.8 m from `Tree_FirmPull`, on firm floor, endH −0.98); `CableLimit` kept as a cheap regression check, limit 80.5 m. Headless winch gym 16/16 PASS (`s401_winchgym.log`): CableLimit anchorDistMax 77.81, CableSnatch 82.69 (rest 77.79, 4.9 m drum slip). ADR-007 records the drum slip.
  `BP_B1Jeep` `MaxCableLength` 80000 → 8000 cm (user). `B1Winch_CableLimit` and `CableSnatch` were built for 40 m: move them ~78 m from `Tree_FirmPull` (floor allows it) or retire `CableLimit` if it stays trivial (rest = distance at attach). ADR-007: snatch overrun accepted as drum slip; close the open line.
  **Done:** winch gym 16/16 PASS headless with 80 m; ADR-007 updated.
  **User check:** —

- [x] **S4-02** Ability set + input config + rig steps (ADR-014) (4 h) <!-- done: 2026-10-27 -->
  Result:
  - `UB1AbilitySet` (`GAS/`, Lyra's `ULyraAbilitySet` minus attribute sets): abilities + input tag (into the spec's dynamic source tags) and effects, plus granted handles. Granted from the new `UB1GASSettings::AbilitySets` in `AB1PlayerState::BeginPlay` (authority).
  - `UB1InputConfig` (`Player/`): input action → `InputTag.*`. `AB1PlayerController::InputConfig` binds Started → `AbilityInputTagPressed`, Completed/Canceled → released, forwarded to `UB1AbilitySystemComponent`: it activates matching inactive specs, and active ones get input pressed/released + the replicated event (Lyra's logic, without the per-frame queue).
  - `UB1GameplayAbility_TestPing` → `GA_B1TestPing` logs `TestPing activated #n` on `LogB1Ability` (new). Tag `InputTag.Debug.TestPing`. Assets: `AbilitySet_B1Player` (`/Game/B1/GAS`), `InputConfig_B1Player`, `IA_B1TestPing`, `IMC_B1Abilities` (P) in `/Game/B1/Input`. `IMC_B1Abilities` was added to `BP_B1PlayerController`'s `DefaultMappingContexts`.
  - Rig: `FB1GymInputStep::AbilityInputTag` (pressed on step entry, released on the next step or at finish; errors without `bPossessWithPlayer`). Metric `abilities=` (ASC `AbilityActivatedCallbacks`), thresholds `Min/MaxAbilityActivations`. `B1Ability_TestPing` in `Lvl_Gym_Core` (`B1Tests/Ability`, origin): 3 presses with gaps, expects exactly 3.
  - Headless `Lvl_Gym_Core` 9/9 PASS (`s402_core.log`, abilities=3, FullThrottle5s 77.07 m = baseline). ADR-014 accepted.
  `UB1AbilitySet` (abilities with input tags + effects), granted once on the `AB1PlayerState` ASC. `UB1InputConfig` (input action → native `InputTag.*` in `B1GameplayTags`). `AB1PlayerController` forwards pressed/released to `UB1AbilitySystemComponent`, which activates matching specs. A no-op `GA_B1TestPing` bound to a key logs activation.
  Rig: `FB1GymInputStep::AbilityInputTag` (pressed at step start, released at end, through the player controller; needs `bPossessWithPlayer`). Rig `B1Ability_TestPing` checks the activation count.
  🔍 Research first: Lyra's `ULyraAbilitySet`, `ULyraInputConfig` and `ULyraAbilitySystemComponent::AbilityInputTagPressed`; the minimum we need without `ULyraInputComponent`.
  **Done:** builds; `B1Ability_TestPing` PASS headless; ADR-014 → accepted.
  **User check:** press the test key in PIE: the activation shows in the log.

- [x] **S4-03** `AB1Character` + on-foot input + rig move step (4.5 h) _(requires S4-02)_ <!-- done: 2026-10-27 -->
  Result:
  - `AB1Character` (`Player/`, abstract): capsule 35 × 90, `FirstPersonCamera` at +70 cm with pawn control rotation, yaw follows the controller. `GetAbilitySystemComponent` = the PlayerState's ASC. `PawnClientRestart` adds `OnFootMappingContext` at priority 1 (above the vehicle contexts, so W/S/A/D walk); `UnPossessed` removes it and clears `State.Avatar.OnFoot`. Mouse look goes through `AddControllerYaw/PitchInput` (template feel, legacy scales on); stick look writes deg/s straight into `RotationInput` (`StickYawRate` 300, `StickPitchRate` 165, Lyra's values).
  - `AB1PlayerController::OnPossess` also sets `State.Avatar.OnFoot` (1 for `AB1Character`).
  - Assets (duplicated from the template's `/Game/Input`): `IA_B1Move`, `IA_B1LookMouse`, `IA_B1LookStick`, `IA_B1Jump`, `IMC_B1OnFoot` (WASD + arrows up/down, Space, Mouse2D, gamepad sticks + face bottom) in `/Game/B1/Input`; `BP_B1Character` in `/Game/B1/Player`. `AB1GameMode::CharacterClass` (`BP_B1GameMode` → `BP_B1Character`; also set on `BP_B1GymGameMode`, which had saved None before the parent edit propagated).
  - Cheat `B1SpawnCharacter`: spawns `CharacterClass` 5 m ahead of the current pawn and possesses it.
  - Rig: `StartOnFootClass`, `FB1GymInputStep::Move`, metrics `pawn= avatar= charZmin=` (m above the rig), threshold `CharZminRange`. The capsule ignores the mud surface without any change (`B1MudSurface` blocks only WorldDynamic).
  - `B1OnFoot_WalkFirm` (`Lvl_Gym_Core`, origin): 29.2 m in 5 s, charZmin 0.020. `B1OnFoot_WalkDeep` (`Lvl_Gym_Winch`, X 2 m, Y −32 m): 41.3 m, endH 0.92, charZmin −0.669 (3 cm above the 70 cm bottom). Core 10/10, mud 22/22, winch 17/17 headless (`s403_full_*.log`).
  - User check (2026-10-27): walking and mud OK; pad pitch was inverted, flipped in `LookStick`.
  First-person `ACharacter`: capsule, camera, walk/look/jump with Enhanced Input, `IMC_B1OnFoot` with keyboard/mouse **and gamepad** bindings. `BP_B1Character` in `/Game/B1/Player/`. Implements `IAbilitySystemInterface` through the PlayerState ASC. Cheat `B1SpawnCharacter` (spawn in front of the jeep and possess) for testing before S4-06.
  ⚠️ Note: the capsule must walk on a patch's hard bottom, not its mud surface; check its collision against the mud channel/profile from `UB1MudSettings`.
  Rig:
  - `StartOnFootClass` (config): spawn that character at the rig and possess it instead of a vehicle.
  - `FB1GymInputStep::Move` (2D, pawn-relative; `AddMovementInput` on a possessed `AB1Character`).
  - Metrics `pawn=` (possessed class), `avatar=` (`State.Avatar.*`), `charZmin=` (lowest capsule bottom).
  - `B1OnFoot_WalkFirm` (`Lvl_Gym_Core`, 5 s forward: dist ≥ 10 m).
  - `B1OnFoot_WalkDeep` (`Lvl_Gym_Winch`, across `Mud_DeepStrip`: `charZmin` within 5 cm of the 70 cm hard bottom, reaches the far side).
  **Done:** builds; both walk rigs PASS headless.
  **User check:** walking and looking feel fine on keyboard and pad; walking through mud stands on the hard bottom.

- [x] **S4-04** Mapping-context swap on possession (1.5 h) _(requires S4-03)_ <!-- done: 2026-10-27 -->
  Result:
  - `AB1PlayerController::VehicleMappingContexts` / `OnFootMappingContexts`; `OnPossess` → `ApplyPawnMappingContexts` removes the other set and adds the pawn's (vehicle for `AWheeledVehiclePawn`, on foot for `AB1Character`, none otherwise) and logs `mapping contexts for <pawn>: vehicle|on foot|none` on `LogB1Vehicle`.
  - `BP_B1PlayerController`: `DefaultMappingContexts` = `IMC_B1Abilities` only; `IMC_Vehicle_Default`, `IMC_Vehicle_MouseLook` (was mobile-excluded) and `IMC_SteeringWheel` (was `bUseSteeringWheelControls`, now off) moved to `VehicleMappingContexts`; `IMC_B1OnFoot` in `OnFootMappingContexts`. The character's own context code (S4-03) is gone. No child BPs of the controller.
  - `AMudDriverPlayerController::OnPawnDestroyed` respawns only when the destroyed pawn is `VehiclePawn` (the possessed vehicle, null on foot). `APawn::Destroyed` unpossesses before `OnDestroyed` fires, so `GetPawn()` can't be used there.
  - Headless: core 10/10 (`B1OnFoot_WalkFirm` 29.17 m, log shows "on foot"), mud 22/22 (`s404_*.log`).
  - Touch controls no longer skip the vehicle mouse-look context (desktop only for now).
  `AB1PlayerController::OnPossess` removes the previous pawn's mapping contexts and adds the new one's (vehicle vs on-foot). The vehicle UI already hides on foot.
  ⚠️ Note: the template adds its contexts in `AMudDriverPlayerController::SetupInputComponent` (`DefaultMappingContexts`, `MobileExcludedMappingContexts`); move the vehicle ones to possession. `IMC_B1Abilities` stays in `DefaultMappingContexts` (ADR-014, S4-02). `OnPawnDestroyed` respawns and possesses a new jeep even if you're on foot: only do that when the destroyed pawn is the one possessed.
  **Done:** builds; core 9 and gym 22 PASS; `B1OnFoot_WalkFirm` still PASS (on-foot context added on possession).
  **User check:** —

- [x] **S4-05** Seat, engine off, parked on exit (3.5 h) _(requires S4-03)_ <!-- done: 2026-10-27 -->
  Result:
  - `AB1Jeep::DriverSeat` (scene component on the mesh, (−20, −40, 100)). `AB1PlayerController::OnFootCharacter` follows possession (`UpdateOnFootCharacter`): possessing an `AB1Jeep` seats it (spawned once from `AB1GameMode::CharacterClass`); possessing another character (cheat, rig) adopts that one and destroys the seated leftover. `AB1Character::SitIn` / `LeaveSeat` (hidden, no collision, `MOVE_None`, attached).
  - `AB1Jeep::SetParked`: Chaos `SetParked` (full handbrake torque on every wheel, no controller needed) + `EB1EngineCut::EngineOff` (movement's engine-cut flags; out of fuel is the other reason) + `Vehicle.State.EngineOff` + `SetParkSleep`. `PossessedBy` unparks. `IB1EngineLoadSource::IsEngineRunning` (default true) stops all fuel use, idle too (MIGRATION.md).
  - Measured creep: locked wheels slid 1.31 m in 10 s at 14 cm/s on the firm 20° slope, just above the sleep speed. Fix per the note: parked raises `SleepThreshold` to `ParkedSleepThreshold` (50 cm/s), so Chaos sleep holds it; mud slopes never sleep (keep-awake). `RefreshSleepThreshold` merges this with the winch/test-force keep-awake and only writes on a change.
  - `AB1Jeep`, `AMudDriverOffroadCar`, `AMudDriverPawn` exported (`MUDDRIVER_API`) for the rig. Rig: step `bParked`, metric `seated=`, threshold `MaxFuelUsed`. `B1Park_FirmSlope` (2 + 10 s parked, lastDisp 0.000 ≤ 0.05, asleep after 0.68 s) and `B1Park_MediumSlope` (10 s, slides 17.6 m ≥ 1), both `seated=1 fuelUsed=0.000`, folder `B1Tests/Park` in `Lvl_Gym_Winch`. Headless core 10/10, mud 22/22, winch 19/19 (`s405_*.log`).
  - Seat socket on `BP_B1Jeep`. The character is spawned once (class on `AB1GameMode`). While driving it is hidden, collision off, `MOVE_None` and attached to the seat.
  - The character follows possession: whenever the controller possesses a jeep, the character is seated in that jeep. Rig-spawned jeeps (`bPossessWithPlayer`) and respawned jeeps get it too, so exits always leave the possessed jeep.
  - Engine off outside: `Vehicle.State.EngineOff`, throttle cut through the same path as out-of-fuel (`SetFuelCutOff` → a general engine-cut reason), restart on enter. Fuel doesn't drain unpossessed already; verify, don't rebuild.
  - Parked on exit (user): handbrake on, released on enter. It must hold on firm ground and on the firm 20° slope, but still slide on a muddy slope.
  🔍 Research first: Chaos `bRequiresControllerForInputs` zeroes raw inputs without a controller, so a handbrake input won't stick on an unpossessed jeep; look at `SetParked` / `ParkingEnabled`. `FB1MudSimulation` already counts handbrake and parking as input (no brake release).
  ⚠️ Note: if the parked jeep holds on the medium slope (locked wheels out-grip `SlopePull`), fix it in the mud sim (portable), not in the host. Keep-awake must still work with no controller.
  ⚠️ Note: since ADR-016 (anti-slide off) nothing but tire friction and sleep holds a jeep on firm ground, and locked rear wheels may creep. Measure first, then set the threshold. If the creep is large, let Chaos sleep hold the parked jeep off mud (keep-awake only fires on mud slopes); don't add a hold force.
  Rig: `FB1GymInputStep::bParked` calls the same jeep function. Metric `seated=` (character attached to the rig's jeep).
  - `B1Park_FirmSlope` (mid firm slope, 10 s): `lastDisp` ≤ measured creep + margin, target 5 cm.
  - `B1Park_MediumSlope` (mid medium slope, 10 s): slides ≥ 1 m.
  **Done:** builds; park rigs PASS headless with `seated=1`; a headless run logs no fuel drain while parked.
  **User check:** —

- [x] **S4-11** Interaction scan + `GA_B1Interact` (ADR-017) (2.5 h) _(requires S4-03)_ <!-- done: 2026-10-27 -->
  Result:
  - `Interaction/` (new include path): `IB1Interactable` (actor or component), `FB1InteractionOption` (text, event tag, priority, point; the scan fills `TargetActor`), `UB1InteractionScanComponent` on `AB1Character` (sphere overlap, all object types, `Range` 250 cm, `ScanInterval` 0.1 s, only while player-controlled and not seated; priority then distance).
  - `UB1GameplayAbility_Interact` → `GA_B1Interact`: requires `State.Avatar.OnFoot`; `CanActivateAbility` runs a fresh scan and fails with nothing in range, so an empty press activates nothing; sends the best option's event (`HandleGameplayEvent`, target = option actor). `UB1GameplayAbility_TestPing` triggers on `Event.Interact.TestPing`.
  - Tags `InputTag.Interact`, `Event.Interact.TestPing`. Assets: `IA_B1Interact` (F, pad Y; pad X is vehicle throttle) in `IMC_B1Abilities`, `InputConfig_B1Player` + `AbilitySet_B1Player` entries.
  - Rig: `AB1TestInteractable` (`MudDriverTests`, query-only sphere, option "Ping") at (3, −1.5, 0.9) m; `B1OnFoot_Interact` (`Lvl_Gym_Core`, origin): walk up, press → abilities 2; walk back 8 m, press → still 2. Core 11/11 headless (`s411_core.log`). ADR-017 accepted.
  Added after the round 1 critique: the jeep door, the jeep front and the anchors all share one interact key, and S4-06/07/08 each needed the same "what's in range, which wins" logic. Needed before S4-06, not backlog.
  - `IB1Interactable::GatherInteractionOptions(Character, Options)`. `FB1InteractionOption`: text, trigger event tag, priority, interaction point. Lyra's `IInteractableTarget` / `FInteractionOption`, minus the per-option granted ability.
  - `UB1InteractionScanComponent` on `AB1Character`: overlap query at ~10 Hz within a range, keeps the best option (priority, then distance to the point).
  - `GA_B1Interact` on `InputTag.Interact`, gated by `State.Avatar.OnFoot`: sends the best option's event, and the target ability triggers from it (`AbilityTriggers`, GameplayEvent). Pad + key binding in `IMC_B1Abilities`.
  - ADR-017 (interaction options + one interact ability) → accepted when the rig passes.
  Rig: `AB1TestInteractable` (option "Ping" → event that triggers `GA_B1TestPing`). `B1OnFoot_Interact` (`Lvl_Gym_Core`): walk to it, press interact (`AbilityInputTag`), `abilities` = 2 (interact + ping); press again out of range: no change.
  **Done:** builds; `B1OnFoot_Interact` PASS headless; ADR-017 added.
  **User check:** —

- [x] **S4-06** Enter/exit vehicle + cheats (3.5 h) _(requires S4-04, S4-05, S4-11)_ <!-- done: 2026-10-27 -->
  Result:
  - `AB1PlayerController::ExitVehicle()`: `FindExitSpot` tries `AB1Jeep::DriverDoor` (mesh (−20, −230, 100)), the passenger side (+230), behind (−350) and in front (+380). Each spot: a Pawn-channel ground trace (ignores mud, lands on the hard bottom), then a capsule room test. The vehicle body blocked spots at ±170. Then park, `LeaveSeat`, teleport, possess, control yaw = jeep yaw. Logs the ASC tags on foot: `Vehicle.State.EngineOff` only (InMud/Winching drop on the controller change). `EnterVehicle(AB1Jeep*)` possesses the free jeep (seats + unparks).
  - `UB1GameplayAbility_ExitVehicle` → `GA_B1ExitVehicle` (`InputTag.Interact`, requires InVehicle); `UB1GameplayAbility_EnterVehicle` → `GA_B1EnterVehicle` (triggered by `Event.Interact.EnterVehicle`, target = jeep, requires OnFoot). Both in `AbilitySet_B1Player`. `AB1Jeep` is an `IB1Interactable`: "Enter" (priority 10) at the door while nobody drives.
  - `UB1AbilitySystemComponent::AbilityInputTagPressed` now checks `CanActivateAbility` for every spec before activating any, so the press that exits can't also interact on foot and re-enter.
  - Parking also clears the driver input (`UB1VehicleMovementComponent::ClearDriverInput`): unpossessed, Chaos replays `ReplicatedState`, so the last throttle would stay on.
  - Cheats `B1ExitVehicle`, `B1EnterVehicle` (nearest free jeep).
  - User check round 1 (2026-10-27): pad X throttle removed from `IMC_Vehicle_Default` (RT stays) and interact moved to pad X (was Y). Exit failed dug in on the deep slope: every spot was "blocked by HardBottom", because the ground trace started inside the 1 m slab. Exit now never fails: trace from +4 m, try each spot at lifts of 2 / 30 / 60 cm, then `FindTeleportSpot`, last resort 1.5 m above the door (warning). Rigs `B1OnFoot_ExitDeepSlope` (mid deep slope) and `B1OnFoot_ExitDeepStrip` (X 20 m, Y −30 m) in `Lvl_Gym_Winch`: 4 s full throttle (sink 70 on all wheels), exit, enter, both PASS.
  - Rig: step `ExpectAvatar` (pawn + tag checked at step end, `avatar@<steps>` on fail), metric `fuelOutside=` / `MaxFuelOutside`; the rig's `bParked` is edge-triggered so it doesn't unpark a jeep the player left. `B1OnFoot_ExitEnter10` (`Lvl_Gym_Core`, X −15 m, Y −30 m): 10 × exit/enter, abilities 30, `fuelOutside=0.0000`, ends seated; no errors or ensures. Core 12/12, mud 22/22, winch 19/19 headless (`s406_*.log`).
  - Controller does the swap: `AB1PlayerController::ExitVehicle()` puts the character at the driver door (fallback spots if blocked), unhides it, parks the jeep, cuts the engine and possesses the character. `EnterVehicle(AB1Jeep*)` possesses the jeep, which re-seats the character (S4-05).
  - `State.Avatar.*` stays in `OnPossess`, keyed on pawn type (ADR-004).
  - `GA_B1ExitVehicle` (`InputTag.Interact`, gated by `State.Avatar.InVehicle`) and `GA_B1EnterVehicle` (triggered by the jeep door's "Enter" option, S4-11) are instant: validate, call the controller, end in the same frame. Cheats `B1ExitVehicle` / `B1EnterVehicle` call the same functions.
  - On exit, check what happens to `Vehicle.State.*` on the ASC (fuel/mud components drop their state when the controller changes, ADR-004) and log it.
  Rig `B1OnFoot_ExitEnter10`: 10 × exit (interact in the jeep) and enter (interact at the door). After each step it checks `pawn=` and `avatar=`. Fuel must be unchanged while outside, with no errors or ensures.
  **Done:** builds; rig PASS headless; all suites PASS.
  **User check:** get out and in on firm ground, in mud and on the slope; camera and controls switch cleanly; the parked jeep stays put on firm ground and slides on the muddy slope.

- [x] **S4-13** Winch reel in/out + detach key + keyboard rebind (3 h) _(requires S4-06)_ <!-- done: 2026-10-27 -->
  Result:
  - Signed pull end to end: `UB1WinchComponent::Pull` / `SetPull` −1..1, new `ReelOutSpeed` (`Winch|Config`; `BP_B1Jeep` 300 cm/s = its `ReelInSpeed`). `FB1WinchSimInput` carries `ReelOutSpeed` and `MaxCableLength`; the sim grows the rest length up to the max, ignoring the stall. Winch window (Reel-out field + save, pull shows in/out/hold), `ShowDebug B1Winch` and telemetry show signed pull.
  - Reel-out damping: damping on body speed snatched a descending jeep in a loop (a steady 3 m/s read as +60 kN). During payout it now damps the stretch rate. Tried on reel-in too: every pull stepped to 60 kN (was 36–38 kN), so reel-in keeps body-speed damping. ADR-007 amended.
  - Input: `IA_B1WinchToggle` renamed `IA_B1WinchDetach`; `AB1Jeep::WinchToggleAction` → `WinchDetachAction` (calls `Detach` only). `IMC_Vehicle_Default` now: pull Up / D-pad Up, pull with Negate Down / D-pad Down, detach Q / pad Y. Removed from it: arrow Up (throttle), Down (brake), Left/Right (steering), E (winch pull). `IMC_B1Abilities`: interact F → E (pad X stays). Pad Y and Q had no template bindings.
  - Rig: `WinchPull` −1..1; thresholds `MaxLastStepSpeed`, `MinLastStepTension`, `MaxRestLength`; metrics `tensionMinLast`, `vmaxLast`.
  - `B1Winch_ReelOutSlope` (X 50 m, Y 20 m on the medium slope, `Tree_MediumSlope`): 2 s hold, 4 s reel-out, 3 s measured; vmaxLast 3.02 ≤ 3.3, tensionMinLast 3938, 22.2 m down, rest 16.6 → 38.1 m. The first seconds peak at 5.1 m/s (start-up overshoot, accepted by the user). Script shortened from 15 s: at 300 cm/s it would run off the slope.
  - `B1Winch_ReelOutLimit` (X 0, Y −55 m, firm, `Tree_FirmPull`, braked): 30 s reel-out (90 s not needed at 300 cm/s), rest 80.00.
  - Headless: winch gym 23/23 (`s413_winch3.log` + `s413_reelout2.log`; reel-in rigs match their old numbers), core `B1OnFoot_ExitEnter10` PASS (`s413_core.log`). Mud and full core not rerun (not touched); run them with the full suite.
  Added mid-sprint (user, 2026-10-27): the winch can only shorten the cable, and S4-07 needs the final in-car controls before it removes attaching from the seat.
  - **Reel-out.** The pull input becomes signed, −1..1: positive reels in as today; negative grows the rest length at `ReelOutSpeed` (new, `Winch|Config`, default = `ReelInSpeed`) up to `MaxCableLength`.
    - Reel-out isn't limited by the drum stall, so a jeep held on a slope descends under control at reel speed.
    - Rigs, the Winch window and telemetry show signed pull.
    - `Vehicle.State.Winching` stays reel-in only.
  - **Input (user's layout).**
    - Pad: D-pad Up reels in, D-pad Down reels out, Y detaches.
    - Keyboard: W/S/A/D drive, Arrow Up reels in, Arrow Down reels out, Q detaches, E interacts (moves from F, both pawns). Arrow Left/Right do nothing in the vehicle.
    - `IA_B1WinchPull` gets both directions (Negate modifier on the "out" keys). `IA_B1WinchToggle` → `IA_B1WinchDetach` (detach only; attaching from the seat ends here, ADR-007). Pad X stays interact.
    - Remove any template bindings on those keys in `IMC_Vehicle_Default` (arrow drive, Y/Q/E if used); list what was removed in the result.
    - `B1WinchAttachNearest` and the Winch window's Attach button stay as debug until S4-07 gives an on-foot attach.
  - ADR-007 amendment: reel-out, signed pull, detach key.
  Rig: `WinchPull` accepts −1..1.
  - `B1Winch_ReelOutSlope` (medium slope, hooked to `Tree_MediumSlope`, full reel-out 15 s): the jeep moves down the slope at ≤ `ReelOutSpeed` + 10 %, tension > 0 throughout (controlled descent, not free fall).
  - `B1Winch_ReelOutLimit` (firm lane, slack cable, reel out 90 s): `rest` ≤ `MaxCableLength`.
  **Done:** builds; both rigs PASS headless; winch gym and core suites PASS; IMC lists match the layout above.
  **User check:** on the medium slope, hook a tree, back down on reel-out, then pull back up; detach on Y / Q; E gets you in and out; no leftover template action fires on the arrows, Q, E or Y.

- [x] **S4-12** Cable lies on the ground and the mud (3 h) _(requires S4-13)_ <!-- done: 2026-10-27 -->
  Result:
  - Research: `UCableComponent` collision sphere-sweeps each free particle every substep (50 Hz) on the component's own object type and response container, with no owner ignore. So the filter is by type: `UB1WinchComponent::SetupCableCollision` sets object WorldDynamic (mud surfaces block only WorldDynamic), QueryOnly, blocks WorldStatic, WorldDynamic and `UB1MudSettings::MudObjectChannel`, ignores the rest (the jeep is `ECC_Vehicle`, the character Pawn). No trace-drape fallback needed.
  - Segments sized for `MaxCableLength`: `CableSegmentLength` 100 cm (replaces `CableSegments`), so 80 segments on `BP_B1Jeep`. New `CableSolverIterations` (16), `bCableCollision`, `CableCollisionFriction` (0.5), all `Winch|Visual`. Cost not measured: ~4000 sphere sweeps/s per 80 m cable, game thread (prototype first).
  - Found on the way (S4-13 bug): an attached jeep sitting still went to Chaos sleep despite `SleepThreshold` 0, and a sleeping body skips the physics step, so reel-out stopped at 54 m. `SetWinchInput` now wakes the body while pull ≠ 0 (reel-in on a slack cable had the same hole).
  - Simulate captures on `Lvl_Gym_Winch` with three temporary `StartAnchor` jeeps (deleted after): slack reel-out across `Mud_DeepStrip` lies on the floor and on the mud surface, and at 80 m piles into loops; taut cable up the medium slope is straight, identical across two frames. The firm-floor-only spot (X −5 m, Y −65 m) isn't streamed in Simulate, so the deep-strip capture covers firm floor on both sides of the patch.
  - Headless winch gym 23/23 (`s412_winch.log`), same numbers as S4-13. Core and mud not rerun (untouched).
  Added mid-sprint (user, 2026-10-27): the cable has no collision, so a slack cable sags through the floor and into the mud, and from the car you can't see whether you're hooked. It has to rest on whatever is under it: the ground, the mud surface (not the hard bottom), slopes and kerbs. It must never touch the jeep's or the character's physics.
  🔍 Research first, in this order:
  1. `UCableComponent` collision (`bEnableCollision`, `CollisionFriction`). Check:
     - which channel/object type its particle sweeps use, and whether that hits `B1MudSurface` (blocks only WorldDynamic, S4-03);
     - that it stays stable with the 16 solver iterations + stiffness from S3-07;
     - how many segments an 80 m cable needs to lie flat (≈ 1 per m) and its game-thread cost.
  2. Fallback: a drape built from traces. Sample the anchor–winch line, trace down on a channel that hits ground and mud surface, lift the slack part onto the hits, and keep the taut part straight. Draw it with the existing cable or with spline mesh segments.
  ⚠️ Note: the cable may hit the jeep and the character only if they're on the trace channel; ignore both owners, so a cable over the bonnet doesn't snag.
  ⚠️ Note: with reel-out (S4-13) the cable can be slack up to 80 m, so a long slack cable lying in a heap or line on the ground is the normal case, not an edge case.
  Visual check through Simulate captures on `Lvl_Gym_Winch`, all hooked by `StartAnchor`:
  - a slack cable across firm floor;
  - a slack cable across `Mud_DeepStrip` (lies on the mud surface, not the 70 cm bottom);
  - a taut cable up the medium slope (straight, no jitter).
  **Done:** builds; the three captures show the cable on the surface; winch rigs still PASS (visual only, no sim change).
  **User check:** from the driver's seat you can see the cable lying on the ground and the mud and tell at a glance that you're hooked; reel in and watch it lift and straighten.

- [x] **S4-07** Winch hook carry (4.5 h) _(requires S4-06, S4-12, S4-13)_ <!-- done: 2026-10-27 -->
  Result:
  - `EB1WinchHook` stowed / carried / attached on `UB1WinchComponent`, derived from `Anchor` / `HookCarrier`. `TakeHook(Hand)`, `DetachToCarrier(Hand)`, `AttachTo` from any state (a carried hook keeps its cable and retargets the end), `Detach` stows from any state. Carried sends no sim input. The cable length grows with the walk-out distance and never shrinks, so walking back leaves slack. `GetCurrentLength` measures to the anchor or the hand. `FindCarriedBy(actor)` / `FindAttachedTo(anchor)` iterate winches, so the hook state has one owner.
  - Leash in `TickCarriedHook`: past `MaxCableLength`, the carrying `ACharacter` is swept back onto the radius (slide on a blocking hit) and loses its outward velocity.
  - `AB1Character::HookHand` (on the camera, (40, 25, −35)); S4-08 can tune it.
  - Options: the jeep adds "Take hook" / "Return hook" at the winch point (priority 10 like Enter; door and winch are ~3.4 m apart, so the nearer one wins). `UB1WinchAnchorComponent` is an `IB1Interactable`: "Attach" when the character carries a hook in reach, "Detach" when a winch is hooked there and the hands are free.
  - Abilities: `UB1GameplayAbility_WinchHook` base + TakeHook / ReturnHook / AttachHook / DetachHook (one file pair), triggered by `Event.Interact.TakeHook|ReturnHook|AttachHook|DetachHook`, gated by OnFoot. BPs `GA_B1TakeHook`, `GA_B1ReturnHook`, `GA_B1AttachHook`, `GA_B1DetachHook` in `AbilitySet_B1Player`.
  - `AB1PlayerController::UpdateOnFootCharacter` stows a carried hook before seating, so every path into a jeep stows it.
  - Rig: step `TeleportPawnTo` / `bTeleportToVehicle` + `TeleportOffset` (target's yaw frame, faces the target). Move steps now drive the character the player got out as (`bPossessWithPlayer`). The rig always takes the vehicle's winch for pull. Winch metrics print with `WinchAnchor` or once the hook left the drum, plus `hook=` and `carryMax=`. Thresholds `MaxCarryDistance`, `ExpectHook`.
  - Rigs in `Lvl_Gym_Winch` (`B1Tests/OnFoot`, firm floor, `Tree_FirmPull`): `B1OnFoot_HookCarry` (5, −45 m): exit, walk to the front, take, teleport by the tree, attach (rest 13.66 m), teleport to the door, enter, full pull 5 s: the jeep moves 14.8 m to rest 1.50, abilities 7, hook=Attached. `B1OnFoot_Leash` (−30, −45 m): take, walk 30 s: carryMax 80.04 ≤ 80.5. `B1OnFoot_EnterWithHook` (−15, −45 m): take, enter: hook=Stowed, abilities 5.
  - Headless: winch gym 26/26 (`s407_winch.log`, S3-15 and reel rigs match S4-12 within 1 kN), core `B1OnFoot*` 3/3 (`s407_core.log`). Mud not run (untouched).
  - Hook states on `UB1WinchComponent`: stowed / carried / attached.
    - **Carried** is visual only: the cable runs to the character's hand, and no `FB1WinchSimInput` anchor is sent, so there is no tension.
    - **Attached** works as today (rest length = distance at attach).
  - Leash on the character side: past `MaxCableLength` (80 m) from the winch point, the character is pulled back onto that radius. A jeep sliding away therefore drags the character (user).
  - Interaction options (S4-11), each triggering its own small ability:
    - Jeep front: "Take hook" when stowed, "Return hook" when carried.
    - Anchor: "Attach" when carried, "Detach" when this anchor is hooked; the hook goes back to carried.
  - Getting in while carrying stows the hook (user).
  - In-car controls are final after S4-13 (reel in/out, detach on Y / Q). `B1WinchAttachNearest` and the Winch window's Attach button stay as debug.
  - The carried cable uses the S4-12 drape between the winch point and the hand.
  Rig: step `TeleportPawnTo` (actor) for long trips only. Metrics `hook=` (state) and `carryMax=` (peak hand-to-winch-point distance).
  - `B1OnFoot_HookCarry`: exit, take, teleport by the tree, attach, teleport back, enter, full pull: the jeep moves toward the tree.
  - `B1OnFoot_Leash`: take, walk away 30 s (`Move`): `carryMax` ≤ 80.5 m.
  - `B1OnFoot_EnterWithHook`: take, enter: `hook=stowed`.
  **Done:** builds; the three rigs PASS headless; S3-15 rigs still PASS.
  **User check:** carry the hook up the slope to the tree, attach, walk back, get in, winch up; on the medium slope, walk out to full length and feel the sliding jeep drag you.

- [x] **S4-08** Interaction prompt, cable from hand, debug panels (3 h) _(requires S4-07, S4-12)_ <!-- done: 2026-10-27 -->
  Result:
  - `UB1InteractionPromptWidget` (`Interaction/`): a code-only `UUserWidget` (canvas + text built in `Initialize`, no WBP). Its tick shows `[keys]  <best option text>` from the possessed character's `UB1InteractionScanComponent`, the same list `GA_B1Interact` uses. Keys come from `QueryKeysMappedToAction` on the interact action (`InputConfig` entry for `InputTag.Interact`): first keyboard key and first pad button; face buttons show as Xbox letters (A/B/X/Y, user). `AB1PlayerController::BeginPlay` adds it on local controllers (z-order 1). PIE capture: "[E / X]  Take hook" at the jeep front.
  - Cable from hand: `AB1Character::HookHand` (S4-07, on the camera) unchanged.
  - Winch window: uses the pawn's winch, or on foot the winch whose hook you carry; shows the hook state, the carrier and a "Carry m" bar vs `MaxCableLength`. `ShowDebug B1Winch`: "hook carried by X" + "carry a / max m", or "hook stowed". Anchors are drawn for the viewer's own winch, which now also means a hook you carry.
  - NetImGui Debug → Abilities (`Debug/B1ImGuiAbilities`): pawn + class, ASC avatar, granted specs (class, input tag, active count in green, pressed), owned tags with counts, the scan's options (text, event, priority, distance, target; best in green). Read-only, no tunables.
  - The panel can't be captured: NetImgui draws only in its remote server app, so that's the user check. Builds; headless `B1OnFoot*` core 3/3 and winch 6/6 with the widget on every player controller (`s408_*.log`).
  - Minimal UMG prompt showing the scan's best option ("Enter", "Take hook", "Attach", "Return hook", "Detach") and its key, read from `UB1InteractionScanComponent`, so it can't disagree with what interact does.
  - Cable end follows a hand socket on the first-person camera.
  - Winch window and `ShowDebug B1Winch` show the hook state and carry distance vs `MaxCableLength`.
  - NetImGui "Abilities" panel (Debug → Abilities, user): granted specs with input tags, active abilities, loose tags on the ASC, current avatar and possessed pawn, the scan's options in range.
  **Done:** builds; the panel opens in PIE (capture).
  **User check:** you always know what the interact key will do; the debug views read the carry state and the abilities panel helps while testing.

- [x] **S4-09** Winchable log (3 h) _(requires S4-01)_ <!-- done: 2026-10-28 -->
  Result:
  - Research: option 1 wins. Chaos runs vehicle sims in parallel but applies forces in a single-threaded `ApplyDeferredForces` pass afterwards (`FChaosVehicleManagerAsyncCallback::OnPreSimulate_Internal`). `FB1VehicleSimulation` overrides it to give the log exactly −force + torque at the anchor point in the same step. `UpdateWinch` reads the log's pose and velocity from its handle (`FB1WinchSimInput::AnchorProxy`, `AnchorPointLocal`) and damps on relative closing speed. ADR-007 amended (replaces the async-tick "Log" line).
  - `AB1WinchLog` (`Winch/`): simulating `Trunk` root (PhysicsActor) + `Anchor`. `UB1WinchAnchorComponent::GetSimulatingBody`, and `EndPlay` unhooks winches. `BP_B1WinchLog` (`/Game/B1/Winch`): cylinder, 1000 kg, anchor at one end. `Log_FirmLane` in `Lvl_Gym_Winch` (X 57.5 m, Y −57 m, scale 0.5 × 0.5 × 5 = 50 cm × 5 m; the root's scale is the actor scale, so a placement at scale 1 gives a 1 m cylinder).
  - Bug found on the way: the first anchor offset used `GetRelativeTransform`, which includes the log's scale. The body frame has none, so the sim saw the anchor 1.9 m further out and stepped to the cap. The offset is now unscaled.
  - Rig: threshold `MinAnchorTravel`, metric `anchorTravel=`. `B1Winch_Log` (X 40 m, Y −57 m, braked, full pull 12 s): breakaway 17 kN, then a steady ~6.5 kN slide at reel speed (3 m/s), tensionMax 19.1 kN (log meeting the bumper), anchorTravel 11.71 m, jeep moved 0.42 m.
  - Headless: winch gym 27/27 (`s409_winch.log`; tree rigs unchanged, e.g. FirmPull 36 097 N), core 12/12 (`s409_core.log`), mud 22/22 (`s409_mud.log`).
  - Tuning note: a heavier log would out-pull the braked jeep's grip (FirmPull drags a braked jeep at well under the cap), so the jeep would slide instead. 1000 kg ≈ a 50 cm × 5 m green trunk.
  `BP_B1WinchLog`: simulating trunk mesh with `UB1WinchAnchorComponent`; it gets the opposite cable force. Log lane in `Lvl_Gym_Winch` (not built in S3-14). Pick mass and friction so the 60 kN cap moves it, but not trivially. Rig `B1Winch_Log`: braked jeep, full pull, the log moves ≥ 3 m toward the jeep, `tensionMax` ≤ 63 kN.
  🔍 Research first, in this order:
  1. Apply the opposite force to the log's body handle from `FB1VehicleSimulation::UpdateWinch` in the same physics step: exactly equal and opposite, no cross-thread read.
  2. The log's `AsyncPhysicsTickActor` reading the published tension.
  3. Game-thread `AddForce` on the log.
  Amend ADR-007 ("Log") if 1 wins.
  ⚠️ Note: slip candidate if the sprint runs long (it doesn't block the on-foot loop); Phase 2's done-when then waits for it.
  **Done:** rig PASS headless.
  **User check:** winch the log off the lane; it slides and rolls believably.
  ❌ User check (2026-10-28): not accepted.
  - Driving into or over the log rolls it; it should stay still.
  - The cable hooks to the end of the log; it should pull from the middle of the trunk.
  - Reeling pulls the jeep toward the log with no way to hold it.
  Replanned as the fallen tree, S4-14..S4-19. The in-step anchor force stays.

### Playtest feedback (S4-09, 2026-10-28)

Plan: [plans/2026-10-28-fallen-tree.md](plans/2026-10-28-fallen-tree.md) (critique round 1 + revision 1). Scenario: VISION.md → "Fallen tree". Order: S4-14 → S4-15 / S4-17 (parallel) → S4-16 → S4-18 → S4-19 → S4-10.
- User decisions:
  - Log: frozen until pulled (picked over a stump hinge, an always-simulated heavy log and our own planar solver).
  - Handbrake: a normal handbrake while driving, plus a standstill hold while the hook is attached, so the winch pulls heavy objects. Not scaled on mud (see how it plays).
  - Pad: handbrake moves from LB to B; pad A no longer brakes (LT does).
  - Hints show both keys, like the prompt.
  - Detach only on foot.
  - The user creates `PM_B1Log` by hand. For the band material, the agent checks MCP first and asks if it can't.
- ⚠️ **Physics first (user).** The log moves by Chaos alone: no hook-position rules, no scripted motion. Hooking at the middle probably won't open a passable gap, and that's expected: the player has to work out where to hook. Tuning touches only physical knobs (mass, friction, root ball). An authored root-end drag or standstill force needs the user's OK.
- ⚠️ New thresholds are measured first, then held (as S4-05). Feel wins at the user check.
- Out of scope: ramming damage (Phase 4), the jeep shoving a log the winch is pulling free, hook-position rules, scripted motion, stump hinge, planar solver, cutting the tree, hook or strap animation, last-used-device key display, several hooks.

- [x] **S4-14** Handbrake + winch hold, no in-car detach (3 h) <!-- done: 2026-10-28 -->
  Result:
  - `FB1VehicleSimulation::UpdateHandbrakeHold` after `UpdateWinch`: −(winch + gravity) on the wheel-normal ground plane, plus −m·v/0.1 s drift, capped at `HandbrakeHoldForce`, `AddForce` at the COM. Active with handbrake ≥ 0.5, attached, ≥ 3 wheels down, ground speed < `HandbrakeHoldSpeed`. Winch + hold → `Mud.SetExternalForce`. `UB1WinchComponent::HandbrakeHoldForce` 80 kN / `HandbrakeHoldSpeed` 30 cm/s (`Winch|Config`) ride `FB1WinchSimInput`; output `HoldForce` / `bHoldActive`.
  - Found: `UChaosVehicleSimulation::VehicleInputs` is only filled with network prediction on (reads 0 here), so the hold reads the step's `InputData.PhysicsInputs.NetworkInputs.VehicleInputs`. Mud's idle check reads the same member: backlog.
  - Found: brake at standstill = Chaos reverse throttle, so brake + handbrake creeps back ~0.2 m/s against the hold. The rigs use the handbrake alone (your "hold B").
  - `IMC_Vehicle_Default`: `IA_Handbrake` Space + `Gamepad_FaceButton_Right`; removed `IA_Handbrake` LB, `IA_Brake` pad A, `IA_B1WinchDetach` Q / pad Y. `IA_B1WinchDetach` deleted (only `BP_B1Jeep` referenced it, resaved). `AB1Jeep::WinchDetachAction` + binding removed.
  - Debug: `ShowDebug B1Winch` "hold x / 80 kN active|off", telemetry `hold=<N>(on|off)`, Winch window hold bar + typed fields + Save to asset.
  - Rig: step `bWinchDetach`, threshold `MinTension`, metric `holdMax`. `B1Winch_HandbrakeHold` (X 0, Y −50 m, `Tree_FirmPull`, handbrake + full pull 10 s): lastDisp 0.023, tensionMax 60000, holdMax 60324. `B1Winch_HandbrakeDetach` (+1 s handbrake + detach): lastDisp 0.014.
  - Changed rigs: `B1Winch_FirmPull` dropped its handbrake (brake 1 only) to stay a drag rig: 36.1 → 29.2 kN. `B1Winch_Log` (hold on now: 27.8 kN, anchorTravel still passes; retired in S4-17) and `ReelOutLimit` (jeep held, rest 80.00) still pass. Other tree rigs within 1 kN (DeepStrip 34.5 → 33.6, Stability120 38.4 → 39.0).
  - Headless: winch 29/29 (`s414_winch.log`), mud 22/22 (`s414_mud.log`), core 12/12 (`s414_core.log`). ADR-007 amendment marked built.
  - User check round 1 (2026-10-28): B then winch held, but winch then B slid. The speed gate disabled the hold while the jeep followed the cable at 3 m/s. Now above `HandbrakeHoldSpeed` the hold cancels the cable pull only (no gravity or drift term), so the handbrake stops the jeep; below it, the full hold. Rig `B1Winch_HandbrakeLate` (same spot): pull 3 s, B + pull 3 s, B + pull 5 s measured: dragged 9.4 m, stopped 0.4 s after B, lastDisp 0.000 at 60 kN. Winch 30/30 (`s414_winch2.log`).
  - **Input layout (user).** `IMC_Vehicle_Default` today (read 2026-10-28): `IA_Handbrake` on Space and pad LB; `IA_Brake` on S, LT and pad A; pad B free. Change it to:
    - `IA_Handbrake` on Space + pad B (`Gamepad_FaceButton_Right`), with the LB binding removed;
    - the pad A binding on `IA_Brake` removed (LT stays).

    `AMudDriverPawn` already binds the handbrake (Started/Completed → `SetHandbrakeInput`). Space stays jump in `IMC_B1OnFoot` (separate context).
  - **Hold.** Without the hook it's a normal Chaos handbrake. While the hook is attached, it adds a standstill hold in `FB1VehicleSimulation`. It's active when all of these are true:
    - handbrake input on;
    - hook attached;
    - ≥ 3 wheels in contact;
    - ground speed < `HandbrakeHoldSpeed` (30 cm/s).

    Hold = −(this step's winch force + gravity along the ground), on the ground plane, plus a drift-killing velocity term. It's capped at `HandbrakeHoldForce` and applied at the COM, and it never pushes. Computed right after `UpdateWinch` in the same step, never from a cached force, and zero in the step the anchor clears. `HandbrakeHoldForce` (`Winch|Config`, default 80 kN) and `HandbrakeHoldSpeed` ride `FB1WinchSimInput`. Winch + hold go to `Mud.SetExternalForce` as one force.
  - **No in-car detach.** Remove `IA_B1WinchDetach` from `IMC_Vehicle_Default`, along with `AB1Jeep::WinchDetachAction` and its binding. Delete the IA asset if nothing else references it. Cheat `B1WinchDetach` and the Winch window's Detach stay.
  - **Debug.** `ShowDebug B1Winch`, the Winch window and the `B1Winch` telemetry show `hold=<N>` and whether it's active. The Winch window gets typed fields for the two new values plus "Save to asset" (imgui.md).
  - ADR-007 amendment (handbrake hold, no in-car detach; supersedes the S4-13 detach key).

  ⚠️ Note: Chaos's handbrake only acts on wheels with `bAffectedByHandbrake` (rear). That's fine: the hold does the rooting, not the wheels.
  ⚠️ Note: mud isn't scaled. The hold works at full strength on mud too (user, 2026-10-28: see how it plays).

  Rig: step `bHandbrake` already exists.
  - `B1Winch_HandbrakeHold` (`Lvl_Gym_Winch`, firm, hooked to `Tree_FirmPull` by `WinchAnchor`): braked + handbrake, full pull 10 s → `lastDisp` ≤ 0.3 m, tension reaches the stall (≥ 95 % of `MaxPullForce`).
  - `B1Winch_HandbrakeDetach`: the same, then a detach step while holding at the cap → jeep `lastDisp` ≤ 5 cm over the next 1 s (no phantom shove). Needs a rig detach step (`bWinchDetach`) if none exists.
  - The existing tree rigs (no handbrake) are unchanged within 1 kN.

  **Done:** builds; both rigs PASS headless; winch, mud and core suites PASS; the IMC list matches the layout above.
  **User check:** hook a tree, hold B and reel in: the jeep stays put; let go of B: the jeep gets pulled; Y / Q do nothing in the jeep; A doesn't brake; B / Space work as a normal handbrake while driving.

- [x] **S4-15** Anchor areas + pull at the trunk axis (3 h) <!-- done: 2026-10-28 -->
  Result:
  - `UB1WinchAnchorComponent`: `AreaStart` / `AreaEnd` / `TrunkRadius` (`Winch|Anchor`), points in the component's frame **without scale**; both zero = the old single point (the log until S4-17). `GetAreaPoint`, `GetAreaAlphaClosestTo`, `GetAreaAlphaClosestToRay` (for S4-16), `GetClosestAreaPoint`, `GetAreaAxis`, `GetSurfacePoint`. `FindNearest`, the option point and `IsInReach` use the closest area point.
  - `UB1WinchComponent::AttachTo(Anchor, AreaAlpha)` (negative = point nearest the winch); `GetHookPoint` / `GetAnchorAlpha`. Sim input, the S4-09 body-frame offset, `GetCurrentLength` use the hook point. Cable `EndLocation` follows the strap surface facing the winch every tick (end component's space, scale included). Cable also blocks PhysicsBody.
  - Strap: `UStaticMeshComponent` from `StrapMesh` (engine cylinder default), `StrapWidth` 15, `StrapClearance` 2, no collision, attached KeepWorld to the anchor, destroyed in `ReleaseAnchor`. `MI_B1WinchStrap` (duplicate of the cable MI, color 0.015 grey) on `BP_B1Jeep`'s `StrapMaterial`.
  - `AB1WinchTree` defaults: anchor at 80 cm, area −40..+80 (40–160 cm), radius 25 (trunk 0.5 scale cylinder = 50 cm). `BP_B1WinchTree` inherits them.
  - Callers: `StartAnchorAlpha` (0.5); `B1WinchAttachNearest` / `ToggleAttachNearest` point nearest the winch; `GA_B1AttachHook` point nearest the hand (S4-16 replaces with aim); rig `WinchAnchorAlpha` default 1/3 (= old 80 cm).
  - Debug: `ShowDebug B1Winch` draws each area (orange hooked, green in range) and the hook point, winch line shows alpha; Winch window shows alpha and cm along the area.
  - Simulate capture (temp jeep 7 m from `Tree_FirmPull`, StartAnchor alpha 0.33, deleted after): black strap around the trunk at 80 cm, cable leaves its jeep side.
  - Headless winch 30/30 (`s415_winch.log`): tree rigs unchanged (FirmPull 29 228 N, Stability20 38 231 N, CableLimit rest 77.79). Core and mud not rerun (anchors not used there).
  - **Area.** `UB1WinchAnchorComponent` gets an area:
    - `AreaStart` / `AreaEnd` (local points on the trunk axis) and `TrunkRadius`;
    - `GetAreaPoint(Alpha)`, plus closest-point helpers for a world point or a ray.

    The component's own location stops being the hook point.
  - **Attach at a point.** `UB1WinchComponent::AttachTo(Anchor, AreaAlpha)` replaces the fixed point. The sim input, `IsInReach` and the interaction option point all use the area point. The S4-09 body-frame offset is taken from the area point (unscaled).
  - **Force on the axis, cable on the surface.** The sim pulls at the axis point. The visual cable ends on the strap's surface on the side facing the winch.
  - **Cable over a log.** `UB1WinchComponent::SetupCableCollision` also blocks PhysicsBody, so a cable lying across the log rests on it. Vehicles and pawns stay ignored.
  - **Strap.** On attach, a static strap mesh appears at the point: a cylinder ring of radius `TrunkRadius` + 2 cm, 15 cm wide, solid black (`MI_B1WinchStrap`). It's removed on detach.
  - **Assets.** `BP_B1WinchTree`: vertical area 40–160 cm, with the right radius. The log's area is set in S4-17 with its new body.
  - **Callers.** `StartAnchor` gets `StartAnchorAlpha` (default 0.5). `B1WinchAttachNearest` attaches at the area point nearest the jeep. The rig's `WinchAnchor` step gets `WinchAnchorAlpha`.
  - **Debug.** `ShowDebug B1Winch` draws the area segment and the hook point; the Winch window shows the alpha.

  ⚠️ Note: every place that read the anchor component's location for the hook (option point, reach, sim input, cable, debug draw) has to switch; grep for it.

  Rig: tree rigs re-run with alpha at the old anchor height (80 cm on a 40–160 area = 0.33), so the numbers match within 1 kN.

  **Done:** builds; winch suite PASS (tree rigs unchanged); a Simulate capture shows the strap around a tree and the cable leaving its side.
  **User check:** the hooked cable looks wrapped around the trunk, not floating next to it.

- [x] **S4-16** Hook placement ghost (3.5 h) _(requires S4-15)_ <!-- done: 2026-10-28 -->
  Result:
  - Research: MCP can build materials (`MaterialTools.create_material` / `add_expression` / `connect_to_output` / `recompile`, `MaterialInstanceTools.create`). `M_B1WinchGhost` (translucent, unlit, `Color` → emissive, `Opacity` → opacity) + `MI_B1WinchStrapGhost` (yellow, 0.4), all via MCP, no user step.
  - `UB1HookAimComponent` (`Winch/`) on `AB1Character` (`HookAim`): ticks every frame; while `FindCarriedBy(owner)` finds a winch, picks among anchors within the scan's `Range` and `IsInReach`; alpha = closest point between the control-rotation ray (from the camera) and the axis; a trunk the Visibility trace hits wins, else smallest ray distance. Writes `UB1WinchComponent::SetAimTarget` / `ClearAimTarget` (cleared by `ReleaseAnchor` too). Ghost: unattached absolute `UStaticMeshComponent` with the winch's `StrapMesh` / `StrapClearance` / `StrapWidth` via the new `UB1WinchAnchorComponent::GetRingTransform` (strap uses it too), `GhostMaterial` on `BP_B1Character`.
  - "Attach" only on the aimed anchor, interaction point = aimed point; `GA_B1AttachHook` attaches at the aim (fails if the aim isn't the event's target). Winch window shows aim anchor, alpha, height, distance.
  - Rig: step `LookAtActor` + `LookAtOffset` (control rotation every frame), per-step `bCheckHookHeight` / `HookHeightRange` (fail `hookH@<steps>`, logged per step), metrics `hookAlpha=` `hookPoint=` (m above the anchor actor). `B1OnFoot_HookAim` (`B1Tests/OnFoot`, X 5 m, Y −45 m, HookCarry's first 7 steps + teleport by `Tree_FirmPull`): look at 1.2 m → 1.200 (alpha 0.667); detach, look at 3 m → 1.600 (alpha 1.000).
  - Headless winch `B1OnFoot*` 7/7 (`s416_onfoot.log`); `HookCarry` now hooks at the aim (1.6 m, it looks up after the teleport) and still pulls the jeep in (rest 1.50). PIE capture (HookAim rig auto-run, step 8 stretched, reverted): yellow ring on the trunk + "[E / X]  Attach". ADR-019 accepted.
  - **Aim.** `UB1HookAimComponent` on `AB1Character`. It ticks only while this character carries a hook. Each tick:
    1. find anchors in the interaction range;
    2. aim with the camera ray: a hit on the trunk is projected onto the area segment; otherwise take the closest point between the ray and the segment;
    3. clamp to the area.
  - **Aim result on the winch (ADR-019).** The component only computes. It writes `SetAimTarget(Anchor, Alpha)` on the carried winch (cleared when nothing is in reach). The option, the ability and the debug read it through `FindCarriedBy`, the hook state's one owner.
  - **Ghost.** A yellow translucent band (`MI_B1WinchStrapGhost`, same ring as the strap) shown at the aimed point; hidden when nothing is in reach.
  - **Attach.** The anchor's "Attach" option uses the aimed point as its interaction point and only appears on the aimed anchor. `GA_B1AttachHook` attaches at the aimed alpha. The prompt and the attach always agree (ADR-017).
  - **Debug.** The Abilities panel or the Winch window shows the aimed anchor, the alpha and the distance.
  - ADR-019 → accepted when the rig passes.

  🔍 Research first: can MCP create a translucent unlit material (`MaterialTools` is in the toolset list)? If not, ask the user to create it (user, 2026-10-28).
  ⚠️ Note: a thin standing tree is easy to miss with the ray, so the closest-point fallback is the normal case, not an edge case. The ray must ignore the character, the cable and the ghost.

  Rig:
  - step `LookAt` (world point or actor + offset → control rotation);
  - metrics `hookAlpha=` and `hookPoint=`;
  - `B1OnFoot_HookAim` (`Lvl_Gym_Winch`, by `Tree_FirmPull`):
    - take the hook, look at 1.2 m height, interact → hook point within 10 cm of 1.2 m;
    - detach, look at 3 m → clamped to 1.6 m.

  **Done:** builds; `B1OnFoot_HookAim` PASS headless; `B1OnFoot_HookCarry` still PASS; PIE capture of the ghost band.
  **User check:** carrying the hook, the yellow band follows your view along the trunk; on interact it turns into the black strap where you aimed.

- [x] **S4-17** Fallen log: frozen until pulled (ADR-018) (4 h) _(requires S4-15)_ <!-- done: 2026-10-28 -->
  Result:
  - Research: `SetSimulatePhysics` false ↔ true keeps the same proxy (kinematic flag only), so the pose stays and the winch's per-call `GetSimulatingBody` check picks it up. Going simulated welds children (`ApplyWeldOnChildren`); welds stay when it goes kinematic again. No stale handle: the S4-09 sim already clears it every step.
  - `AB1WinchLog` rebuilt:
    - `Body` (unscaled `UBoxComponent` root, 1000 × 50 × 50 cm along X, PhysicsActor, starts kinematic);
    - `TrunkVisual` (engine cylinder, no collision; new name so the old `Trunk` overrides can't re-apply);
    - `RootBall` (engine sphere, 140 cm, collision, at the −X end, bottom flush with the box);
    - `Anchor` area −400..+480 cm on X, radius 25.
  - `Log|Config`: `TrunkMass` 1800, `RootBallMass` 1500 (mass overrides), `BreakawayTension` 40 kN (placeholder; measured in S4-18), `RefreezeDelay` 0.5 s, refreeze below 5 cm/s and 2 °/s. Tick: frozen → free at tension ≥ breakaway; free → frozen after the delay below breakaway and at rest. `ResetToPlacedPose`, `GetYawSincePlaced`, `GetMass` (welded mass when free).
  - `BP_B1WinchLog`: `PM_B1Log` (user) on `Body` and `RootBall`, the stale 48 cm `Anchor` offset cleared. `Log_FirmLane` re-placed at scale 1, yaw 90 (across the lane), X 57.5 m, Y −57 m, Z 26 cm. Its instance `Anchor` had lost its parent with the old root (sitting at world coordinates); re-parented to `Body` through MCP.
  - Debug: `ShowDebug B1Log` (the log's own HUD binding: state, tension vs breakaway, switches, yaw, mass), switch lines on `LogB1Physics`, Winch window "Log" section (nearest log: state, bar, typed mass / breakaway / refreeze fields, Reset, Save log to asset), cheat `B1LogReset`.
  - Rig: `TargetLog` (or a log as `WinchAnchor`; reset to its placed pose at script start), metrics `logState logSwitches logMove logYaw logMass`, thresholds `MaxLogMove`, `MinLogYaw`, `MaxLogSwitches`, `ExpectLogState`. `B1Winch_Log` retired (deleted).
    - `B1Log_Ram` (`B1Tests/Log`, X 42.5 m, 15 m from the log): 5 s full throttle. Hits at speed and jumps it (vmax 22 m/s): logMove 0.000, switches 0, frozen.
    - `B1Log_RamHooked` (same spot, hooked at alpha 0.5, pull 0): throttle 0.4 for 6 s, brake, handbrake. Climbs over at 6 m/s, tension ≤ 92 N: logMove 0.000, switches 0. The first full-throttle version ran 7 m past and snatched the slack cable to 60 kN, which freed the log: correct physics for driving off while hooked, not a ram.
  - Headless winch 32/32 (`s417_winch2.log`). ADR-018 stays proposed until S4-18's rigs pass.
  - User check round 1 (2026-10-28): the winch on the log works. But the 50 cm log was easy to drive over (it should stop the jeep), and it sat too close to the edge, so the user moved it to Y −47.5 m.
    - New `Log|Shape` (`TrunkLength` 1000, `TrunkRadius` 50, `RootBallRadius` 100), applied in `OnConstruction` to the box, visual, root ball and anchor area: a 1 m trunk with a 2 m root ball.
    - `Log_FirmLane` at Z 51 (resting on the floor); both ram rigs moved to Y −47.5 m.
    - `B1Log_Ram`: hits at 13.9 m/s and stops, maxH −0.94 (no climb), logMove 0.000. `B1Log_RamHooked`: stops against it, tension ≤ 71 N, logMove 0.000.
    - Masses unchanged (tuning is S4-18). Winch 32/32 (`s417_winch3.log`).
  - **Body restructure.** Today the root is the scaled trunk (actor scale 0.5 × 0.5 × 5), so children inherit the scale (the S4-09 scale bug class). New layout:
    - root: an unscaled `UBoxComponent` (simulating, PhysicsActor, `PM_B1Log`) sized to the trunk. It's the flat-sided no-roll collision, with a clean body frame;
    - trunk visual: a cylinder mesh child with no collision, scaled freely;
    - root ball: a welded child primitive at one end with collision and its own mass. The compound mass puts the COM toward the root, so no `COMNudge`;
    - `Anchor` on the box root (`GetSimulatingBody` reads the attach parent), with its area (S4-15) along most of the trunk.

    Re-place `Log_FirmLane` at actor scale 1.
  - **States.** Frozen (no simulation, blocks the jeep like a wall) and free (simulating).
    - Frozen → free when the hooked winch's published `Tension` ≥ `BreakawayTension`.
    - Free → frozen when tension < `BreakawayTension`, linear speed < 5 cm/s and angular speed < 2 °/s, all for `RefreezeDelay` (0.5 s).

    It switches on the game thread (log tick, one frame late is fine). Config: `Log|Config`.
  - **Winch side, already in place.** The winch sends `AnchorLocation` every tick (`B1WinchComponent.cpp:70`), and `GetSimulatingBody` checks `IsSimulatingPhysics` per call. So a frozen log is a static anchor at its current pose, and freeing it only adds the proxy to the next input. Verify that the rest length survives (no `AttachSerial` bump).
  - **Physics only (user).** No hook-position logic and no forces on the log except the cable and Chaos.
  - **Debug.**
    - `ShowDebug B1Log`: the log draws its own state (frozen/free, tension vs breakaway, switch count, yaw since spawn) whatever the viewer's winch is.
    - Each switch logged on `LogB1Winch` with the tension.
    - NetImGui: log section in the Winch window with typed fields for the config (mass, root ball mass, breakaway, refreeze) + "Save to asset".
    - Cheat `B1LogReset` (back to the placed pose, frozen).
  - ADR-018 → accepted when S4-18's rigs pass.

  🔍 Research first: does `SetSimulatePhysics` false ↔ true keep the same physics proxy and the body's pose and velocity? Do welded children stay welded across the toggle? If the proxy changes, the winch must pick up the new one without a stale handle (S4-09 clears it after each step).
  ⚠️ Note: MCP can't create physical materials (mcp-tools.md). Ask the user to create `PM_B1Log` by hand, as with the mud materials (user, 2026-10-28).
  ⚠️ Note: the S4-09 rig `B1Winch_Log` changes meaning; retire it or re-point it at the S4-18 rigs.

  Rig: metrics `logState=` (end state), `logSwitches=`, `logMove=` (max anchor-body displacement), `logYaw=`; thresholds `MaxLogMove`, `MinLogYaw`, `ExpectLogState`.
  - `B1Log_Ram`: 5 s full throttle into the log from 15 m → `logMove` ≤ 1 cm, `logSwitches` = 0.
  - `B1Log_RamHooked`: the same with the hook attached and no pull.

  **Done:** builds; both ram rigs PASS headless; winch suite PASS.
  **User check:** drive into the log and over it: it doesn't move or roll.

- [x] **S4-18** Road lane + fallen tree tuning (4 h) _(requires S4-14, S4-16, S4-17)_ <!-- done: 2026-10-28 -->
  Result:
  - Road (`Gym/Road`, 35 actors): **8 m wide** for its whole length (user, 2026-10-28: two cars side by side; 6 m was too tight to pull from and to drive through). Bands at Y −43.5 / −51.5 m, X 27.5..77.5 m: BasicShapes cube panel (Z 20..100 cm, 15 cm thick, BlockAll) + cylinder posts every 5 m, `M_B1RoadBand` (new, MCP: world-position Custom node, red/white diagonal stripes). Notch in the south band around the root (X 50.5..59.5 m, back at Y −55.1 m, 30 cm behind the root ball) so the log can swing toward the jeep and the jeep can't get round the root. A 46.5..52 m notch first: at ~48° the trunk jammed on its corner post (rigs stalled at the same yaw for any mass).
  - `Log_FirmLane`: X 57.5, Y −48.8 m, yaw 90; tip 30 cm inside the north band, root end 2.3 m past the south band.
  - Rig (`B1GymTestRig`): `LogFreeBandOffset` (cm past the placed tip); metrics `logGap` (end), `logGapT`, `logPivot` (root end's path / yaw turned while turning > 1°/s), `logDrift` (last 5 s of the last pulling step), `logCross` (peak vehicle travel past the placed log line); thresholds `MinLogGap`, `MaxLogGap`, `MaxLogGapTime`, `MaxLogPivot`, `MinLogCross`, `MaxDisplacement`. Step `bSteerToTarget` / `SteerTarget` (rig frame, full lock at 20° off the nose): open-loop steering couldn't thread the gap.
  - Tuning (physical knobs only, PIE runs):
    1. Breakaway lower bound: braked jeep (brake 1, no handbrake) holds 40.6 kN statically before it slides (same at Y −47.5 and −50 m) → `BreakawayTension` **49 kN** (1.2 ×).
    2. Tip: the swing stops where the cable turns along the trunk, not where friction runs out. On the 6 m road: 11 t trunk 49.5° / gap 3.14 m, 10 t 51.2° / 3.34 m (jeep ~2.5 m wide didn't fit). On the 8 m road at 10 t: 58.6°, gap 4.41 m. Stall torque gives tip resistance ≈ 31–35 kN (0.53–0.58 × cap).
    3. Pivot 0.38–0.40 m from the root end: the root ball holds; no root-end drag needed.
    - Final: `TrunkMass` **10000**, `RootBallMass` **5000**, `BreakawayTension` **49000**, `PM_B1Log` unchanged (μ 0.7, Average), shape unchanged. Set on `BP_B1WinchLog` and the instance.
  - Rigs (`B1Tests/Log`, X 42.5 m, Y −50 m, near the root side: the cable stays closer to square with the trunk). Pull script: handbrake 1 s, handbrake + pull 20 s, reel out −0.3 for 0.5 s (a stalled drum keeps 60 kN on a still log, so it stays free), handbrake 2.5 s.
    - `B1Log_PullNoHandbrake` (alpha 0.9, brake 1 s, brake + pull 4 s, brake 2 s; `MaxTension` 63 kN): tensionMax 40.6 kN, dragged 8.9 m, logSwitches 0.
    - `B1Log_PullEnd` (alpha 0.9; `MinLogGap` 3, `MaxLogGapTime` 15, `MaxLogPivot` 1, `MaxDisplacement` 0.5, frozen): gap 4.41, 3 m at 7.7 s, yaw 58.6, pivot 0.39, disp **0.49** (close to the limit: the lurch at breakaway), refrozen.
    - `B1Log_PullMiddle` (alpha 0.5; `MaxLogGap` 3): yaw 6.2, gap 0.27, drift 0.026 m/s (creeps ~2.6 cm/s under 60 kN while free; reported, accepted as physics).
    - `B1Log_DriveThrough` (PullEnd script, detach, reverse 4.5 s at brake 0.4, handbrake 1 s, steer to (5, +4.25) m 4 s at 0.3, to (30, +4.25) m 7 s at 0.4; `MinLogCross` 5): logCross 22.6.
    - `B1Log_Ram` / `RamHooked` unchanged, still logMove 0.000, switches 0.
  - User check (2026-10-28): the user widened the road further (12 m: north band Y −43.1 m, a straight south band `Band_Free2` at Y −55.1 m, notch removed; log unchanged) and likes it. Rigs re-aimed: `LogFreeBandOffset` 70 cm on the four pull rigs; DriveThrough waypoints (0, +5) m for 5 s, then (30, +5) m. On that road, headless winch 36/36 (`s418_winch2.log`): PullEnd yaw 60.7, gap 5.11 at 7.3 s, disp 0.04; PullMiddle yaw 6.4, gap 0.68; NoHandbrake 39.6 kN, no switch; DriveThrough logCross 25.7.
  - Cable visual (user, same check): a loaded cable sagged on the ground while pulling. `UB1WinchComponent::CableTautTension` (`Winch|Visual`, 5000 N, typed field + Save in the Winch window): the cable's `CableGravityScale` = 1 − tension / that, so it pulls straight under load and sags as before when slack or overpaid; reset to 1 when carried. Simulate capture: slack before the pull, straight mid-pull. User: looks good.
  - Headless winch 36/36 (`s418_winch.log`), core 12/12 (`s418_core.log`), mud 22/22 (`s418_mud.log`). Simulate captures of the opened tree and the drive-through (top view). ADR-018 accepted.
  - **Road lane.** A banded road in `Lvl_Gym_Winch`, rebuilt from the log lane (X ~57 m, Y −57 m):
    - firm road 6 m wide, closed off on both sides by bands (blocking posts + a striped band) the jeep can't pass;
    - a ~10 m fallen tree across it, its root end past one band;
    - a 30 m run-up.
  - **Tuning, physical knobs only** (mass, `PM_B1Log` friction, root ball mass and size, contact shape), in this order:
    1. **Breakaway lower bound.** Run `B1Log_PullNoHandbrake` with breakaway at the cap and read the tension a braked jeep reaches while dragged. Set `BreakawayTension` ≥ 1.2 × that.
    2. **Tip vs middle.** With uniform friction turning about the root end, the tip needs about μ·m·g/2 and the middle about μ·m·g. Target tip resistance about 0.55–0.8 × cap, so the middle sits above the cap.
    3. **Pivot.** The root has to push back with about the pull force to stay the pivot; otherwise the log turns about a point roughly 0.3 L from the far end. If `logPivot` > 1 m, try a heavier or wider root ball first. An authored root-end drag force only with the user's OK.

    Record the final values and the numbers per step in the result.

  Rig: metrics `logGap=`, `logPivot=` (distance of the turn centre from the root end, while turning), `logDrift=` (m/s in the last 5 s of pull); threshold `MinLogGap`. `logGap` = the shortest distance, measured across the road, from the free-side band line to the log's footprint (box corners + root ball), sampled across the road at the log's position.
  - `B1Log_PullNoHandbrake`: alpha 0.9, braked only → the jeep is dragged toward the log; `logSwitches` = 0 once breakaway is set.
  - `B1Log_PullEnd`: alpha 0.9 (free end), braked + handbrake, full pull → `logGap` ≥ 3 m within *measured* s (target ≤ 60 s); `logPivot` ≤ 1 m; jeep `lastDisp` ≤ 0.5 m; `logState` = frozen 2 s after the pull stops.
  - `B1Log_PullMiddle`: alpha 0.5, same script → `logGap` smaller than in `PullEnd`, reported as is (physics first: no fixed mark). `logDrift` reported to catch Chaos creep; some creep is acceptable physics, and an authored standstill term only with the user's OK.
  - `B1Log_DriveThrough`: the `PullEnd` script, then a detach step, then steer through the gap → the jeep passes 5 m beyond the log line.

  ⚠️ Note: if the physical knobs can't make a door at all, stop and bring the numbers to the user rather than adding a scripted assist.

  **Done:** the four rigs + the ram rigs PASS headless; all suites PASS; capture of the opened tree.
  **User check:** the whole fallen tree scenario: ram it, get out, hook near the end, hold B, reel in, drive through the gap. Hooking at the middle struggles. It feels physical, not scripted.

- [x] **S4-19** Controls hints (2.5 h) _(requires S4-14)_ <!-- done: 2026-10-28 -->
  Result (done before S4-18, user):
  - `UB1ControlHintSet` (`Player/`, data asset): `FB1ControlHint` = `Actions` (several, e.g. mouse + stick look), `Label`, `RequiredTags`, `BlockedTags`, `KeysPerDevice`. `HintSet_B1Player` (`/Game/B1/Input`) on `BP_B1PlayerController` (`ControlHints`).
    - Jeep (`State.Avatar.InVehicle`): Drive, Brake / reverse, Steer (2 keys), Handbrake, Winch in / out (+ `Vehicle.State.WinchHooked`, 2 keys), Get out.
    - On foot (`State.Avatar.OnFoot`): Move (4 keys), Look (mouse + stick actions), Jump.
  - `UB1ControlHintsWidget` (`Player/`, code-only, lower-left, 16 pt): rebuilds every 0.2 s from the PlayerState ASC's tags and the applied mapping contexts (so a rebind or a possession change shows up), logs on `LogB1Vehicle` when the text changes. Added by `AB1PlayerController::BeginPlay` next to the prompt.
  - `B1InputKeyText::GetKeyText` (`Player/`), shared with the prompt: keyboard/mouse keys, then pad keys, short pad names (A/B/X/Y, RT/LT, RB/LB, LS/RS, D-pad, Mouse, Space). Keys are filtered by menu category (keyboard, mouse, gamepad): the template's VR and racing-wheel mappings showed up otherwise ("LS Vive (L) Trackpad X-Axis", and racing wheel first after re-entering, when context order changes). Keyboard first, like the prompt ("W / RT   Drive").
  - `Vehicle.State.WinchHooked` (native tag): `UB1WinchComponent::HookedTag` (set by `AB1Jeep`), on the owner's ASC while attached and controlled. `SetWinchingTag` became `UpdateOwnerTags` / `SetOwnerTag` for both.
  - PIE captures (plain PIE; `B1OnFoot_HookCarry` auto-run, reverted): jeep unhooked = Drive, Brake / reverse, Steer `D A / LS`, Handbrake `Space / B`, Get out `E / X`; on foot = `W S A D / LS Move`, `Mouse / RS Look`, `Space / A Jump`; jeep hooked adds `Up Down / D-pad Up D-pad Down  Winch in / out`.
  - Headless core 12/12 (`s419_core.log`), winch 32/32 (`s419_winch.log`), with the widget on every player controller.
  - **Data.** `UB1ControlHintSet` (data asset): entries of input action, label, required tags and blocked tags. `HintSet_B1Player` in `/Game/B1/Input`, set on `AB1PlayerController`.
    - In the jeep: Drive (RT / W), Brake / reverse (LT / S), Steer, Handbrake (B / Space), Exit (X / E); Winch in/out (D-pad ↑↓ / arrows ↑↓) only with `Vehicle.State.WinchHooked`. These read right only after S4-14's mapping cleanup (no pad A brake, no LB handbrake), since the helper shows the first pad key.
    - On foot: Move, Look, Jump. The interact prompt (S4-08) stays separate.
  - **Widget.** `UB1ControlHintsWidget` (`Player/` or `UI/`, code-only like `UB1InteractionPromptWidget`) in the lower-left corner. It filters entries by the player ASC's tags and shows both keys ("RT / W  Drive"). Extract the key-text code from the prompt into a shared helper so both read keys the same way (Xbox letters for face buttons).
  - **Tag.** New `Vehicle.State.WinchHooked`: the winch sets it on the owner's ASC while attached and the pawn is controlled (like `WinchingTag`, host-set property `HookedTag`).

  ⚠️ Note: collapsed `UUserWidget`s don't tick (S4-08): keep the root visible.
  ⚠️ Note: keys come from the mapping contexts, so a rebind updates the hints without touching the asset.

  **Done:** builds; PIE captures in the jeep (unhooked, hooked) and on foot show the right lists; headless core and winch suites PASS with the widget on.
  **User check:** the hints match what the controls do, and the winch line appears only when hooked.

- [x] **S4-10** Phase 2 playtest (user + agent, 1–2 h) _(requires S4-06..S4-08, S4-11..S4-19)_ <!-- done: 2026-10-28 -->
  User sign-off (2026-10-28): happy with the results; Phase 2 closed.
  Agent part (2026-10-28, after the taut-cable build `182f282`): headless winch 36/36 (`s418_winch2.log`, on the user's 12 m road), core 12/12 (`s410_core.log`), mud 22/22 (`s410_mud.log`). Waiting on the user's playtest and sign-off.
  Full loop on foot in `Lvl_Gym_Winch`: get stuck, get out, carry the hook, attach, get in, winch out; same on both slopes; lower the jeep down a slope on reel-out; the fallen tree scenario on the road lane (VISION.md). All suites headless.
  **Done:** all suites PASS; ROADMAP Phase 2 ticked.
  **User check:** sign-off on Phase 2.

### Backlog (found during the sprint)

- **Mud idle check reads empty inputs (suspected, S4-14).** `FB1MudSimulation::PreFriction` decides "no input" from `Vehicle.VehicleInputs`, which Chaos only fills with network prediction on (`ChaosVehicleManagerAsyncCallback.cpp`), so it may always read 0. If so, brake, handbrake and parking torque are zeroed on every muddy slope. Verify with a rig (brake on the medium slope vs idle), then pass the step's `InputData` controls like the handbrake hold does.

---

## Sprint 5 — Visuals: dirty jeep + mud spray (Phase V)

**Status:** planned 2026-11-07. Start with `/work`.

**Goal.** Close Phase V. The Wrangler starts clean and collects mud (tires, then arches, then splatter, then spots), capped at `Concept/jeep_wrangler_mud_reference.jpg`. The tires throw a spray close to `Concept/mud-car-drive.mp4`. Hard wheelspin splashes the chase camera's lens, capped at `Concept/mud-on-the-screen.jpg` (S5-21..S5-28, added 2026-11-08).

Design: [plans/2026-11-07-visuals.md](plans/2026-11-07-visuals.md) (revision 1). Decision: ADR-021 (material layer + Custom Primitive Data, not decals).

**Sprint-wide notes**
- **Look-first order.**
  - Spikes S5-01..03 → generator → material → `PreviewDirt` → stills → **checkpoint 1** (S5-09).
  - Then accumulation, rigs and spray → **checkpoint 2** (S5-19).
  - Don't start S5-10 before the user signs off the stills.
- ⚠️ **Gym rigs spawn `BP_B1Jeep` (buggy) by default.** Dirt and spray rigs set `VehicleClass` = `BP_B1Jeep_Wrangler`.
- ⚠️ **Child-BP rule:** config saves and CDO edits go to `BP_B1Jeep_Wrangler` (the player's pawn), not only `BP_B1Jeep`. Check with `get_properties`.
- ⚠️ **Portability:** `UB1MudDirtComponent` and `B1MudThrow` live in `Mud/` with no project includes. Wrangler-specific setup (tags, arch params) is host-side. Each task that adds a host requirement updates MIGRATION.md.
- ⚠️ **ImGui rule:** every tunable float uses `B1ImGui::DragFloatTyped`.
- ⚠️ **Glass isn't on `M_Wrangler_Base`:** `MI_Wrangler_Glass` is a child of the template's `M_SportsCar_Lights`. The glass film needs its own Wrangler glass material that calls `MF_B1MudDirt` (S5-06), so the template material stays untouched.
- **Out of scope:** splash sheet, washing, UV-painted dirt, dirt material on the buggy, perf work. **Stretch:** wetness (CPD slot 5), tread-arc spawn.
- **Suites:** `Lvl_Gym_Core` (12), `Lvl_Gym_Mud` (22), `Lvl_Gym_Winch` (36) must stay green. The spray and dirt are visual only.

### Spikes

- [x] **S5-01** Spike: Custom Primitive Data and tags on the Wrangler's inherited components (1 h) <!-- done: 2026-11-07 -->
  Result: both work in the BP, no host-side fallback needed.
  - Inherited C++ components are reachable as `/Game/B1/Vehicles/BP_B1Jeep_Wrangler.Default__BP_B1Jeep_Wrangler_C:<name>` (names: `Chassis`, `Tire Front Left`, `Tire Front Right`, `Tire Rear Left`, `Tire Rear Right`). `ObjectTools.set_properties` with `ComponentTags` and `CustomPrimitiveData` (`{"data": [..]}`) sticks; the parent `BP_B1Jeep` stays untouched. Tags survive compile, save and an editor restart.
  - Material side: CPD is read by a `ScalarParameter` with `bUseCustomPrimitiveData=true` and `PrimitiveDataIndex`, not `PerInstanceCustomData` (that one is for ISMs). A temporary lerp to red on `M_Wrangler_Base` BaseColor turned only `Tire Front Left` red (capture in the scratchpad, `spike_cpd.png`); `MI_Wrangler_Tire` inherits from the base, so tires and body both see it.
  - Kept: tag `B1MudDirt` on `Chassis` + four tires of `BP_B1Jeep_Wrangler`. Reverted: the material nodes (graph back to its 5 original expressions, re-saved) and the tire's CPD (empty array).
  🔍 Research first: `UPrimitiveComponent::SetCustomPrimitiveDataFloat`, the `PerInstance/CustomPrimitiveData` material node, and whether `ComponentTags` / `CustomPrimitiveData` are editable on inherited C++ components in a child BP through `ObjectTools`.
  On `BP_B1Jeep_Wrangler`:
  - tag `Chassis` and the four tire components with `B1MudDirt`;
  - set CPD slot 0 = 1 on one tire;
  - add a temporary `CustomPrimitiveData[0]` → tint lerp in `M_Wrangler_Base`.
  Revert the material change at the end.
  ⚠️ Note: if the tags won't stick on inherited components, set them host-side in `AB1Jeep`'s constructor (portability still holds). Record which way worked.
  **Done:** a capture of the placed Wrangler shows one red tire; the tags persist after save and reload; the result is noted in this task.
  **User check:** —

- [x] **S5-02** Spike: Niagara toolset builds a GPU mesh-renderer emitter with depth-buffer collision (1.5 h) <!-- done: 2026-11-08 -->
  Result (done inside S5-14 on `NS_B1MudSpray` itself, no scratch system; the existing GPU emitter already had depth-buffer collision): the toolset covers it all.
  - `NiagaraToolset_System`: `AddRenderer(newRendererLocation, rendererClass)`, `RemoveRenderer(rendererToRemove)`, `SetRendererData(renderer, rendererData.propertyValues)` (JSON string: `Meshes[0].mesh`, `bOverrideMaterials`, `OverrideMaterials[{explicitMat}]`), `AddModule(moduleLocationRef{scriptName: ParticleSpawnScript/ParticleUpdateScript}, moduleAsset)` appends, `SetStackInputData(stackInputRef{inputNameStack:[name]}, inputData)`, `AddUserVariables`, `GetSystemCompileState`, `GetStackIssues(system)`. Every ref needs all six fields (`rendererIndex` -1, empty strings).
  - Input values: float `{struct NiagaraFloat, value{value}}`, enum `NiagaraExt_StackInputData_Enum` (enumName `X::NewEnumeratorN`, look up with `NiagaraToolset_Info.UEnum_Info`), HLSL `NiagaraExt_StackInputData_HlslExpression {hlslExpression}`; `rand(1.0f)` and `User.*` / `Particles.*` work in expressions.
  - `GetEmitterTopology` / `GetEmitterInputValues` are ~60k chars: run them inside `execute_tool_script` and return only what's needed. Scripts' result dicts reject `.get(k, default)`.
  - Modules: `/Niagara/Modules/Spawn/Orientation/V3/InitialMeshOrientation`, `/Niagara/Modules/Update/Orientation/V3/UpdateMeshOrientation`, `/Niagara/Modules/Update/Size/ScaleMeshSize`.
  🔍 Research first: `describe_toolset` on `NiagaraToolset_System` (once) and `NiagaraToolset_Assets.FindNiagaraScripts` for the collision and mesh-renderer modules.
  In a scratch system `/Game/B1/FX/Scratch/NS_B1Spike`: one GPU emitter, a mesh renderer with an engine cube, spawn rate, velocity, gravity, collision (depth buffer, no kill), and a user float wired to the spawn rate.
  ⚠️ Note: ADR-011 rules out distance fields, so collision has to be depth buffer. If the mesh renderer or collision can't be configured, record the fallback (masked clump sprites, or the user wires the module from a written spec).
  **Done:** a PIE/Simulate capture shows cubes landing and stopping on the floor; findings (tool names, gotchas) noted here; scratch system deleted.
  **User check:** —

- [x] **S5-03** Spike: Wrangler body space (0.5 h) <!-- done: 2026-11-07 -->
  Result (cm, body local space = `SKM_Wrangler` mesh space, since `Chassis` sits at 0/0/0 on the mesh and the `VisWheel_*` sockets have zero offset on `PhysWheel_*`):
  - **Arch centres = wheel centres** (from `Scripts/VehicleImport` `wrangler_info.json`, run `wrangler2` with the defaults 19.5 / 1.2 / 6 / 3; its body bounds match `get_bounds` exactly): FL (136.5, −80.4, 46.3), FR (136.5, 80.4, 46.3), BL (−136.5, −80.4, 46.3), BR (−136.5, 80.4, 46.3). Wheel index order in Chaos is FL, FR, BL, BR (CPD slots 1–4).
  - **Body bounds** `SM_Wrangler_Body`: min (−245.6, −109.9, 42.4), max (213.0, 109.9, 211.1). Tire `SM_Wrangler_Tire`: ±46.3 radius, ±18.2 width, centred.
  - **Sill:** body underside 42.4; the side step / rocker band reads as ~42–75 cm from the S5-01 capture (top of the tire is 92.6). Start `SillHeight` at 75 and refine in the S5-08 stills. Arch falloff radius start: tire radius + ~20 = 65.
  - **CPD unused:** `M_Wrangler_Base` (all body/tire MIs) has one parameter `Tint`; `MI_Wrangler_Glass`'s parent `M_SportsCar_Lights` (thin translucent) has `BaseColor`, `Metallic`, `Specular`, `Roughness`, `Opacity`. None uses CPD.
  Read:
  - the bounds of `SM_Wrangler_Body` and `SM_Wrangler_Tire` (`StaticMeshTools`);
  - the `VisWheel_*` socket positions in `SKM_Wrangler`;
  - the `Chassis` relative transform in `BP_B1Jeep_Wrangler`.
  From these, work out the four arch centres and the sill height in the body mesh's local space. Check that no `MI_Wrangler_*` reads CPD already.
  **Done:** arch centres, body bounds and sill height written into this task (cm, body local space).
  **User check:** —

### Dirt look (checkpoint 1)

- [x] **S5-04** Mud texture + clump mesh generator (2.5 h) <!-- done: 2026-11-07 -->
  Result: `Scripts/MudTextures/gen_mud_textures.py` (seed 7, ~40 s) + README. Masks are reveal-order fields (histogram-equalized, so threshold t covers fraction t of the area); splatter is 0 outside the splats (12 % covered). Imported: `T_B1MudBlotch` 1024², `T_B1MudSplatter` 1024², `T_B1MudNoise` 512² (all `SRGB` off, `TC_Grayscale`: 8-bit uncompressed, so thresholds don't band; separate textures, not packed), `SM_B1MudClump` (48 tris, bounds ±1.13 × 0.79 × 0.77 cm, Nanite off). Contact sheet checked: no seams on 2×2 repeats; the first noise pass was too fine and got lowered. Python deps aren't installed system-wide: use a venv with `numpy` + `pillow`.
  `Scripts/MudTextures/` (Python + numpy/pillow, fixed seeds, README with the parameters and the re-import steps, like `Scripts/VehicleImport`):
  - `T_B1MudBlotch` (1K tileable, large soft blotches);
  - `T_B1MudSplatter` (1K tileable, small hard splats with drips);
  - `T_B1MudNoise` (512 breakup);
  - `SM_B1MudClump.obj` (low-poly lumpy blob, ~40 tris, centred, 1 cm radius).
  Import the textures into `/Game/B1/Mud/Dirt/` with `TextureTools.import_file` (masks: linear, BC4 / grayscale), and the mesh into `/Game/B1/FX/`.
  ⚠️ Note: `import_file` won't overwrite: delete, re-import, reassign (HISTORY 2026-11-07). Pack the masks into channels of one texture if it saves samples, but keep it simple.
  **Done:** assets imported and saved; `get_size` matches; a contact sheet PNG of the masks is checked; the README is re-runnable.
  **User check:** —

- [x] **S5-05** `MF_B1MudDirt` material function (3.5 h) _(requires S5-01, S5-03, S5-04)_ <!-- done: 2026-11-07 -->
  Result: `/Game/B1/Mud/Dirt/MF_B1MudDirt`.
  - **Inputs:** BaseColor, Roughness, Normal (default 0,0,1), IsTire (scalar, default 0; the host feeds it from a static switch), ArchFL/FR/BL/BR, SillHeight, BoundsMin/Max. **Outputs:** BaseColor, Roughness, Normal, Mask (for the host's own lerps, e.g. metallic).
  - **CPD params** (in the function, group `Mud Dirt`): `DirtOwn` slot 0, `DirtWheelFL/FR/BL/BR` slots 1–4. **Look params:** `DirtBlotchTex`/`DirtSplatterTex`/`DirtNoiseTex` (texture objects, `LinearGrayscale` sampler), `DirtBlotchTile` 150 / `DirtSplatterTile` 120 / `DirtNoiseTile` 60 cm, `DirtArchRadius` 65, `DirtArchFade` 50, `DirtEdgeSoftness` 0.08, `DirtRoughness` 0.85, `DirtNormalFlatten` 1, `DirtMudColor`.
  - **Logic** is one Custom node (`Scripts/MudTextures/MF_B1MudDirt.hlsl`, keep in sync): local-space triplanar (`LocalPosition`, `VertexNormalWS` → Transform to local), drips run down on side projections. Tire: coat by `field = lerp(blotch, noise, 0.35)` vs slot 0. Body: blotch-coverage = max(arch spheres × wheel slot, sill gradient × max(body, mean wheels), low-body × body); splatter = body dirt denser low, + late sparse spots (> 0.7), + near arches. Returns `float4(lerp(BC, mud, m), m)`; early-out to `BC` when every slot is 0, and every other output is a lerp with alpha = mask, so dirt 0 is exact.
  - Custom node inputs through MCP: set element 0, then N copies, then the names (26 inputs).
  - Stage test with CPD set by hand on the placed photo-booth Wrangler at 1.0: tires coated, arches/sills/lower body filled, splatter up the doors (stills in the scratchpad, `caps/d100_*.png`). First look for S5-08: mud too light/sandy in sun, door blotches too big, upper body lacks the reference's light film.
  🔍 Research first: `MaterialTools` function creation and inputs/outputs; triplanar in local space (`LocalPosition`, vertex normal in local space).
  `/Game/B1/Mud/Dirt/MF_B1MudDirt`:
  - **Inputs:** BaseColor, Roughness, Normal, `bIsTire` (static bool), arch centres (4 × vector), sill height, body bounds.
  - **CPD:** slot 0 = own dirt (body or tire), slots 1–4 = the wheels.
  - **Mask:** a two-scale triplanar mask (blotch × splatter, with noise breakup).
  - **Region-weighted reveal:**
    - tire: full coat by slot 0;
    - arches: a sphere falloff per wheel, by that wheel's slot;
    - sills: low-height gradient;
    - doors and bumpers: splatter by body dirt, denser low;
    - hood, roof and glass: sparse spots late (body dirt > ~0.7).
  - **Output:** a lerp to one dry mud colour and roughness ~0.85, normal flattened under mud.
  ⚠️ Note: dirt 0 must leave the inputs bit-exact, so the clean jeep doesn't change. Keep the HLSL in Custom nodes small, as `M_B1Mud` does.
  **Done:** the function compiles; a test MI with CPD set by hand shows each stage.
  **User check:** —

- [x] **S5-06** Wire `MF_B1MudDirt` into the Wrangler materials (1.5 h) _(requires S5-05)_ <!-- done: 2026-11-07 -->
  Result:
  - `M_Wrangler_Base`: BaseColor (D × Tint), MRA.G, Normal → the function → BaseColor/Roughness/Normal outputs; Metallic = lerp(MRA.R, 0, Mask). Params (group `Mud Dirt`): `bIsTire` static switch (1/0 into IsTire), `DirtArchFL/FR/BL/BR`, `DirtSillHeight` 75, `DirtBoundsMin/Max` with the S5-03 values. `MI_Wrangler_Tire` sets `bIsTire` = true.
  - `M_Wrangler_Glass`: duplicate of `M_SportsCar_Lights` (thin translucent) + the function and a light film: film = `DirtOwn` × `DirtGlassFilm` (0.35); BaseColor lerps to `DirtGlassFilmColor` by film, roughness → 0.85 by film; Opacity → 1, Metallic → 0 and TransmittanceColor → dark brown by max(mask, film). `MI_Wrangler_Glass` reparented: Metallic 1, Specular 1, Roughness 0.05, Opacity 0.35, BaseColor (0.02, 0.025, 0.03) kept.
  - Dirt-0 check: before/after captures at the photo-booth poses differ only at the capture noise floor (two captures with no change differ by the same amount: animated sky reflected in paint/glass, TAA). Exact by construction (all lerps alpha 0, early-out).
  - `M_Wrangler_Base` calls the function, with the arch centres, sill and bounds from S5-03 as parameters.
  - `MI_Wrangler_Tire` sets `bIsTire`.
  - New `/Game/Vehicles/Wrangler/Materials/M_Wrangler_Glass`: a duplicate of the `M_SportsCar_Lights` graph, or a simple translucent glass, plus the function (light film only). `MI_Wrangler_Glass` is reparented to it.
  ⚠️ Note: reparenting `MI_Wrangler_Glass` can drop parameter overrides. Check its look before and after with a capture at dirt 0.
  **Done:** captures at dirt 0 before and after are identical (no visual change on a clean jeep); all `MI_Wrangler_*` compile.
  **User check:** —

- [x] **S5-07** `UB1MudDirtComponent`: targets, CPD writes, `PreviewDirt` (2.5 h) _(requires S5-01)_ <!-- done: 2026-11-07 -->
  Result: `Mud/B1MudDirtComponent` on `AB1Jeep` as `MudDirt` (`GetMudDirt()`), `TargetTag` `B1MudDirt`, `PreviewDirt`, `WheelDirt[4]` / `BodyDirt` (0 until S5-10), `GetShown*` (preview or accumulated), `GetNumTire/BodyTargets` for the rig.
  - Targets resolve in the first tick that has wheels (they come with the physics state, like the spray's emitters); tire = within the wheel radius of `UB1MudVehicleComponent::GetWheelLocation` (new, forwards `IB1MudSimHost`). PIE in `Lvl_Gym_Core`: both the player's and the booth Wrangler log `dirt targets: body=Chassis w0=Tire Front Left w1=Tire Front Right w2=Tire Rear Left w3=Tire Rear Right`.
  - CPD goes through `SetCustomPrimitiveDataFloat` (the runtime copy), so preview never dirties a level or BP; the serialized `CustomPrimitiveData` stays empty. Writes compare against the component's current CPD (≥ 0.005, or exactly 0/1), so a re-registered part is caught.
  - Editor (no game world): `bTickInEditor`, `OnRegister` and `PostEditChangeProperty` write `max(PreviewDirt, 0)` into slots 0–4 of every tagged part. `PreviewDirt` 0.5 via `ObjectTools` on the placed booth jeep renders in a `CaptureViewport` with no PIE.
  - Headless: core 12/12 and mud 22/22 PASS, numbers as before (FullThrottle5s 77.13 m). MIGRATION.md: component, tag, CPD layout, `MF_B1MudDirt` + textures, `SM_B1MudClump`.
  - Fix found on the way: `MaterialInstanceTools.set_parent` doesn't mark the package dirty, so `MI_Wrangler_Glass`'s reparent (S5-06) wasn't saved. Re-set a parameter after `set_parent`, then save.
  ⚠️ Note: new class → `Scripts/BuildEditor.ps1` (drops `Makefile.bin`).
  `Mud/B1MudDirtComponent` (portable), on `AB1Jeep` as `MudDirt`:
  - `WheelDirt[4]`, `BodyDirt`;
  - `PreviewDirt` (−1 = off; `Mud|Dirt`, applied in `OnRegister` and `PostEditChangeProperty`, so it works in the editor and in game);
  - target discovery at BeginPlay: components tagged `B1MudDirt`, nearest wheel within the wheel radius → that wheel's tire, none → the body. Wheel locations come through `UB1MudVehicleComponent` / `IB1MudSimHost`. In the editor (no wheels), use the target's distance to the owner's wheel sockets, or treat all tagged parts as preview-only (every slot = `PreviewDirt`);
  - one log line on `LogB1Mud`: `dirt targets: body=… w0=… w1=… w2=… w3=…`;
  - CPD written only on a change ≥ 0.005.
  MIGRATION.md: the component, the tag and the CPD layout.
  **Done:** builds; a placed Wrangler shows stages when `PreviewDirt` is set through `ObjectTools`; PIE logs four tires plus the body; no other suite changes.
  **User check:** —

- [x] **S5-08** Photo booth + stage stills (1.5 h) _(requires S5-06, S5-07)_ <!-- done: 2026-11-07 -->
  Result:
  - Booth: `B1PhotoBooth_Wrangler` in `Lvl_Gym_Core` (folder `B1Tests/PhotoBooth`) at (−20, −130, 0.02) m, **yaw 115°**, 130 m from the rigs at the origin; core suite 12/12 with it placed. The two concept angles face opposite ways, so one is always backlit unless the sun (yaw ≈166°) rakes both from the side; yaw 115 does that.
  - Poses (x, y, z cm, pitch, yaw): front ¾ left (−1855, −12458, 150, −6, −105); rear ¾ right (−2145, −13542, 165, −7, 75).
  - Scripts: `Scripts/MudTextures/photobooth.py` (paste into `ProgrammaticToolset.execute_tool_script`, **one stage per call**: 10 captures in one reply closed the MCP connection) and `stage_sheet.py <out_dir> <result files…>` (crops the stills next to the clean and mud concept images).
  - Look passes before the checkpoint: mud colour 0.2/0.13/0.07 → 0.055/0.047/0.038 → 0.065/0.05/0.035 (was sandy in sun); body film (`DirtFilmStrength` 0.5, from body dirt 0.3, patchy by noise; new Custom input `FilmStrength`); `DirtBlotchTile` 150 → 100; glass film colour → 0.06/0.05/0.04.
  - Sheet: `stills3/stage_sheet.png` in the session scratchpad.
  A placed `BP_B1Jeep_Wrangler` in `Lvl_Gym_Core` (folder `B1Tests/PhotoBooth`, a clear spot away from the rigs), with two camera transforms matching the concept angles (front three-quarter left, rear three-quarter right) recorded in this task. A `ProgrammaticToolset` script sets `PreviewDirt` to 0 / 0.25 / 0.5 / 0.75 / 1 and captures both poses (`CaptureViewport`, overlays off).
  ⚠️ Note: a new actor in a World Partition map needs `save_assets([])` to write its external package; check `git status` afterwards. Make sure the parked Wrangler doesn't spawn into a rig's lane (placed jeeps simulate in PIE).
  **Done:** 10 stills in the scratchpad, plus a side-by-side sheet with the concept images; the script is saved under `Scripts/MudTextures/photobooth.py` (or noted here) for reuse.
  **User check:** —

- [x] **S5-09** **Checkpoint 1:** dirt look stills (user, 0.5–2 h with iterations) _(requires S5-08)_ <!-- done: 2026-11-08 -->
  ✅ User sign-off on round 2 ("All good for now"). Final sheet: `stills6/stage_sheet.png` (session scratchpad; regenerate with `photobooth.py` + `stage_sheet.py`).
  Round 1 feedback (2026-11-07):
  - Editor scalability is now medium with high shadows (user's change): judge all Phase V captures with it.
  - Scale the effect down: today's 0.8 is the new 1.0 → `DirtScale` 0.8 in `MF_B1MudDirt`.
  - Clean cheat → S5-11 (`B1DirtClean`).
  - A separate tire mud layer → material part here, runtime in S5-20: tread fully coated after one wheel rotation in mud, thicker (towards the rim) the longer it's in mud, thinner again on firm ground; wet = darker, dries lighter. Subtle, not comical.
  Round 2 changes (shot at the new scalability, sheet `stills6/stage_sheet.png` in the session scratchpad):
  - `DirtScale` 0.8 on body dirt and the tire crust (not on the tire layer).
  - Tire branch: tire CPD slot 1 = coat (0.4 = tread band r > `TireTreadRadius` 40 covered; above 0.4 the band edge moves in to `TireRimRadius` 23), slot 2 = wetness (× `DirtTireWetDarken` 0.7). Tire mud is 1.6× the body mud colour: at body colour it vanished on black rubber (a red-colour test showed full coverage on all four tires). New function inputs `TireTreadRadius`/`TireRimRadius` (defaults 40/23), params `DirtScale`, `DirtTireWetDarken`.
  - Rims: the tire mesh's `Metal_part` slot shared `MI_Wrangler_MetalPart` with the body, so it ran the body branch. New `MI_Wrangler_Rim` (copy, `bIsTire` on) on `SM_Wrangler_Tire`.
  - Glass went milky white from 0.5: rough + metallic + specular 1 spreads the sky. Glass roughness now only rises to 0.35 and specular drops to 0.25 under mud.
  - Captures right after a material recompile can show glass missing (shaders still compiling): wait a few seconds.
  The user reviews the stage sheet. Iterate on S5-04 and S5-05 parameters (thresholds, scales, colour) and re-shoot with S5-08's script until the stages read as tires → arches → splatter → spots, and dirt 1 matches the concept.
  **Done:** user sign-off on the stills; final sheet path noted here.
  **User check:** stages and the cap look right next to `jeep_wrangler_mud_reference.jpg`.

### Dirt accumulation

- [x] **S5-10** `B1MudThrow` helper + dirt accumulation (2 h) _(requires S5-07, S5-09)_ <!-- done: 2026-11-08 -->
  Result:
  - `Mud/B1MudThrow` (namespace, plain structs): intensity = lerp(`MinDepthFactor` 0.3, 1, sink / `FullSinkDepth` 15 cm) × clamp(tread / `FullTreadSpeed` 5 m/s + slip / `FullSlipSpeed` 3 m/s); 0 off mud. `GetWheelInput` reads `UB1MudVehicleComponent`.
  - Tread speed: new `FB1MudSimOutput::TreadSpeed` (|`GetWheelGroundSpeed`|, cm/s) → `UB1MudVehicleComponent::GetWheelTreadSpeed`. Inside `Mud/`, no new host requirement.
  - `UB1MudDirtComponent`: `WheelSecondsToFull` 20, `BodySecondsToFull` 80, belly term `BellyBodyRate` 1 × belly depth / `BellyFullDepth` 20 cm × max wheel motion. `GetWheelIntensity`. One `LogB1Mud` line per new tenth: `dirt w=a/b/c/d body=`.
  - Headless mud 22/22 PASS, rig numbers unchanged. Medium rigs reach ~0.1 wheel / 0.02 body in ~2 s, deep 0.1 / 0.04 (belly); firm rigs log no dirt.
  - `Mud/B1MudThrow` (static, plain C++): intensity 0..1 per wheel from sink depth, tread speed and slip.
  - `UB1MudDirtComponent` tick:
    - `WheelDirt[i] += intensity·dt / WheelSecondsToFull` (start 20 s);
    - `BodyDirt += mean intensity·dt / BodySecondsToFull` (start 80 s), plus a belly term in deep mud;
    - all clamped to 1, never decreasing.
  - Reads only `UB1MudVehicleComponent`.
  ⚠️ Note: the tick must not touch the sim or the spray. `PreviewDirt` ≥ 0 overrides the accumulated values (it doesn't reset them).
  **Done:** builds; in PIE a jeep driven by a rig into medium mud shows the values rising in the log; firm ground stays 0.
  **User check:** —

- [x] **S5-11** Dirt debug: Mud window, cheat, telemetry, ShowDebug (2 h) _(requires S5-10)_ <!-- done: 2026-11-08 -->
  Result:
  - Mud window → "Dirt" header (player pawn's `UB1MudDirtComponent`): bars w0–w3 (with throw intensity) and body, target counts, Clean / Max, `Preview dirt (-1 off)`, typed fields for wheel/body seconds-to-full, belly full depth, belly body rate. "Save to asset" copies those four (not `PreviewDirt`, which stays live-only) to the pawn BP's component archetype and saves it.
  - `UB1MudDirtComponent::SetDirt(v)` (all values to v, preview off, can lower) and `Clean()`; cheats `B1Dirt <v>` (negative = clean) and `B1DirtClean` call them. S5-20 extends `SetDirt` to the tire layer.
  - Telemetry: `dirt=w0/w1/w2/w3/body` after `belly=` on the `B1Mud` line (CrossMedium: `dirt=0.16/0.16/0.17/0.17/0.04` at the far edge). ShowDebug B1Mud: `dirt w a/b/c/d  body e  (preview p)`.
  - **Mud window "Dirt" section:**
    - per-wheel and body bars, per-wheel intensity;
    - `DragFloatTyped` for both seconds-to-full values and `PreviewDirt`;
    - Clean / Max buttons;
    - "Save to asset" writes the component archetype on the pawn's own BP class (`BP_B1Jeep_Wrangler`).
  - **Cheat** `B1Dirt <0..1>` (negative = clean) on `UB1CheatManager`.
  - **Cheat** `B1DirtClean` (user request, 2026-11-07): wheel and body dirt and the tire mud layer (S5-20) back to 0. The component gets a `Clean()` the cheat and the ImGui Clean button share.
  - **Telemetry:** `dirt=w0/w1/w2/w3/body` on the `B1Mud` line.
  - **ShowDebug:** a dirt line in `ShowDebug B1Mud`.
  **Done:** builds; the telemetry line shows dirt in a rig run.
  **User check:** Mud window bars move while driving; Clean / Max / `B1Dirt 0.5` work; Save keeps edits after restarting the editor.

- [x] **S5-12** Dirt rig metrics + four dirt rigs (3 h) _(requires S5-10)_ <!-- done: 2026-11-08 -->
  Result:
  - Rig: `StartDirt`; line `dirtW=a/b/c/d dirtBody= dirtLast= dirtTargets=tires/body` on every vehicle with the component; thresholds `Min/MaxWheelDirt` (lowest/highest wheel), `Min/MaxBodyDirt`, `bExpectBodyBehindWheels`, plus `MaxLastStepDirtChange` (`dirtLast` = summed change during the last step, for the cap).
  - New step option `ShuttleDistance` (m): forward at `Throttle`, reverse (brake) once the vehicle is that far past the step start along its heading, and back. Open-loop forward/reverse legs drifted out of the 20 m patch (4/4 s +54 m, 3/4 s backwards in 15 s).
  - Rigs (`Lvl_Gym_Mud`, `B1Tests/Dirt`, Wrangler): `B1Dirt_Firm` (X 80, Y −45 m, 20 s 50 % full lock circle: 0/0), `B1Dirt_Medium` (X 20, Y 29 m, 15 s 50 % shuttle ±6 m: wheels 0.51–0.52, body 0.17), `B1Dirt_Drive90` (same, 90 s: wheels 1, body 1), `B1Dirt_Cap` (deep lane X 14, Y 55 m — medium let it drive out — `StartDirt` 0.95, 20 + 10 s full: all 1, dirtLast 0).
  - Tuning (not thresholds): `WheelSecondsToFull` 20 → 15, `BodySecondsToFull` 80 → 45. Continuous medium driving at 50 % gives ~0.5 throw intensity, so body ≈ 1 at ~87 s.
  - Headless: mud 26/26 (22 + 4 dirt), core 12/12, FullThrottle5s 76.99 m (77.07–77.13 in earlier runs). A `LogHttp` timeout error once failed `B1Dirt_Firm` (its line said PASS); environment flake.
  `AB1GymTestRig`:
  - `StartDirt` (−1 = leave);
  - line fields `dirtW=a/b/c/d dirtBody=` and `dirtTargets=` (tires/body found);
  - thresholds `Min/MaxWheelDirt`, `Min/MaxBodyDirt`, `bExpectBodyBehindWheels`.
  Rigs (Wrangler `VehicleClass`, `Lvl_Gym_Mud` folder `B1Tests/Dirt`):
  - `B1Dirt_Firm` (20 s at 50 % on firm → all 0);
  - `B1Dirt_Medium` (15 s at 50 % → wheels ≥ 0.4, body < min wheel);
  - `B1Dirt_Drive90` (90 s at 50 % in medium, back and forth → body ≥ 0.9, wheels 1);
  - `B1Dirt_Cap` (`StartDirt` 0.95, then 30 s full throttle → all exactly 1, unchanged over the last 10 s).
  ⚠️ Note: if `Drive90` misses, tune the seconds-to-full values, not the threshold, and record the values. Thresholds check order and cap, not exact numbers.
  **Done:** the 4 dirt rigs PASS headless; mud suite 22/22 and core 12/12 unchanged.
  **User check:** —

- [x] **S5-20** Tire mud layer: coat + wetness (2.5 h) _(requires S5-10)_ <!-- done: 2026-11-08 -->
  Result:
  - `UB1MudDirtComponent` (`Mud|Dirt|Tire`): `TreadCoat` 0.4, `TireCoatSecondsToFull` 20, `TireShedDistance` 150 m, `TireDrySeconds` 40. On mud the coat rises by tread distance (|ω·r|, so spin coats faster) to 0.4 in one 2πr, then by time to 1; off mud (or airborne) it sheds by tread distance and the wetness dries. Tire CPD slot 1 = coat, 2 = wetness; `PreviewDirt` drives both; `SetDirt`/`Clean` reset both. `GetTireCoat/Wetness`.
  - Mud window Dirt section: per-tire coat/wet bars and typed fields for the four values (in "Save to asset").
  - Rig: `tireCoat=a/b/c/d coatPeak= coatDrop= coatDist=` (worst wheel's path from its first mud contact to `TreadCoat`), thresholds `MinTireCoatPeak`, `MinTireCoatDrop`, `MaxTireCoatDistance`. `B1Dirt_TireCoat` (X 2, Y 29 m, 12 s 50 % through medium, then ~75 m firm): coatDist 2.86 ≤ 3, peak 0.49, end 0 (drop 0.49 ≥ 0.1).
  - ADR-021 CPD table and MIGRATION.md updated. Headless mud 27/27, core 12/12.
  User request at checkpoint 1. Per wheel, in `UB1MudDirtComponent`, on top of the slow `WheelDirt` crust:
  - `TireCoat` 0..1: on mud, rises by rolled distance so the first wheel rotation (2πr) takes it to `TreadCoat` (0.4 = tread fully covered), then keeps thickening by time in mud to 1 (`TireCoatSecondsToFull`, start 20 s). Off mud it falls by rolled distance (`TireShedDistance`, start 150 m to 0).
  - `TireWetness` 0..1: 1 while touching mud, then dries over time (`TireDrySeconds`, start 40 s).
  - CPD on tires: slot 1 = coat, slot 2 = wetness (the material reads them in S5-09's tire branch). `PreviewDirt` drives both. Clean resets both.
  - Rig fields `tireCoat=a/b/c/d` and checks: entering medium mud gives ≥ 0.4 on every wheel within ~3 m; driving 50 m on firm afterwards lowers it.
  ⚠️ Note: ADR-021's CPD table gets the tire slots 1–2; MIGRATION.md too.
  **Done:** builds; rig line shows the coat rising on entry and falling on firm ground.
  **User check:** drive into mud: tires go muddy almost at once and darker; drive on firm ground: they lighten and thin out, without looking comical.

### Spray

- [x] **S5-13** Spray rate from `B1MudThrow` + `User.Intensity` (1.5 h) _(requires S5-10)_ <!-- done: 2026-11-08 -->
  Result:
  - Rate = intensity × `MaxSpawnRate` (400, also the per-wheel cap), × `RollingRateFactor` 0.4 when |slip| < `MinSlipSpeed`. `SpawnRatePerSlip` removed. Speed formula unchanged; a rolling wheel throws back along its travel (sign of velocity · forward), a spinning one opposite its spin as before.
  - `User.Intensity` set every tick; `GetWheelIntensity`. Telemetry `spray=rate/intensity`. MIGRATION.md: the fourth user param and the rate rule.
  - Headless mud 27/27 (`s513_mud.log`): medium wheel with slip 0.00 at sink 16.9 → `spray=27/0.17`. With `-ini:Engine:[ConsoleVariables]:b1.Mud.Spray=0`, `B1Dirt_Medium` gives `spray=0/0.00` and the same dirt (wheels 0.52, body 0.172).
  - Unity build fix on the way: `CmPerM` in `B1MudThrow.cpp` clashed with `B1VehicleSimulation.cpp` → `B1MudThrowPrivate::CmPerM`.
  - `UB1MudSprayComponent`: rate = f(`B1MudThrow` intensity), capped per wheel, so a rolling wheel in mud sprays too.
  - New `User.Intensity` (0..1).
  - Velocity stays with today's rules.
  - Update the `spray=` telemetry and MIGRATION.md (new user param).
  ⚠️ Note: `b1.Mud.Spray 0` must not change dirt (dirt doesn't read the spray). Check it in a rig run with the CVar off, via an `ini` / `-ExecCmds` on the headless runner, or skip it if the agent can't set CVars.
  **Done:** builds; telemetry shows spray > 0 for a wheel rolling in medium mud with |slip| < `MinSlipSpeed`.
  **User check:** —

- [x] **S5-14** `NS_B1MudSpray`: clump emitter (3 h) _(requires S5-02, S5-04)_ <!-- done: 2026-11-08 -->
  Result: emitter `Spray` (GPU) now renders `SM_B1MudClump` with `M_B1MudClump` (new: opaque, lit, ParticleColor RGB, roughness 0.2, specular 0.6, no shadows); sprite renderer removed. `User.Intensity` added (default 1 so the asset previews).
  - Size: Mesh Scale Mode Uniform = `lerp(1.1, 6.5, pow(rand(1.0f), 3.0)) * lerp(0.6, 1.0, saturate(User.Intensity))` (clump ≈ 2 cm across, so ~2–12 cm, skewed small).
  - Rotation: `InitialMeshOrientation` Random; `UpdateMeshOrientation` axis = velocity × up, rate = `saturate(|v| / 300) * 3`, so it tumbles in flight and stops at rest.
  - Lifetime 0.6–1.2 s; `ScaleMeshSize` = `saturate((1 - NormalizedAge) * 4)` (shrinks over the last 25 %). Collision: depth buffer, radius from mesh, restitution 0, friction 0.9, rest state on. Gravity, drag, `SpawnRate`/`SprayVelocity`/`Color` bindings unchanged.
  - Capture (Simulate, `B1Mud_DigInMedium` temporarily auto-run with the Wrangler, reverted): `spray/clump_b.png` and `clump_b_crop.png` in the session scratchpad: mixed clumps off the rear tread, landing behind the wheel. They read a bit light/grey in sun; colour is for S5-17 / checkpoint 2.
  Replace the sprite emitter with GPU mesh particles (`SM_B1MudClump`):
  - size 2–12 cm, skewed (e.g. `pow(rand, 3)`), scaled by `User.Intensity`;
  - random rotation and spin rate, gravity, drag;
  - depth-buffer collision with high friction and no bounce; scale-out after resting;
  - lifetime 0.6–1.2 s;
  - material: a mesh variant of `M_B1MudSpray` (lit, wet, `User.Color`).
  Fixed bounds as today.
  ⚠️ Note: keep the existing user param names (`SpawnRate`, `SprayVelocity`, `Color`) so the component keeps working between tasks.
  **Done:** a capture from a PIE rig with wheelspin shows small mixed-size clumps landing and resting.
  **User check:** —

- [x] **S5-15** `NS_B1MudSpray`: droplets + haze emitters (2 h) _(requires S5-14)_ <!-- done: 2026-11-08 -->
  Result: two emitters from `/Niagara/DefaultAssets/Templates/Emitters/Fountain`, both cone velocity on `User.SprayVelocity`, sphere 10 cm spawn.
  - `Droplets`: GPU, fixed bounds as `Spray`; rate `User.SpawnRate * 2`; life 0.3–0.6 s; non-uniform size `float2(1, 2.5) * lerp(0.5, 3, pow(rand, 2))` cm, `VelocityAligned`, `M_B1MudSpray`, `User.Color`; speed ×0.9–1.25, cone 30°, drag 0.5, gravity −980.
  - `Haze`: CPU; rate `User.SpawnRate > 0 ? 8 * saturate(User.Intensity) : 0` (≤ 8/s per wheel); life 1–2 s; 50–150 cm; colour = `User.Color` lerped 50 % to (0.55, 0.5, 0.45), alpha `0.13 * lerp(0.5, 1, Intensity)`; speed ×0.3, cone 40°, drag 4, gravity −50. New `M_B1MudHaze` (translucent lit, sphere-mask disc × particle alpha, depth fade 60 cm).
  - Compiles with no errors or warnings. Captures in the session scratchpad `spray/`: `full_a_vp.png`, `full_b_vp.png` (all three), `near_a_vp.png` (faint haze puff behind the front tire), `droponly_a_vp.png` (clumps and haze hidden: fine droplet streaks). Droplets are small at 3–4 m, haze is subtle; both are tuning for S5-17 / checkpoint 2.
  - `stat Niagara` counts: not taken (no console exec from the agent); user-side.
  - **Droplets:** GPU sprites 0.5–3 cm, velocity-aligned, ~2× the clump rate, short life.
  - **Haze:** CPU or GPU, a handful of soft translucent sprites (50–150 cm, opacity ≤ 0.15, `User.Color` lightened), slow, high drag, rate scaled by `User.Intensity`.
  ⚠️ Note: cap the haze spawn (≤ 8/s per wheel) to keep overdraw down. It's tuning, not optimisation.
  **Done:** a capture shows the rooster tail with fine droplets and a faint haze; `stat Niagara` counts noted (user-side, or from the log if available).
  **User check:** —

- [x] **S5-16** Spray section in the Mud window (1.5 h) _(requires S5-13)_ <!-- done: 2026-11-08 -->
  Result: Mud window → "Spray" header (player pawn's `UB1MudSprayComponent`): per-wheel rate bar (rate / `MaxSpawnRate`, with throw intensity), typed fields for `MinSlipSpeed`, `MaxSpawnRate`, `RollingRateFactor`, `SpraySpeedPerSlip`, `Min/MaxSpraySpeed`, `SprayElevation`; "Save to asset" copies them to the pawn BP's component archetype (same path as Dirt). Builds; core 12/12 (`s516_core.log`). The render check is left to the user check (the agent can't connect a NetImGui client).
  - Fix found at the user check: the `NetImGuiLaunch` cheat crashed the editor. UE 5.8's `FPlatformProcess::CreateProc` asserts on `nullptr` Parms (`WindowsPlatformProcess.cpp:542`), so it now passes `TEXT("")`.
  Per-wheel intensity and rate; `DragFloatTyped` for the `Mud|Spray` config (`MinSlipSpeed`, rates, speeds, elevation, rolling factor); "Save to asset" to the pawn's own BP class.
  **Done:** builds; the section renders (smoke check in a PIE log or capture).
  **User check:** edits change the spray live; Save keeps them after a restart.

- [x] **S5-17** Spray capture rig + reference comparison (1.5 h) _(requires S5-14, S5-15)_ <!-- done: 2026-11-08 -->
  Result:
  - Rig `B1Spray_Medium` (`Lvl_Gym_Mud`, folder `B1Tests/Spray`): X 15, Y 29 m (the `DigInMedium` spot), Wrangler, one 5 s full-throttle step, no thresholds. Saved with `bAutoRun` **off**: saved on, it would also start in headless runs and drive into the medium lane next to the other rigs. The capture script turns it on, captures in Simulate, then turns it off again. Headless mud 28/28 (`s517_mud.log`): `dist=23.33 vmax=13.40`, out of the patch by the end.
  - Camera (x, y, z cm, pitch, yaw): (3300, 2450, 110, −3, 158), low front ¾ left like the clip. Captures: warmup 2.5 s → t=1 s, +2 s → t=3 s (one call), warmup 3.5 s → t=2 s (second call); at most 2 captures per call.
  - `Scripts/MudTextures/spray_sheet.py <out_dir> <result files…>` decodes the `cNN` captures, crops the 3D view out of the editor shot (it's only ~680×440) and puts them next to 3 frames of `mud-car-drive.mp4` (only 30 frames, 1.25 s). Needs `opencv-python-headless`. Sheet: `spray3/spray_sheet.png` in the session scratchpad.
  - Comparison for checkpoint 2: our spray is a low, flat trail at wheel height behind the rear tire; the clip's is a rooster tail rising above the roof and fanning out back and up. Clumps read grey or light-edged (clip: dark brown); no haze visible at this distance (clip: a large tan cloud). The Wrangler speeds up fast (13 m/s by 5 s), so at t=2–3 s it's mostly rolling throw, not spin.
  `B1Spray_Medium` (`bAutoRun`, Wrangler, full throttle from rest on the medium lane of `Lvl_Gym_Mud`). Set the camera before `StartPIE` (mcp-tools.md) at an angle similar to `mud-car-drive.mp4`, then `CaptureEditorImage` at about 1, 2 and 3 s. Build a side-by-side sheet with the reference frames (scratchpad OpenCV, or re-extract with `Scripts/MudTextures`).
  **Done:** comparison sheet saved and noted here; the rig passes its smoke check (no thresholds).
  **User check:** —

### Lens mud (added 2026-11-08)

User request before checkpoint 2: hard wheelspin splashes the chase camera's lens, the mud builds up and fades, capped at `Concept/mud-on-the-screen.jpg`. Design: [plans/2026-11-08-mud-lens.md](plans/2026-11-08-mud-lens.md) (revision 1). Decision: ADR-022. Same look-first order: component → textures → material → stills → **lens checkpoint** (S5-25) → debug and rigs.

- [x] **S5-21** Spray getters + `UB1MudLensComponent` core (3.5 h) <!-- done: 2026-11-08 -->
  Result:
  - Spray: `GetNumWheels`, `GetWheelSprayOrigin` (emitter world location), `GetWheelSprayVelocity` (world launch velocity), zero when not spraying. Behaviour unchanged.
  - `Mud/B1MudLensComponent` on `AB1Jeep` as `MudLens` (`GetMudLens()`); `AB1Jeep` tags `BackCamera` with `B1MudLens` (inherited on `BP_B1Jeep_Wrangler`; its PP is empty, `WeightedBlendables` []). Ticks after the spray (tick prerequisite). Per-wheel exposure struct (intensity, throw, aim, distance, launch speed, aim angle), slots, `RandomSeed` 22, getters, `SetLensDirt`/`CleanLens`/`Splash`, `PreviewLensDirt`, CVar `b1.Mud.Lens`. Log `lens d= exp= splashes=` per new tenth, with one tenth of slack (at the cap, fading between splashes re-logged every splash: 51 lines → 10 in `B1Dirt_Cap`). Camera PP blendables apply from the next `GetCameraView`, no refresh needed.
  - `UB1MudSettings::LensMaterial` added, **left unset** in `DefaultGame.ini` until S5-23 makes the MI (a missing asset path would warn on every load). Unset: logs once, the lens still simulates.
  - Headless instead of the PIE run (same check): mud 28/28 (`s521_mud.log`), numbers unchanged. Lens lines: `B1Spray_Medium` (Wrangler, full throttle at the DigInMedium spot) up to 0.52; buggy `DigInMedium` / `ExitMediumFull` 0.32, `SinkMedium` / `RecoverMedium` 0.2; deep spins `DeepFullThrottle` / `Fuel_DeepFull` 0.62, `Dirt_Cap` (30 s stuck in deep) 1.0 at exp ~0.62. None in `FirmSmoke`, any firm rig, `Dirt_Medium`/`Drive90` (50 % shuttle), `CrossMedium` or `CrawlIntoMedium`. Core 12/12 (`s521_core.log`, FullThrottle5s 77.10 m).
  - `UB1MudSprayComponent`: read-only `GetWheelSprayOrigin` / `GetWheelSprayVelocity` (world, cm/s, zero when not spraying). Spray behaviour unchanged.
  - `Mud/B1MudLensComponent` (portable) on `AB1Jeep` as `MudLens`:
    - targets: camera components tagged `B1MudLens` (host tags `BackCamera` in `AB1Jeep`'s constructor); only an `IsActive()` camera accumulates and shows mud;
    - MID of `UB1MudSettings::LensMaterial` (new soft ref, default `/Game/B1/FX/MI_B1LensMud`; null → log once, no lens) appended to the camera's `PostProcessSettings.WeightedBlendables`, removed in EndPlay;
    - exposure per wheel = intensity × smoothstep(`LensMinThrowSpeed` 350, `LensFullThrowSpeed` 500, launch speed) × aim (1 to `LensAimFull` 75°, 0 at `LensAimZero` 110°, horizontal angle between throw and emitter → camera) × `1 / (1 + (d / LensFalloffDistance 800 cm)²)`; E = max over the wheels;
    - splashes while E > `LensSplashThreshold` 0.1, every `LensSplashInterval` 0.25 s ±40 % (seeded `FRandomStream`), amount = interval × E × 4 / `LensSecondsToFull` 4 s;
    - 4 slots (strength, offset UV, variant 0–7, time since hit): emptiest slot (< 0.05) gets a new placement, else the weakest is topped up; hold `LensHoldSeconds` 1.5 s, then linear fade over `LensFadeSeconds` 6 s; MID params `LensSlot0..3` (strength, offset U, offset V, variant) written on change;
    - `LensDirt` = mean strength; `PreviewLensDirt` (−1 off); `SetLensDirt(v)`, `CleanLens()`, `Splash()`; getters for exposure factors per wheel and the splash count;
    - CVar `b1.Mud.Lens`; one `LogB1Mud` line per new tenth: `lens d= exp=`.
  - MIGRATION.md: component, tag, setting, MI.
  🔍 Research first: `UCameraComponent::PostProcessSettings.WeightedBlendables` (`FWeightedBlendable`) with a MID, and whether a camera PP change needs `bUsePawnControlRotation`-style refresh or applies next frame; check `BackCamera` on `BP_B1Jeep_Wrangler` has no PP set today.
  ⚠️ Note: new class + UPROPERTYs → `Scripts/BuildEditor.ps1`. The material doesn't exist yet: the component must run with a null `LensMaterial` (exposure and slots still work, so S5-27 can measure before S5-23). Portability: no project includes; the tag and default come from the host / settings.
  **Done:** builds; a PIE run of `B1Mud_DigInMedium` with the Wrangler (temporary `VehicleClass`/`bAutoRun`, reverted) logs `lens` lines rising during wheelspin; `B1Gym_FirmSmoke` logs none; core 12/12 and mud 28/28 headless unchanged.
  **User check:** —

- [x] **S5-22** Lens texture generator (2 h) <!-- done: 2026-11-08 -->
  Result: `Scripts/MudTextures/gen_lens_textures.py` (seed 22, ~20 s) + README section. Per slot: 20 drops (r 8–34 px, `u^1.6`), 260 specks (r 1.3–5.5 px, 45 % in 14 clusters); 4 slots ≈ the concept's ~70 drops / ~1000 specks on a 16:9 screen. Background id = 255 exactly. Variants: bit 0 flip U, bit 1 flip V, bit 2 invert reveal order (a 2:1 tile can't rotate 90°). Imported `T_B1LensDrops` / `T_B1LensSpecks` 2048×1024, sRGB off, `TC_VectorDisplacementmap`, no mips, `TEXTUREGROUP_UI`. Contact sheet (`Saved/MudTextures/lens/lens_contact_sheet.png`, a numpy stand-in of the material): first pass too small and too few visible (most specks under 1 px); sizes raised. In the stand-in the specks read too black and hard: that's speck colour/alpha in the MI (S5-23/25). The stand-in uses drop fill 1.6, so drops don't shrink to dots at 0.25. Output now goes to `Saved/MudTextures/` (git-ignored) instead of the session scratchpad.
  `Scripts/MudTextures` (fixed seed, README section):
  - `T_B1LensDrops` 2048×1024: RG = drop normal xy (0.5-biased), B = dome height, A = per-drop reveal id (histogram-equalized). Sizes skewed small, a few large, about a quarter of the concept's drop count.
  - `T_B1LensSpecks` 2048×1024: R = speck mask, G = per-speck reveal id. Many 2–8 px specks at 1080p, some clustered, a quarter of the concept's count.
  - Contact sheet next to `Concept/mud-on-the-screen.jpg` (all four quarters stacked = full density).
  Import into `/Game/B1/FX/Lens/` with `TextureTools.import_file`: linear (sRGB off), uncompressed / `TC_VectorDisplacementmap` (so ids and normals don't band), no mips needed (screen-space 1:1).
  ⚠️ Note: `import_file` won't overwrite: delete, re-import, reassign. Tiles must wrap seamlessly in both axes (random slot offsets wrap).
  **Done:** assets imported and saved; `get_size` is 2048×1024; contact sheet checked; README re-runnable.
  **User check:** —

- [x] **S5-23** `M_B1LensMud` + `MI_B1LensMud` (3.5 h) _(requires S5-22)_ <!-- done: 2026-11-08 -->
  Result:
  - `/Game/B1/FX/M_B1LensMud`: `MD_PostProcess`, `BL_SceneColorAfterTonemapping`, Emissive = one Custom node (`Scripts/MudTextures/M_B1LensMud.hlsl`, 17 inputs). A `SceneTexture` PostProcessInput0 node feeds `Scene`, which also binds the input for `SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(uv, 14), 14), 14, true)` at the refracted UV. Aspect from `ViewSize`, UV from `ScreenPosition.ViewportUV`. Only the topmost drop per pixel refracts. Specks are a union over the slots.
  - Params (group `Lens Mud`): `LensSlot0..3`, `LensDrops`/`LensSpecks` (sampler `LinearColor`), `LensRefraction` 0.03 (screen heights), `LensDropFill` 1.6, `LensDropTint` (0.30, 0.22, 0.13) × `LensDropTintAmount` 0.35, `LensRimDarken` 0.6, `LensHighlight` 0.4, `LensSpeckColor` (0.07, 0.05, 0.03), `LensSpeckOpacity` 0.85.
  - `MI_B1LensMud` (slots 0) and `MI_B1LensMud_Preview` (all 4 slots at 1, offsets/variants (0.13, 0.71, 0), (0.58, 0.29, 3), (0.87, 0.52, 5), (0.34, 0.06, 6)). `DefaultGame.ini` `LensMaterial=/Game/B1/FX/MI_B1LensMud`. Compiles with no shader errors in the log.
  - MCP: a Custom node's `Inputs` grows only by appending the element **as read back** (full struct), one at a time; a bare `{inputName}` copy is "ambiguous".
  `/Game/B1/FX/M_B1LensMud`: post-process domain, blendable location after tonemapping. One Custom node (`Scripts/MudTextures/M_B1LensMud.hlsl`, keep in sync) over the 4 slots:
  - UV = screen UV with aspect (fit 2:1 tile to screen height), variant rotation/mirror, slot offset, wrap;
  - specks: id < strength, alpha fades with strength, `LensSpeckColor`;
  - drops: id < strength and height > 1 − strength × `LensDropFill`; scene sampled at UV − normal × `LensRefraction`, tinted to `LensDropTint`, darker rim from normal length, small highlight;
  - early-out to the scene colour when every slot strength is 0.
  `MI_B1LensMud` (game default, slots 0) and `MI_B1LensMud_Preview` (slots set by hand). Set `UB1MudSettings::LensMaterial` default to the MI.
  🔍 Research first: sampling `PostProcessInput0` at an offset UV from a Custom node in 5.8 (SceneTexture node input vs `SceneTextureLookup`), `ViewSize` / `ScreenPosition` for aspect.
  ⚠️ Note: Custom node inputs through MCP follow the array rule (set element 0, append copies, rename). Slot params are vector params, so the MID writes 4 values per slot.
  **Done:** material and both MIs compile with no errors; the `LensMaterial` setting default points at `MI_B1LensMud`.
  **User check:** —

- [x] **S5-24** Lens preview volume + stills + PIE confirmation (1.5 h) _(requires S5-21, S5-23)_ <!-- done: 2026-11-08 -->
  Result:
  - `B1PhotoBooth_LensPreview` (`PostProcessVolume_UAID_7CB566F64C2FEE1203_1507231727`, `B1Tests/PhotoBooth`): bound, scale 30 (so its brush edges stay out of frame), priority 10, blendable `MI_B1LensMud_Preview`, saved **disabled**. A volume spawned through `SceneTools.add_to_scene_from_class` does get a brush (its edges showed at scale 2).
  - Chase pose (back camera: arm 680, 95 + 160 cm up): (−1713, −13616, 257), pitch −7, yaw 115.
  - `photobooth.py` `MODE = 'lens'`: `LENS_LEVELS` (`None` = volume off), sets all 4 slots of the preview MI, then restores slots to 1, disables the volume and saves the MI. `stage_sheet.py --lens <out> <results…>` → `lens_sheet.png` next to the concept; it prints non-image keys.
  - Look pass before the stills: specks were too big/black/hard → generator speck radius 0.8–3.4 px with 1.3 px soft edge (re-imported), `LensSpeckColor` (0.13, 0.09, 0.05), `LensSpeckOpacity` 0.8. Drops read as grey glass → `LensRefraction` 0.05, `LensDropTint` (0.32, 0.22, 0.12) × 0.25, `LensRimDarken` 0.85, `LensHighlight` 0.25 (parent material defaults).
  - Sheet: `Saved/MudTextures/lens/stills1/lens_sheet.png`. `l000` vs `lnone`: mean diff 0.7/255, 0.8 % of pixels > 8, mostly in the sky (moving clouds, the same noise floor as S5-06); the shader early-outs at 0.
  - PIE (floating window, `PreviewLensDirt` 1 on the Wrangler CDO's `MudLens`, reverted): mud over the game view, HUD clean on top (`Saved/MudTextures/lens/pie/c_pie.png`). `UB1MudSettings::LensMaterial` was also set on the running editor's CDO, since the ini changed after startup. `StartPIE` takes `{"options": {"bSimulate", "playMode"}}`; `CaptureEditorImage` returns `{data, mimeType}` directly.
  - Core 12/12 with the volume placed (`s524_core.log`, FullThrottle5s 76.98 m). The editor didn't close for the run (a save-changes dialog for an unknown package; none of the lens assets were dirty), so the run went with the editor open.
  - Bound `PostProcessVolume` `B1PhotoBooth_LensPreview` in `Lvl_Gym_Core` (`B1Tests/PhotoBooth`) around a new **chase pose** behind the booth Wrangler (like the back camera: ~6.8 m behind, pitch −7°), blendable `MI_B1LensMud_Preview`, **saved disabled**.
  - `photobooth.py` lens mode: enable the volume, set slot strengths for `LensDirt` 0 / 0.25 / 0.5 / 0.75 / 1, `CaptureViewport` from the chase pose, disable. `stage_sheet.py` lens mode: stills next to the concept image.
  - One PIE capture: `PreviewLensDirt` 1 on the player's jeep (set before `StartPIE`, reverted), `CaptureEditorImage`: mud under a clean HUD.
  ⚠️ Note: new actor in a WP map → `save_assets([])`, check `git status`. At most 2 captures per reply. Scalability medium / shadows high. Restored asset-editor tabs can sit in front of the PIE capture (HISTORY 2026-11-08): close them first.
  **Done:** 5 stills + sheet in the scratchpad; the 0 still equals the volume-disabled capture; the PIE capture shows the effect with the HUD on top; the core suite still 12/12 with the volume placed.
  **User check:** —

- [x] **S5-25** **Lens checkpoint:** look stills (user, 0.5–1.5 h with iterations) _(requires S5-24)_ <!-- done: 2026-11-08 -->
  ✅ User sign-off on round 3. Final sheet: `Saved/MudTextures/lens/stills4/lens_sheet.png` (regenerate with `photobooth.py` `MODE = 'lens'` + `stage_sheet.py --lens`).
  Round 1 feedback (2026-11-08, sheet `stills1`): drops too dark, black outline (only slightly brown wanted), too sharp (should be blurred), too regular (almost all round). The small black specks were an artifact of the concept image: drops only. Fewer drops: the old 0.75 count is the max.
  Round 2 changes (sheet `Saved/MudTextures/lens/stills2/lens_sheet.png`):
  - Specks removed everywhere: generator, `T_B1LensSpecks` deleted, shader, params `LensSpecks`/`LensSpeckColor`/`LensSpeckOpacity`. `LensRimDarken` removed too (no dark rim).
  - Generator: 15 drops per slot, r 9–36 px, aspect 1–1.5, wobbly outline (2nd/3rd/5th harmonics 0.11/0.06/0.03), 40 % get a merged satellite blob (0.35–0.7 r). B = 1 − normalized radius, so the edge fades in the shader.
  - Shader (Custom node rebuilt, 16 inputs): edge fade over `LensDropSoftness` 0.3; refracted scene blurred with 9 taps at `LensDropBlur` 0.004 screen heights; light brown towards the edge (`LensDropTint` × `LensDropTintAmount` 0.3 × (0.25 + 0.75 n²)); soft highlight `LensHighlight` 0.12; blended at `LensDropOpacity` 0.85. `LensRefraction` 0.04, `LensDropFill` 2.5 (drops are full size from strength 0.4; at 1.3 they had vanished at 0.25–0.5).
  - A capture right after a material recompile showed a brown blurred frame round the screen: a transient, gone in the re-shot.
  Round 2 feedback: too invisible, should look muddy (brown near some edges); a few more and slightly larger drops low on the screen, where most mud lands; 0.25 shows almost nothing, medium drops should come first.
  Round 3 changes (sheet `Saved/MudTextures/lens/stills4/lens_sheet.png`):
  - Generator: 26 drops per slot; reveal order medium-first (score = 0.5·rand + |size − 0.4|), tiny and largest later.
  - Shader: new params `LensBottomScale` 1.2 (tile zoomed towards the bottom by UV.y², so drops there are 1.2× larger), `LensTopDensity` 0.45 (drops shown where id < strength^γ × lerp(TopDensity, 1, UV.y²): ~12 per slot at the top, ~18 per slot at the bottom after the zoom), `LensRevealGamma` 0.5 (0.25 already shows half the drops). Mud: brown tint weight = `LensDropTintAmount` 0.85 × per-drop muddiness (0.35–1 from its id) × (0.35 + 0.65 · n² · lower-edge weight); `LensDropTint` (0.30, 0.20, 0.10), `LensDropOpacity` 0.95. `LensDropFill` 5 (full size from strength 0.2; at 2.5 the 0.25 drops were faint and small), also on `MI_B1LensMud`.
  - First try with `LensBottomScale` 1.35 and 18 drops gave *fewer* drops at the bottom: zooming spreads them out, so density has to beat zoom².
  The user reviews the lens sheet. Iterate on the generator (S5-22: counts, sizes, clustering) and the MI params (tint, refraction, speck colour, drop fill) until 1 matches `Concept/mud-on-the-screen.jpg` and the in-between levels read as "a few splashes", not rain.
  **Done:** user sign-off; final sheet path noted here.
  **User check:** the lens look at 0.25–1 next to the concept image.

- [x] **S5-26** Lens debug: Mud window, cheats, telemetry, ShowDebug (2 h) _(requires S5-21)_ <!-- done: 2026-11-08 -->
  Result: Mud window "Lens" section (`B1ImGuiMud.cpp` `DrawLens`): bar, exposure/splashes/cameras line, slot table (hold = time left, fade = strength × `LensFadeSeconds`), per-wheel exposure table, Splash/Clean/Max, typed preview, 10 typed `Mud|Lens` floats, "Save to asset##Lens" (`SaveLensDefaults`, not `PreviewLensDirt`/`RandomSeed`/`TargetTag`). Cheats `B1Lens <v>` / `B1LensSplash`. Telemetry `lens=d/exp` after `dirt=`; ShowDebug `lens d  exp  slots` (+ preview). Builds; mud 28/28 headless (`s526_mud.log`), `lens=` on the Wrangler rigs (`B1Dirt_Cap` up to 1.0). MIGRATION.md §7 updated. The first build attempt hit a save-changes dialog for packages not touched here; the user closed the editor.
  - Mud window → "Lens": `LensDirt` bar; slot table (strength, hold/fade, offset, variant); per wheel exposure with throw / aim / distance / intensity factors, launch speed and aim angle; splash count; `DragFloatTyped` for every `Mud|Lens` float; Splash / Clean / Max buttons; "Save to asset" to the pawn BP's component archetype (same path as Dirt and Spray).
  - Cheats `B1Lens <v>` (negative = clean) and `B1LensSplash` on `UB1CheatManager`.
  - Telemetry `lens=<dirt>/<exposure>` on the `B1Mud` line. ShowDebug B1Mud: `lens d  exp e  slots a/b/c/d`.
  **Done:** builds; telemetry shows `lens=` in a rig run; MIGRATION.md §7 lists the section and fields.
  **User check:** while flooring it in medium the Lens section's exposure and bar move; Splash / Clean / `B1Lens 0.5` work; Save keeps edits after an editor restart.

- [x] **S5-27** Lens rig metrics + four lens rigs (3 h) _(requires S5-21)_ <!-- done: 2026-11-08 -->
  Result:
  - `AB1GymTestRig`: `StartLensDirt`; thresholds `Min/MaxLensPeak`, `MaxLensEnd`, `Min/MaxLensSplashes`, `MaxLensExposure`; line fields `lensPeak= lensEnd= lensSplashes= lensExpPeak= throwPeak= lensCams=` on every vehicle with a `UB1MudLensComponent`. `throwPeak` is read from the spray (max launch speed over the wheels, cm/s), so it doesn't need an active camera. Unpossessed rig jeeps have an active `BackCamera` (`lensCams=1` everywhere).
  - Rigs (`Lvl_Gym_Mud`, `B1Tests/Lens`, Wrangler): `B1Lens_Firm` (8000, −4500; 20 s 50 % steer 1; splashes ≤ 0, exposure ≤ 0), `B1Lens_Roll` (CrossMedium spot; 10 s 50 %; peak ≤ 0.1, exposure ≤ 0.1), `B1Lens_Spin` (DigInMedium spot; 4 s full; ≥ 3 splashes, peak ≥ 0.3), `B1Lens_Cap` (Spin + `StartLensDirt` 0.9; peak ≤ 1), `B1Lens_Fade` (Spin + 10 s idle; peak ≥ 0.3, end ≤ 0.05).
  - Measured (`s527_measure.log`, no thresholds): rolling throw peaks 150 (Roll, CrossMedium, CrawlInto, Restart*) to 291 cm/s (`Dirt_Drive90`), `ExitDeep` 283; firm 0. Wheelspin: buggy medium 547–555, Wrangler medium 675 (exp 0.68, 14 splashes in 4 s, peak 0.52), deep 900 (exp 0.57). `LensMin/FullThrowSpeed` 350/500 sit in the 291–547 gap: defaults unchanged. Cap: 1.000, Fade: 0.000 after 10 s idle.
  - Headless: mud 33/33 (`s527_mud.log`), core 12/12 (`s527_core.log`, FullThrottle5s 77.31 m), winch 36/36 (`s527_winch.log`).
  - MCP: `save_actor` on a new WP actor still fails after a `save_assets([])` and a restart ("Asset does not exist"); `save_assets([])` works.
  `AB1GymTestRig`: `StartLensDirt` (−1 = leave); line fields `lensPeak= lensEnd= lensSplashes= lensExpPeak= throwPeak=` on vehicles with the component; thresholds `Min/MaxLensPeak`, `MaxLensEnd`, `Min/MaxLensSplashes`, `MaxLensExposure`.
  Rigs (`Lvl_Gym_Mud`, `B1Tests/Lens`, Wrangler):
  - `B1Lens_Firm`: 20 s at 50 % on firm → 0 splashes, exposure 0;
  - `B1Lens_Roll`: 50 % cross of medium → peak ≤ 0.1 and below Spin's;
  - `B1Lens_Spin`: 4 s full throttle from rest at the DigInMedium spot → ≥ 3 splashes, peak ≥ 0.3; a second rig `B1Lens_Cap` with `StartLensDirt` 0.9 never shows > 1;
  - `B1Lens_Fade`: spin 4 s, then 10 s idle → end ≤ 0.05.
  **Measure first:** run Firm / Roll / Spin (plus a deep spin) with no thresholds, record `throwPeak` and `lensExpPeak` here, then set the `LensMin/FullThrowSpeed` defaults and the thresholds.
  ⚠️ Note: check first that the rig's own jeep has an active `BackCamera` without a controller (exposure is measured from it). If Roll splashes, tune the defaults, not the thresholds. Rigs on one lane collide in PIE: headless runs only, or PIE in rounds.
  **Done:** measured numbers noted here; the 5 lens rigs PASS headless; core 12/12, mud 28/28 (+5), winch 36/36 unchanged.
  **User check:** —

- [x] **S5-28** Lens docs pass (1 h) _(requires S5-25, S5-26, S5-27)_ <!-- done: 2026-11-08 -->
  Result: ADR-022 "As built" replaces the interim note: look, measured throw speeds (band 350/500 kept), rates and clear time, camera, debug and rigs. MIGRATION.md checked: §3 `LensMaterial`, §4 material/MIs/texture, §5 component + `B1MudLens` tag + spray getters, §7 debug (S5-26), §9 rigs (S5-27); nothing missing. ARCHITECTURE.md through the `/save` that closes this session.
  ADR-022 "As built" note (measured throw speeds, final defaults); MIGRATION.md complete (component, `B1MudLens` tag, `LensMaterial` setting, `M_B1LensMud` + MIs + textures, spray getters, debug, rigs). ARCHITECTURE.md through `/save`.
  **Done:** MIGRATION.md lists every lens host requirement; `/save` run.
  **User check:** —

### Close

- [x] **S5-18** Docs pass (1 h) _(requires S5-12, S5-17)_ <!-- done: 2026-11-08 -->
  Result: MIGRATION.md already had the component, `B1MudThrow`, tag, CPD layout, `MF_B1MudDirt` + textures, `User.Intensity`, `SM_B1MudClump`. Added: the three spray emitters (§4), dirt/spray in ShowDebug, the ImGui Dirt/Spray sections and the telemetry fields (§7), the dirt and spray rigs (§9). ADR-021 gets an "As built" note: targets resolve on the first tick with wheels (not BeginPlay), only debug lowers dirt, glass calls the function, slot 5 still free, CPD on the runtime copy. ARCHITECTURE.md: via the `/save` that closes this session.
  MIGRATION.md complete:
  - the component, `B1MudThrow`;
  - the tag and the CPD layout;
  - `MF_B1MudDirt` + textures;
  - `User.Intensity`, `SM_B1MudClump`.
  ADR-021 consequences checked against what was built. ARCHITECTURE.md is updated through `/save`.
  **Done:** MIGRATION.md lists every new host requirement; `/save` run.
  **User check:** —

- [x] **S5-19** **Checkpoint 2:** Phase V playtest (user + agent, 1–2 h) _(requires S5-11..S5-18, S5-20, S5-21..S5-28)_ <!-- done: 2026-11-08 -->
  ✅ User sign-off (2026-11-08): Swamp playtest done, happy with spray, dirt and lens. Suites after the lens work: mud 33/33, core 12/12, winch 36/36 (`s527_*.log`).
  Lens (added 2026-11-08): the playtest also covers the lens: flooring it in medium splashes it within ~1 s and fills it in a few seconds of wheelspin; rolling doesn't; a side orbit still gets splashed, a front view doesn't; it clears within ~8 s.
  Agent part (2026-11-08): headless core 12/12, mud 28/28 (22 mud + 5 dirt + `B1Spray_Medium`), winch 36/36 (`s519_*.log`). Waiting on the user's Swamp playtest and sign-off.
  Playtest request (2026-11-08): spray tunables in `Mud|Spray` + the Mud window (typed, Save to asset):
  - `SpawnBackOffset` 20 / `SpawnLift` 20 cm replace the hard-coded `SurfaceLift`; same placement by default.
  - `SpraySpread` 35° → `User.SpraySpread` (Spray cone; Droplets ×1.2). `SpraySpeedSpread` 0.35 → `User.SpeedSpread`: per-particle speed × U(1 − s, 1 + s) on Spray and Droplets (Droplets also × 1.075). The clumps had a fixed 25° cone and no speed variation. Haze unchanged.
  - Builds; `NS_B1MudSpray` compiles with no errors or warnings. MIGRATION.md updated (6 user params).
  **Agent:** all suites headless (core, mud + dirt rigs, winch).
  **User:** Swamp playtest. The spray reads as mud (small mixed clumps, haze) on spin and when rolling; dirt builds tires → arches → spots and stops at about the concept look in ~90 s of mud driving. Tune with the Mud window as needed.
  **Done:** all suites PASS; ROADMAP Phase V ticked.
  **User check:** sign-off on Phase V.

### Backlog (found during the sprint)

- _(empty)_
