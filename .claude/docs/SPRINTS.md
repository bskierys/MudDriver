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

**Status:** refined 2026-10-27 after the Sprint 3 checkpoint; start with `/work`.

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

- [ ] **S4-12** Cable lies on the ground and the mud (3 h) _(requires S4-13)_
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

- [ ] **S4-07** Winch hook carry (4.5 h) _(requires S4-06, S4-12, S4-13)_
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

- [ ] **S4-08** Interaction prompt, cable from hand, debug panels (3 h) _(requires S4-07, S4-12)_
  - Minimal UMG prompt showing the scan's best option ("Enter", "Take hook", "Attach", "Return hook", "Detach") and its key, read from `UB1InteractionScanComponent`, so it can't disagree with what interact does.
  - Cable end follows a hand socket on the first-person camera.
  - Winch window and `ShowDebug B1Winch` show the hook state and carry distance vs `MaxCableLength`.
  - NetImGui "Abilities" panel (Debug → Abilities, user): granted specs with input tags, active abilities, loose tags on the ASC, current avatar and possessed pawn, the scan's options in range.
  **Done:** builds; the panel opens in PIE (capture).
  **User check:** you always know what the interact key will do; the debug views read the carry state and the abilities panel helps while testing.

- [ ] **S4-09** Winchable log (3 h) _(requires S4-01)_
  `BP_B1WinchLog`: simulating trunk mesh with `UB1WinchAnchorComponent`; it gets the opposite cable force. Log lane in `Lvl_Gym_Winch` (not built in S3-14). Pick mass and friction so the 60 kN cap moves it, but not trivially. Rig `B1Winch_Log`: braked jeep, full pull, the log moves ≥ 3 m toward the jeep, `tensionMax` ≤ 63 kN.
  🔍 Research first, in this order:
  1. Apply the opposite force to the log's body handle from `FB1VehicleSimulation::UpdateWinch` in the same physics step: exactly equal and opposite, no cross-thread read.
  2. The log's `AsyncPhysicsTickActor` reading the published tension.
  3. Game-thread `AddForce` on the log.
  Amend ADR-007 ("Log") if 1 wins.
  ⚠️ Note: slip candidate if the sprint runs long (it doesn't block the on-foot loop); Phase 2's done-when then waits for it.
  **Done:** rig PASS headless.
  **User check:** winch the log off the lane; it slides and rolls believably.

- [ ] **S4-10** Phase 2 playtest (user + agent, 1–2 h) _(requires S4-06..S4-09, S4-11..S4-13)_
  Full loop on foot in `Lvl_Gym_Winch`: get stuck, get out, carry the hook, attach, get in, winch out; same on both slopes; lower the jeep down a slope on reel-out; log lane. All suites headless.
  **Done:** all suites PASS; ROADMAP Phase 2 ticked.
  **User check:** sign-off on Phase 2.
