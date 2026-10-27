# HISTORY

Session snapshots, newest at the bottom. Grown by `/save` and `/plan-roadmap`.

---

## 2026-10-25 — Sprint 1 planning

### Context
First planning pass for the VISION.md example scenario, the river crossing from Huta to Bagnisko. The project started as the UE vehicle template plus a NetImGui menu. Design questions were settled in Q&A, the plan went through `/plan-new`, `/plan-analyse` (needs revision) and `/plan-replan` (ready), and is now converted into ROADMAP Phases 0–5 plus Sprint 1. Sprint 1 goes after the two biggest unknowns: the Chaos simulation hook and mud sinking, tested in a gym with agent-runnable test rigs. T0.1 (render budget) was done during planning: the user restarted the editor and was happy with the result (no `stat unit` numbers recorded).

### Tasks created
- S1-01 through S1-24 in SPRINTS.md (S1-01 already done)
- Sprint 2 outlined (GAS + fuel + mud polish), to be detailed at sprint start

### Decisions made
- ADR-001 dynamic per-wheel mud sinking
- ADR-002 mud params in a custom physical material (amended by ADR-012)
- ADR-003 environment forces inside the Chaos simulation (amended: stepping mode decided in S1-12, template constructor change)
- ADR-004 single ASC on PlayerState; the jeep holds a weak ref (user's call, overriding the two-ASC proposal)
- ADR-005 single Condition attribute, no fail state
- ADR-006 obstacles without ASC
- ADR-007 winch as a capped tension force, Uncharted-style
- ADR-008 visual-only mud trails
- ADR-009 gyms before the scenario level
- ADR-010 fuel attribute consumed by engine work
- ADR-011 low-end render budget
- ADR-012 mud collision layering + sinking via suspension trace offset
- Testing model: agent rig tests (`AB1GymTestRig : AFunctionalTest`, PIE via MCP or headless) + a user check per task + a playtest sign-off per phase; feel wins over thresholds
- Target machine: i7-12700H, 32 GB RAM, RTX 3050 Ti Laptop 4 GB. Target 60 fps at 1080p Medium
- Phase 4: the character is hidden on the seat while driving; the engine switches off on exit

### Open follow-ups
- gdep MCP was failing this session (`mcp` is now pinned to 1.30.0). Claude Code restart planned before `/work`.
- No `stat unit` baseline for T0.1. The first numbers come from the S1-24 playtest.

---

## 2026-10-25 — Sprint 1: debug foundation, simulation hook, test rigs (S1-02..S1-11)

### Context
First `/work` session of Sprint 1, on the `dev` branch. Starting point: the vehicle template plus the NetImGui menu, with Sprint 1 planned. The goal was to get through the foundation tasks: debug tooling, the ADR-003 Chaos simulation spike, and agent-runnable test rigs, before any mud work.

### What was built
- S1-02: debug classes renamed `Mud*` → `B1*` (`UB1ImGuiSubsystem`, `UB1CheatManager`, `B1ImGui::DrawVehicleBasics`). No asset redirects needed.
- S1-03: `B1Log.h/.cpp` with `LogB1Vehicle`, `LogB1Physics`, `LogB1Test` (`MUDDRIVER_API`).
- S1-04: per-wheel NetImGui table + `B1Wheels` telemetry line behind CVar `b1.Vehicle.Telemetry`.
- S1-05: `ShowDebug B1Vehicle` text above the vehicle.
- S1-06: template pawn constructors take `FObjectInitializer`.
- S1-07: `UB1VehicleMovementComponent` + `FB1VehicleSimulation` (physics-thread step).
- S1-08: `AB1Jeep` + `/Game/B1/Vehicles/BP_B1Jeep` (template BP duplicated and reparented, 0 CDO diffs); `BP_OffRoadGameMode` now spawns it.
- S1-09: test forces (push / spring-to-anchor) + `B1TestForce` cheat, with an anchor debug line.
- S1-10: `MudDriverTests` module with `AB1GymTestRig`; rig `B1Gym_FullThrottle5s` placed in `Lvl_Offroad`.
- S1-11: `Scripts/RunGymTests.ps1` headless runner.

### Decisions made
- ADR-013: test rigs in a separate `MudDriverTests` DeveloperTool module, so Shipping stays clean. This deviates from the S1-10 note, which said to add FunctionalTesting to `MudDriver.Build.cs`.
- ADR-003 amended: the simulation hook is accepted; game → physics data goes through a lock-guarded buffer, not the Chaos async input.
- Rig threshold: MinDistance 50 m against an 86–88 m baseline. It's loose on purpose, to catch broken drive rather than tuning drift.

### What we learned
- `SetCurrentAsyncInputOutput` is non-virtual, so custom async input structs aren't possible without engine changes.
- A sleeping Chaos vehicle skips `UpdateSimulation`; the test forces zero `SleepThreshold` while active.
- `-nullrhi` (both `-game` and editor automation) still steps Chaos physics.
- Baselines: full throttle for 5 s at 60 fps gives 88.01 m in PIE and 85.71 m headless (vmax 23.8 m/s, peakOsc ~4.2 m/s from terrain hops). Expect about 3 % run-to-run spread.
- Functional test names are `Project.Functional Tests.<map path minus /Game/, dotted>.<actor label>`. External actors in World Partition maps are discovered.
- The editor's automation exit code is 0 regardless of test results; the runner parses `Test Completed. Result={...}`.
- MCP: new WP actors only save via `save_assets([])`; `trace_world` misses the unloaded WP landscape. Added to `.claude/rules/mcp-tools.md`.
- UBT skips new .cpp files unless `Makefile.bin` is deleted (saved to agent memory).

### Open follow-ups
- S1-12 next: stepping decision (async fixed dt vs substepping), using spring rigs at 20 and 120 fps. It needs `MUDDRIVER_API` on `UB1VehicleMovementComponent` so the rig can toggle test forces.
- User checks still open: S1-10 (place your own rig, `bAutoRun`, Play) and S1-11 (run the script).
- Per-wheel sink depth metric in the rig: deferred to S1-18, when `SinkDepth` exists.
- The `bAutoRun` PIE mode logs a harmless LogFunctionalTest warning ("result will not be tracked").

---

## 2026-10-25 — S1-12 physics stepping measurements

Headless (`RunGymTests.ps1 -Filter B1Step`), `Lvl_Offroad`, same spawn spot for all rigs. Configs were applied with `-ini:Engine:[/Script/Engine.PhysicsSettings]:...` overrides.
- **Push:** a 3 kN test push along the jeep (world +Y, uphill), no throttle, 5 s.
- **Spring:** 40 % throttle against the test spring for 10 s (k 20 kN/m, c 9 kN·s/m, 5 m slack, cap 15 kN, mass 1500 kg). `settleTaut` = time from first taut until speed stays below 0.1 m/s.

| Config | Push disp 20 / 120 fps (m) | Push diff | Spring settleTaut 20 / 120 (s) | Spring dist 20 / 120 (m) | Stretch growth | Sim dt |
|---|---|---|---|---|---|---|
| Sync, MaxPhysicsDeltaTime 1/30 (old default) | 3.66 / 7.78 | −53 % | 4.80 / 3.03 | 9.19 / 8.62 | 0 | 0.0333 (clamped) / 0.0083 |
| Async 1/60, MaxPhysicsDeltaTime 1/30 | 3.70 / 7.67 | −52 % | 4.50 / 3.05 | 8.68 / 8.67 | 0 | 0.0167 |
| Sync, MaxPhysicsDeltaTime 0.1 | 7.50 / 7.78 | −3.6 % | 3.15 / 3.03 | 9.12 / 8.62 | 0 | 0.05 / 0.0083 |
| **Async 1/60, MaxPhysicsDeltaTime 0.1 (chosen)** | 7.59 / 7.66 | −0.9 % | 3.00 / 3.04 | 8.67 / 8.65 | 0 | 0.0167 |
| Substep 1/60 ×6, MaxPhysicsDeltaTime 0.1 | 7.71 / 7.78 | −0.9 % | 3.00 / 3.03 | 8.63 / 8.63 | 0 | 0.0167 / 0.0083 |

- The chosen config reproduced its numbers exactly when read from `DefaultEngine.ini` instead of overrides (deterministic fixed step).
- Every config settles at 3.00–3.05 s at 120 fps. That's the scenario's physical settle time, so the spec's 3 s limit is on the edge for this setup.
- Scenario tuning on the way:
  - full throttle out-pulls the 15 kN cap;
  - damping 3 kN·s/m (ζ≈0.26) makes a one-sided-cable limit cycle (1.5 m, never settles);
  - a sideways push can't beat tire grip (0.6 m).

---

## 2026-10-25 — S1-13 B1Mud collision channel

### Context
First Mud gym task. ADR-012 needs a mud surface that only the wheel suspension traces hit, so the chassis can sit "in" the mud. S1-12 had just closed. Its headless re-run with the 3.5 s settle limit was still pending.

### What was built
- `B1Mud` object channel (`ECC_GameTraceChannel1`, default Ignore) and the `B1MudSurface` profile (QueryOnly, blocks only `WorldDynamic`) in `DefaultEngine.ini`. `B1_ObjectChannel_Mud` in `Physics/B1CollisionChannels.h`.
- `UB1VehicleMovementComponent` constructor: `WheelTraceCollisionResponses` blocks `B1Mud`. The `BP_B1Jeep` CDO picks it up.
- Rig metric `endH` (vehicle height above the rig at script end) and the optional `MinEndHeight` check (`bCheckEndHeight` toggle).
- Rigs `B1Mud_WheelsOnPlane` and `B1Mud_ChassisThroughRail` in `Lvl_Offroad`, folder `B1Tests/MudChannel`. They sit on their own BlockAll floors at Z≈5000, away from the terrain.

### Decisions made
- The mud trace response lives on the movement component, not `AB1Jeep` (the sprint task said the jeep). The mud simulation is there, and every B1 vehicle should hit mud.
- Test geometry floats on self-built floors, because the WP landscape can't be traced in the editor and the terrain under the shared rig spawn isn't flat.

### What we learned
- The Chaos suspension trace (and its batched overlap) queries `ECC_WorldDynamic` with `WheelTraceCollisionResponses` (`ChaosWheeledVehicleMovementComponent.cpp:333`). A hit needs both sides to block, so the mud profile blocks only `WorldDynamic`. The new channel's default Ignore keeps every other query and body off it.
- Static mesh components from `/Engine/BasicShapes/Cube` have `bUseDefaultCollision=true`, which overrides any profile with the mesh's BlockAll. Setting `BodyInstance.collisionProfileName` through MCP doesn't clear the flag (`SetCollisionProfileName` does). Set `bUseDefaultCollision=false` first, then the BodyInstance fields. Added to `.claude/rules/mcp-tools.md`.
- With the rail still blocking, the jeep was pushed up at spawn and sat on the rail, wheels spinning in the air: dist=0. So the rail test really does detect chassis contact.
- Results (PIE = headless): WheelsOnPlane endH=-0.30 (a fall-through would be -1.30). ChassisThroughRail dist=10.25 peakOsc=0.077 endH=-0.29.
- Full `Lvl_Offroad` suite headless: 7/7 PASS. FullThrottle 85.00 m. S1-12 spring settleTaut 3.04/3.00 s with the 3.5 s limit (the pending re-run).

### Open follow-ups
- S1-14 `BP_B1MudPatch`: set the profile with `SetCollisionProfileName` or untick default collision (note on the task).
- The `B1MudTest_*` platforms are throwaway S1-13 geometry. Move or retire them once `Lvl_Gym_Mud` (S1-15) has its own mud checks.
- The `b1.Vehicle.Telemetry` lines spam the PIE log for the player's jeep while rigs run. Filter by category when reading.

---

## 2026-10-25 — S1-14 mud patch actor

### Context
Next Mud gym task after S1-13. The channel and profile worked, but the S1-13 test geometry was hand-built cubes with the collision fixed through MCP. The gym needs a reusable patch actor with a surface and a hard bottom, per ADR-012.

### What was built
- `AB1MudPatch` (C++, `Source/MudDriver/Mud/`, include path added). It has a root plus two cube-mesh components:
  - `MudSurface`: 10 cm thick, top at the actor origin, `SetCollisionProfileName(B1_CollisionProfile_MudSurface)`, with `MudMaterial` as its phys material override.
  - `HardBottom`: a 1 m BlockAll slab whose top sits `HardBottomDepth` below the surface. `bHardBottom` off makes it hidden and NoCollision.
  - `OnConstruction` sizes both slabs from `Size`.
- `B1_CollisionProfile_MudSurface` in `B1CollisionChannels.h`.
- `/Game/B1/Mud/M_B1Mud`: a lit material with a `Color` vector parameter (dark brown), roughness 1, specular 0. `/Game/B1/Mud/BP_B1MudPatch` uses it on `MudSurface`.
- `B1MudTest_Patch` (8×8 m) next to the S1-13 platforms, and rig `B1Mud_PatchSurface` (folder `B1Tests/MudChannel`).

### Decisions made
- C++ base plus a thin BP child, so the collision setup lives in code and can't fall into the default-collision trap.
- The hard bottom's depth is a patch setting for now. ADR-012 ties it to `MaxSinkDepth`; revisit once `UB1PhysicalMaterial_Mud` exists (S1-17).
- One base material with a `Color` parameter, so S1-15 can tell patch depths apart with material instances, not new materials.

### What we learned
- `SetCollisionProfileName` in the constructor gives the BP CDO `bUseDefaultCollision=false`, the `B1Mud` object type and QueryOnly. Verified on the CDO and by the rig: endH=-0.31 on the surface, where the hard bottom would give about -0.81.
- `FBodyInstance::PhysMaterialOverride` also applies to complex traces (`GetComplexPhysicalMaterials`), so the override reaches the wheel trace.
- MCP: `BlueprintTools.create(folder_path, asset_name, asset_type)` makes a BP from a C++ class. `CaptureViewport` needs an `annotations` object even when unused. Image results come back as base64, so decode them to a PNG in the scratchpad to view.

### Open follow-ups
- Headless run of `B1Mud_PatchSurface` at the next editor close (PIE only so far).
- S1-15 `Lvl_Gym_Mud` next. The `B1MudTest_*` geometry in `Lvl_Offroad` can retire once the gym has its own checks.

---

## 2026-10-25 — S1-15 mud gym greybox

### Context
The mud patch actor (S1-14) was done. The mud core tasks (S1-17+) need a dedicated gym map with patches of each depth, per ADR-009.

### What was built
- `/Game/B1/Gym/Lvl_Gym_Mud`: a copy of `VehicleBasic` (WP/OFPA) with the landscape, cones, tunnel and walls removed. Sky, fog, lights and the post process volume stay.
- Ground: `WorldGridMaterial` cube tiles with tops at Z=0. Lanes run along +X. The patch zone (X 10–30 m) has gaps in the tiling for the patches:
  - firm lane at Y 0;
  - shallow 15 cm at Y 16, medium 35 cm at Y 29, deep 70 cm at Y 42, each 20×10 m with its own hard bottom.
- Pit at Y −19: 2 m deep, an 8 m ramp in (14°), a 12 m ramp out (9.5°), and a 60 cm mud surface over the floor (`bHardBottom` off; the floor is the bottom).
- `MI_B1Mud_Shallow/Medium/Deep` (tints of `M_B1Mud`). Text-render lane labels.
- `Spawn_1_Firm` .. `Spawn_5_Pit` PlayerStarts at X −12 m.
- `BP_B1GymGameMode` (empty child of `BP_OffRoadGameMode`) as the world override.
- After the user's first drive: a 30 m firm margin on every side (floor X −50..120, Y −60..80 m), plus 3 m perimeter walls (`Gym/Walls`). The walls are hidden in game and use Custom collision with Camera ignored.
- Rig `B1Gym_WallStop` (folder `B1Tests`): full throttle into the north wall, checks `endH ≥ −1.5`.

### Decisions made
- The default spawn is picked by the `Player0` actor tag, which `AOffroadGameMode::ChoosePlayerStart` already matches. No new C++. Other lanes: move the tag or use "Play from here".
- Patches sit in gaps between ground tiles, not on top of the ground, so the hard bottom can be below ground level (ADR-012).
- The patch depths (15/35/70 cm) are placeholder hard-bottom depths until the S1-17 presets set `MaxSinkDepth`.

### What we learned
- `AssetTools.duplicate` on an OFPA map copies its external actors too. The copy has to be saved before `load_level` accepts it.
- The template's spring arms have `bDoCollisionTest=false`, so the camera already ignores level geometry.
- Setting `bodyInstance` (`collisionProfileName: Custom`, `collisionResponses.responseArray`) through `ObjectTools.set_properties`, after `bUseDefaultCollision=false`, does apply at runtime: the rig stopped at the wall.
- Rig `endH` is relative to the rig actor. A rig placed 1 m up reads about −1.0 on the floor.
- `B1Gym_WallStop` PIE: dist=28.30 disp=22.83 vmax=18.71 stuck=4.60 endH=−0.99, PASS.
- `TextRenderComponent.textRenderColor` set through MCP was clamped to white (FColor JSON looks 0–1).

### Open follow-ups
- Mud physical materials on the patches (S1-17).
- Patch exits are vertical 1 m tile edges. Once sinking is in (S1-19), a deeply sunk chassis may catch them; consider ramped exits.
- Headless runs of `B1Mud_PatchSurface` and `B1Gym_WallStop` at the next editor close. (Done in S1-16.)

---

## 2026-10-25 — S1-16 gym smoke test

### Context
The mud gym was built (S1-15) and the user signed off on the layout after the margin and walls were added. The ROADMAP Phase 0 "done when" needs a smoke test in the gym, both in PIE and headless.

### What was built
- Rig `B1Gym_FirmSmoke` in `Lvl_Gym_Mud` (folder `B1Tests`, at X 35 m on the firm lane, past the patches and clear of the player spawn). It runs 6 s at 50 % throttle, with MinDistance 20 m and endH ≥ −1.5.

### What we learned
- PIE: dist=42.82. Headless: dist=42.97. The headless gym ran at avgDt 0.0216 (about 46 fps, not 60), and the async fixed step still kept the distance within 0.4 %.
- Gym headless: 2/2 PASS (`B1Gym_WallStop` stuck=4.21 endH=−1.01).
- `Lvl_Offroad` headless: 8/8 PASS. FullThrottle 84.97 m, PatchSurface endH=−0.30 (the pending S1-14 headless run), springs settleTaut 3.04/3.00.

### Open follow-ups
- S1-17 next: `UB1PhysicalMaterial_Mud` + presets. It needs a full build with the editor closed.

---

## 2026-10-25 — S1-17 mud physical material

### Context
The gym and its smoke test were done. Mud sinking (S1-18+) needs per-surface parameters that the wheel contact can report (ADR-002).

### What was built
- `UB1PhysicalMaterial_Mud : UPhysicalMaterial` (`Source/MudDriver/Mud/`): `MaxSinkDepth`, `SinkRate`, `RiseRate`, `FrictionVsDepth`, `RollingResistanceVsDepth`, `BellyDragCoefficient`. The constructor sets base `Friction` 0.5 and default curves.
- `PM_B1Mud_Shallow/Medium/Deep` (values in SPRINTS S1-17), set as `MudMaterial` on `Mud_Shallow/Medium/Deep/Pit` in `Lvl_Gym_Mud` (the pit uses Deep).

### Decisions made
- Parameter units: depth in cm, `SinkRate` in cm/s at full wheelspin, `RiseRate` in cm per m rolled with grip (rising is tied to distance, so a parked wheel doesn't rise), rolling resistance as a fraction of wheel load, belly drag as a fraction of weight per m past clearance.
- Base `Friction` is the zero-depth grip; the depth curve multiplies on top. ADR-002 is now accepted, with an implementation note.
- The patches' `HardBottomDepth` stays manual (it matches the presets' `MaxSinkDepth`: 15/35/70). Tying it to the material automatically isn't needed yet.

### What we learned
- `ApplyWheelFrictionForces` calls `PWheel.SetSurfaceFriction(HitResult.PhysMaterial->Friction)` on the physics thread every step (`ChaosWheeledVehicleMovementComponent.cpp:519`).
- MCP can't create physical material assets (`DataAssetTools.create` only takes `UDataAsset`). The user made one by hand and the agent duplicated it. `StartPIE`'s `startTransform` is ignored when the game mode picks a PlayerStart. Both are in `.claude/rules/mcp-tools.md`.
- The verification used temporary rigs (since removed): jeeps dropped on each patch report `PM_B1Mud_*` on all four wheels.

### Open follow-ups
- S1-18: per-wheel `SinkDepth` in `FB1VehicleSimulation`.

---

## 2026-10-25 — S1-18 per-wheel sink depth

### Context
The mud presets were on the gym patches (S1-17). ADR-001 needs a per-wheel depth state on the physics thread, driven by wheelspin and rolling.

### What was built
- `FB1VehicleSimulation::UpdateSinkDepth`, plus an output channel (`FB1VehicleSimOutput`, `OutputLock`, `GetGameThreadOutput`).
- `FB1MudSimParams` in the sim input. `UB1VehicleMovementComponent::GatherMudParams` keeps a `KnownMudMaterials` cache and exposes `GetWheelSinkDepth`.
- `UB1PhysicalMaterial_Mud::FullSinkSlipSpeed` (5 m/s).
- `sink=` in the `B1Wheels` telemetry and in the rig result line.
- Rig `B1Mud_SinkMedium` in `Lvl_Gym_Mud`.

### Decisions made
- The game thread doesn't send a per-wheel material table. It sends params for every mud material the vehicle has touched, and the physics thread matches its own trace hit by `TWeakObjectPtr ==` (no dereference). Cost: about one frame of firm on first contact with a new material.
- The sink signal is the slip speed in m/s (`GetSlipMagnitude`), not Chaos's normalized slip. It's continuous and has physical units, and `FullSinkSlipSpeed` makes it tunable.
- Rise is tied to distance rolled. A parked wheel neither sinks nor rises.
- Airborne wheels keep their depth; non-mud contact resets it.

### What we learned
- Under full throttle the jeep's weight shifts back: the rears grip (slip 0, suspension 0.37) and the fronts spin at about 3 m/s. Only the fronts sink (7.4/8.8 cm in 2 s on medium).
- Headless: gym 3/3 PASS (FirmSmoke 42.80 m, unchanged); `Lvl_Offroad` 8/8 PASS, no regressions.

### Open follow-ups
- S1-19: offset the suspension hit by `SinkDepth` (`PerformSuspensionTraces`), with a rate limit and an entry ramp.

---

## 2026-10-25 — S1-19 sinking via suspension trace offset + weight sinking

### Context
Depth was tracked (S1-18) but didn't move the jeep. The first version (hit offset by `SinkDepth`, slip-only sinking) worked in the rigs. The user's playtest then found that only the front wheels sank, nothing sank without throttle, and the effect was too weak outside deep mud.

### What was built
- `FB1VehicleSimulation::PerformSuspensionTraces`: traces from starts raised by `SinkOffset`, then pushes `Location`/`ImpactPoint` down, clamped to full droop. `UpdateSinkOffset` has a rise-only limit of 30 cm/s.
- Weight sinking: `StaticSinkRatio` (0.45) and `SettleRate` (25 cm/s) on `UB1PhysicalMaterial_Mud`. A per-wheel floor scaled by relative load; the dig is also scaled by load.
- Presets retuned: SinkRate 15/30/45, RiseRate 4/2/1, FullSinkSlipSpeed 3 m/s.
- Rigs in `Lvl_Gym_Mud`: `B1Mud_CrossMedium`, `B1Mud_ExitMediumFull`, `B1Mud_SinkDeepStop`.
- `ChaosVehiclesCore` added as a private dependency of `MudDriver`.

### Decisions made
- ADR-001 amended: weight settles every wheel to a static depth, wheelspin digs deeper. ADR-012 implementation note: raised trace start; a rise-only limit instead of the entry ramp.
- Load is relative to the average wheel load, not `RestingForce`.
- The rig peakOsc limit for patch transitions is 0.5 m/s (firm driving is about 0.17).

### What we learned
- In 5.8 the suspension reads `HitResult.Distance` (length) and `Location`/`ImpactPoint` (constraint target, friction point); all three must move together.
- A sunk chassis drags the sweep start into the 10 cm slab. Raising the start by the offset fixes it, and Chaos's `Distance` arithmetic does the rest.
- Chaos's reported wheel load at rest is about 0.55–0.65× `RestingForce` (sprung mass × g); the constraint suspension carries the rest.
- Open-diff behaviour: under full throttle the front axle unloads and one wheel spins at 16–19 m/s. With slip-only sinking that free wheel dug to the max.
- Template: brake at a standstill engages reverse (matters for rig scripts).
- MCP: `CaptureViewport` with a transform renders the editor world, so use `CaptureEditorImage` during Simulate; array properties can't be resized and edited in one `set_properties` call. Both are in `.claude/rules/mcp-tools.md`.
- Headless after the rework (sink F/F/R/R):
  - CrossMedium 16/16/20/20, peakOsc 0.361.
  - SinkMedium 35/35/24/24, endH −1.28.
  - ExitMediumFull 35/35/23/23, peakOsc 0.410 (0.608 with a 50 cm/s rise limit).
  - SinkDeepStop 70×4, endH −1.63.
  - Gym 6/6 and `Lvl_Offroad` 8/8 PASS.
- User check: "The mud sinking feels better now."

### Open follow-ups
- S1-20: traction and rolling resistance vs depth. In deep mud the jeep currently rests on the hard bottom but can still drive out.
- The visual capture used Simulate + `CaptureEditorImage`; there's no saved artifact in the repo.

---

## 2026-10-26 — S1-20 traction, rolling resistance and speed drag + S/M/L/XL mud

### Context
Sinking worked (S1-19), but depth didn't affect grip or resistance yet. The first pass aimed at the sprint criterion "full throttle on medium digs in within 2–4 s". It passed the rigs, but the user's playtest found three problems: driving slowly into medium got stuck for good, deep at full speed barely slowed or sank, and after leaving deep the jeep drove on sunk into the firm tiles. After the rework, the user asked for a deeper shallow preset and a new size between medium and deep.

### What was built
- `FB1VehicleSimulation::ApplyWheelFrictionForces` override:
  - scales each mud wheel's `FrictionMultiplier` by `FrictionVsDepth` around the base call;
  - adds rolling resistance plus speed drag at the contact point, static-friction style at a standstill, capped so it never reverses the wheel.
- `UB1PhysicalMaterial_Mud::SpeedDragCoefficient`: × normalized depth × v².
- Curves are copied to the physics thread as `FRichCurve`; `StepInput` is read at the start of the step; relative loads are shared with sinking.
- The `SinkOffset` rise limit is distance-based: max(30 cm/s, 0.5 × wheel speed).
- Telemetry shows `mu=` and `rr=` per wheel; the component exposes `GetWheelMudFrictionScale` and `GetWheelRollingResistance`.
- Presets retuned. New `PM_B1Mud_Heavy` (50 cm) with tint `MI_B1Mud_Heavy`; Shallow 15→25 cm.
- Gym: Heavy lane at Y 42; the Deep lane moved to Y 55 with ground tiles re-cut around it; labels and spawns renumbered.
- Seven new rigs (listed in ARCHITECTURE Testing).

### Decisions made
- ADR-001 amended: medium is a slog, not a trap, and full stops come from belly drag in deep (S1-21). The S1-20 and S1-23 "stuck within 2–4 s" criterion became "bogs down" (the user's feel wins).
- Speed drag is quadratic so slow, careful driving is barely affected. Full throttle in medium (~16 km/h, wheels spinning) is slower than 50 % throttle (~23 km/h).
- The new size is named Heavy (L), so existing assets didn't need renaming.

### What we learned
- The jeep's wheel `FrictionForceMultiplier` is 4. Effective grip is about 0.83 × the friction-curve value × weight share.
- From a standstill, 40 % throttle gives only ~290 Nm per wheel (~570 N), because the engine sits near idle. Static rolling resistance above that traps the jeep.
- If resistance can exceed grip at full depth, the jeep is trapped for good: rising needs rolling, and rolling needs grip > resistance.
- With resistance that only opposes motion, a spinning stuck wheel creeps at ~15 cm/s. Opposing the tire push at a standstill fixes it.
- Chaos rebuilds `SurfaceFriction` from the phys material inside `ApplyWheelFrictionForces`; the per-wheel `FrictionMultiplier` is the only hook.
- `RigidHandle->M()` needs the Chaos module to link; `PWheel.MassPerWheel` (set by Chaos) avoids that.
- Headless: gym 13/13, `Lvl_Offroad` 8/8 PASS. DeepRunUp enters at 66 km/h and stops at 40.1 m inside the patch. ExitDeep ends at endH −1.01.

### Open follow-ups
- User playtest of the reworked mud and the four sizes. Pending: medium slog feel, Heavy restart (6.3 m in 6 s at 40 %), and climb-out bumps (ExitDeep peakOsc 0.92).
- S1-21 belly drag should make full-depth deep a real stop.
- The material constructor defaults (curves 1→0.6, RR 0.05→0.3) no longer match the tuned ranges; they only affect new assets.

---

## 2026-10-26 — S1-21 belly drag

### Context
The user asked the agent to run the rest of Sprint 1 without stopping for user checks; those are batched at the end of the sprint. Deep mud already stopped the jeep through rolling resistance and speed drag (S1-20), but the analytic belly drag from ADR-001/012 wasn't there yet.

### What was built
- `FB1VehicleSimulation::ApplyBellyDrag`: excess = average `SinkDepth` (all wheels) − `BellyClearance`; drag = mean `BellyDragCoefficient` of the mud wheels × excess (m) × vehicle weight, at the centre of mass against ground velocity. At standstill it opposes the tires' summed push; capped at push + 0.4 × stopping force.
- `UB1VehicleMovementComponent::BellyClearance` (45 cm, `Mud|Config`), `GetBellyDepth`, `GetBellyDrag`, and a one-time `LogB1Physics` line with the measured body clearance.
- Telemetry `belly=<cm>/<N>`. Rig line: `vend=` and `belly=`; new threshold `MaxEndSpeed`.
- Rig `B1Mud_DeepFullThrottle` in `Lvl_Gym_Mud`.

### Decisions made
- `BellyClearance` is axle height (45 cm), not the physics body's underside (60–64 cm measured). With 62 cm only the last 8 cm of deep would drag. With 45, Heavy dug to full depth gets a little drag, and Medium and weight-only sinking (static floor ≤ 31.5 cm) get none.
- The force goes at the centre of mass, not the belly, so it doesn't pitch the jeep.

### What we learned
- Full throttle from rest in deep already stops in under a metre; belly drag (≈7.3 kN, about half the weight) holds it there. DeepFullThrottle: dist 0.60, stuck 0.85 s, vend 0.00. DeepRunUp is unchanged at 39.97 m, belly 7355 N.
- The unreal-mcp tools can fail to register at session start even when the editor comes up later. The server still answers plain JSON-RPC on `127.0.0.1:8001/mcp` (initialize → `Mcp-Session-Id` header → `tools/call` `call_tool`), which is enough to place and save rigs.
- Headless: gym 14/14, `Lvl_Offroad` 8/8 PASS.

### Open follow-ups
- No A/B run with belly drag off; deep was already a stop without it. The user's end-of-sprint playtest decides whether it needs to be stronger or start earlier.
- Preset `BellyDragCoefficient` values are all the class default (2.0); tuning per size is for S1-24.

---

## 2026-10-26 — S1-22 NetImGui Mud window + mud telemetry

### Context
Same autonomous sprint run. Mud tuning needed a live editor before the S1-24 playtest.

### What was built
- `Debug/B1ImGuiMud` (Debug → Mud): belly readout, live `BellyClearance`, a per-wheel mud table with sink bars, and live editing of every loaded mud preset (scalars plus curve keys) with "Save to asset" in editor builds.
- `B1VehicleTelemetry::LogMud` behind CVar `b1.Mud.Telemetry`: a 2 Hz `B1Mud` line on `LogB1Physics`, only while touching mud. The mud fields left the `B1Wheels` line.

### Decisions made
- Presets are edited on the asset objects directly, not on a copy. The component already copies params to the physics thread every frame, and PIE uses the same asset objects as the editor, so "save" just writes the package.
- Save uses `UPackage::SavePackage` under `WITH_EDITOR`, so the runtime module doesn't need an `UnrealEd` dependency.

### What we learned
- The NetImGui plugin ships ImGui 1.90 (`BeginDisabled` and tables are available).

### Open follow-ups
- User check: the window's layout, live `SinkRate` edits while driving, and Save persisting across an editor restart.

---

## 2026-10-26 — S1-23 mud rig suite

### Context
Same autonomous sprint run. The suite items mostly had rigs already; they needed thresholds that check the sprint's wording, plus a creep check.

### What was built
- Rig: `lastDisp` metric + `MaxLastStepDisplacement` threshold.
- New rigs `B1Mud_CreepDeep` and `B1Mud_CreepMedium`. Thresholds tightened on `DigInMedium` (5–15 m), `CrossMedium` (10 s, ≥ 32 m, peakOsc ≤ 0.65), `CrawlIntoMedium` (peakOsc ≤ 0.5) and `DeepRunUp` (peakOsc ≤ 0.8).

### Numbers (headless, 1/60 s fixed step)
| Rig | Result |
|---|---|
| DigInMedium | dist 13.57, vmax 4.41 |
| CrossMedium | dist 60.47, peakOsc 0.573 |
| CrawlIntoMedium | dist 25.69, peakOsc 0.387 |
| RestartMedium / RestartHeavy | dist 10.83 / 6.28 |
| RecoverMedium | dist 19.17 |
| DeepFullThrottle | dist 0.65, stuck 0.85 s, vend 0.02, belly 7259 N |
| DeepRunUp | dist 39.99 (≤ 42), peakOsc 0.598 |
| CreepDeep / CreepMedium | lastDisp 0.008 / 0.002 m |
| ExitMediumFull / ExitDeep | peakOsc 0.379 / 0.916 |

PIE (editor ~34 fps, async 1/60 physics) lands within a few percent: CrossMedium 60.44, DigInMedium 13.85, DeepRunUp 39.97.

### What we learned
- PIE with every rig on `bAutoRun` breaks: rigs on the same lane spawn jeeps on top of each other. Run them in rounds, one rig per lane.
- Toggling `bAutoRun` dirties the rig packages; `StopEditor.ps1` then hangs on the save dialog. Save after resetting it to false.
- Holding still in mud (creep) is solid: deep 8 mm in 10 s, medium 2 mm.

### Open follow-ups
- Exit bumps (CrossMedium 0.57, ExitDeep 0.92) for the S1-24 playtest.

---

## 2026-10-26 — S1-25 type-in values in the Mud window

### Context
User review of S1-21..S1-23 came back with four notes: mud patch edge ramps, typed ImGui values, engine RPM in Vehicle Basics, and a better chase camera. They became S1-25..S1-28, ahead of the S1-24 playtest.

### What was built
- `DragFloatTyped` in `B1ImGuiMud.cpp`: drag, then a 70 px `InputFloat` field, then the label. Typed values are clamped to the drag's range. Used for every preset scalar and belly clearance. Curve keys get an `InputFloat2` next to the `DragFloat2`.
- `Static sink ratio` changed from a slider to the same drag + field.

### Open follow-ups
- User check: typing a value applies it like a drag.

---

## 2026-10-26 — S1-26 engine RPM in Vehicle Basics

### What was built
- Vehicle Basics: `Speed / Gear / Throttle %` line and an RPM progress bar (`current / max rpm`), so wheelspin in mud shows as revs without speed.

### Open follow-ups
- User check: floor it in medium; RPM climbs while speed stays low.

---

## 2026-10-26 — S1-27 chase camera

### What was built
- `AMudDriverPawn`: `CameraYawRate` 1.5, `CameraRecenterDelay` 2 s, `CameraRecenterSpeed` 0.5 (category `Camera`). Recentering waits for the delay after the last look input; yaw is normalized to ±180 so it swings back the short way.
- Back spring arm 850 cm (was 650), socket Z 180 (was 150). `BP_B1Jeep` has no overrides.

### What we learned
- The template's 30–45° limit wasn't a clamp: `FInterpTo(Yaw, 0, dt, 1)` every frame balanced the look input at a small angle.

### Open follow-ups
- User check: orbit, let go, camera holds then drifts back. Values are on the pawn for tuning.

---

## 2026-10-26 — S1-28 mud patch edge ramps

### What was built
- `AB1MudPatch`: four `EdgeRamps` boxes, per-edge `bRamp*` flags (default off) and `EdgeRampLength` (300 cm). The ramp box is pitched by `atan(HardBottomDepth / Length)` so its top face runs from the bottom up to the surface at the edge.
- `FB1VehicleSimulation`: `TraceHardGround` (line trace with `B1Mud` ignored) per mud wheel; `HardGroundDepth` caps `SinkOffset` and `SinkDepth`. Works for landscape bottoms too.
- Gym: ramps on all four edges of Shallow/Medium/Heavy/Deep. Traced profile on Deep: −23 cm at 1 m in, −70 cm from 3 m, −35 cm 1.5 m before the exit edge. Nine in-mud rigs moved +2 m in X.

### Numbers (headless)
| Rig | Before | After |
|---|---|---|
| ExitDeep peakOsc | 0.916 | 0.541 |
| CrossMedium peakOsc | 0.573 | 0.367 |
| CrawlIntoMedium peakOsc | 0.387 | 0.243 |
| ExitMediumFull peakOsc | 0.379 | 0.502 (limit → 0.55) |
| DeepRunUp | dist 40.0, stuck at the exit wall | dist 56.9, exits at ~20 km/h, peakOsc 1.03 |

Everything else within 1 %. Gym 16/16, `Lvl_Offroad` 8/8.

### What we learned
- DeepRunUp's "stuck" was the vertical exit wall of the hard bottom, not mud drag. Deep slows a 66 km/h run-up to ~20 km/h in ~1.5 s, which carries it across a 20 m patch.
- `StopEditor.ps1` once timed out right after `save_assets([])` with nothing left dirty in git; a second save + stop worked.

### Open follow-ups
- S1-24: should a run-up carry through 20 m of deep? If not, raise Deep's `SpeedDragCoefficient` / rolling resistance and restore DeepRunUp's distance limit.
- User check: climb-out over ramps feels smoother.

---

## 2026-10-26 — Sprint 1 sign-off

### Context
User reviewed S1-25..S1-28. Mud physics: satisfied, S1-24 closed without preset changes. Camera: the S1-27 recenter felt like no pull at all. Also asked for a fresh gdep snapshot and a gdep-first search rule.

### What was built
- Camera recenter retuned: delay 2 → 1 s, speed 0.5 → 0.75 (template 1.0, no delay). Only non-zero look input resets the delay. PIE check: 120° → 70° (0.6 s) → 44° (1.2 s) → 7° (3.7 s).
- gdep regenerated (`gdep init <root> --force`): 30 files / 29 classes (was 18 / 18, from before the B1 classes).
- `/save` got a step 5: regenerate gdep when classes change. CLAUDE.md "Code structure and search" and AGENTS.md now say to search dependencies with the gdep MCP tools before Grep/Read.
- Sprint 1 closed; ROADMAP Phase 0 done, Phase 1 core items ticked.

### Decisions made
- The deep run-up carrying through a 20 m patch (S1-28) stays: the user is satisfied with the mud physics as-is.

### What we learned
- `IA_LookAround` has no triggers and maps only `MouseX`, so it fires only while the mouse moves; the weak pull was the tuning, not stray zero input.

### Open follow-ups
- User check: the retuned recenter.
- Sprint 2 (GAS + fuel + mud polish) needs detailing at sprint start (`/sprint`).

---

## 2026-10-26 — Sprint 2 planning + GAS foundation (S2-01, S2-02)

### Context
Sprint 1 closed. Sprint 2 was a one-line placeholder in SPRINTS.md; detailed it, then started the GAS bootstrap. No GAS code existed (plugin enabled, modules unlinked).

### What was built
- Sprint 2 in SPRINTS.md: 16 tasks (~30 h) in four groups: GAS bootstrap (S2-01..06), fuel (S2-07..09), mud polish (S2-10..14), close playtests (S2-15..16).
- S2-01: `GameplayAbilities`/`GameplayTags`/`GameplayTasks` linked publicly; native tags in `GAS/B1GameplayTags` (`State.Avatar.InVehicle/OnFoot`, `Vehicle.State.InMud/OutOfFuel`, `Data.Fuel`, `Data.Damage`).
- S2-02: `AB1PlayerState` (ASC + `UB1VehicleAttributeSet` as default subobjects, Mixed mode), empty `UB1AbilitySystemComponent`, attribute set Condition/MaxCondition 100, Fuel/MaxFuel 60 L, meta attribute `Damage` → Condition in `PostGameplayEffectExecute`, clamped in `PreAttributeChange`.

### Decisions made
- Rigs that spawn their own jeep have no ASC; fuel/tag rigs get a `bPossessWithPlayer` option (S2-09).
- `AB1PlayerController` will subclass the template controller and only fix its vehicle-only `OnPossess` (S2-03).
- GEs as BP assets via MCP; native GE classes as fallback (S2-05).
- Damage via meta attribute targeted by a SetByCaller modifier, so `Damage` is not `HideFromModifiers` (Lyra hides it because it uses an execution calc).
- No attribute replication: single-player prototype.

### What we learned
- The ASC registers attribute sets outered to its owner in `InitializeComponent` (`AbilitySystemComponent_Abilities.cpp:96`); no `AddAttributeSetSubobject` needed for default subobjects.

### Open follow-ups
- S2-01's tag-picker check moved to S2-05 (first GE BP).
- Nothing uses `AB1PlayerState` until S2-03 sets it on `AB1GameMode`.

---

## 2026-10-26 — GAS wiring, effects and status window (S2-03..S2-06)

### Context
S2-01/02 had the tags, player state and attribute set, but no game mode used them. This session wired the ASC into play, added the first gameplay effects and a debug view of condition and fuel.

### What was built
- S2-03: `AB1GameMode` (player state class) and `AB1PlayerController` (`InitAbilityActorInfo` + `State.Avatar.InVehicle` loose tag). BPs `BP_B1GameMode` / `BP_B1PlayerController` in `/Game/B1/Player/`; `BP_B1GymGameMode` reparented, `Lvl_Offroad` uses `BP_B1GameMode`. The template controller now accepts any pawn and hides the vehicle UI on foot.
- S2-04: `AB1Jeep` implements `IAbilitySystemInterface` via a weak pointer to the player state's ASC, set in `PossessedBy`.
- S2-05: GEs `GE_B1Damage_Impact`, `GE_B1FuelDrain`, `GE_B1FuelAdd` (instant, SetByCaller); `UB1GASSettings` developer settings; ASC helpers; cheats `B1DamageVehicle`, `B1SetCondition`, `B1AddFuel`.
- S2-06: Debug → Vehicle Status ImGui window and the `B1Status` telemetry line (`b1.Vehicle.StatusTelemetry`).
- Gym 16/16 and `Lvl_Offroad` 8/8 after every task, numbers unchanged.

### Decisions made
- GE classes are looked up through a `UDeveloperSettings` (soft class refs in `DefaultGame.ini`), not hard-coded paths.
- The jeep keeps its ASC pointer after unpossess, so its status stays readable when parked.
- Fuel add and drain are separate GEs; the ASC helper picks by sign.

### What we learned
- MCP can author GE BPs: `BlueprintTools.create` with `asset_type=/Script/GameplayAbilities.GameplayEffect`, then `set_properties` on the CDO `Modifiers` with a SetByCaller magnitude JSON.
- The template controller respawned `BP_OffroadCar_Pawn`; the B1 controller's `VehiclePawnClass` had to be set to `BP_B1Jeep`.
- A local `bHidden` in the template controller shadowed `AActor::bHidden` (C4458) and failed the build.

### Open follow-ups
- Pre-existing `Lvl_Offroad` load error (missing dependent package on one external actor), logged in the backlog.
- User check: S1-27 recenter retune.

---

## 2026-10-26 — Fuel: consumption, out of fuel, rigs, mud strain (S2-07..09, S2-17)

### Context
S2-03..06 committed (`0d9b191`). This session built the fuel loop from ADR-010, then reworked it after the user's playtest.

### What was built
- S2-07: `UB1VehicleTuningData` + `DA_B1JeepTuning`; the jeep accumulates consumption and applies a drain GE every 0.25 s; engine power rebuilt from throttle² × torque curve(RPM) × RPM. Vehicle Status shows L/h, L/100 m, power; `B1Status` gets `lph= l100= pow=`.
- S2-08: Fuel attribute delegate → `Vehicle.State.OutOfFuel` + throttle cut-off in the movement component.
- S2-09: rig `bPossessWithPlayer` / `StartFuel`, fuel metrics and thresholds, rigs `B1Fuel_Firm/Medium/Empty`.
- S2-17 (from playtest): mud strain term `MudWorkRate × throttle × sink/MudFullStrainDepth`; `fuelPerHour=` metric; rigs `B1Fuel_FirmFull`, `B1Fuel_DeepFull`. `DragFloatTyped` moved to `Debug/B1ImGuiWidgets`, save code to `Debug/B1ImGuiAssetSave`; rule `.claude/rules/imgui.md` (listed in CLAUDE.md).
- Gym 21/21, `Lvl_Offroad` 8/8.

### Decisions made
- ADR-010 amended: mud strain term, because Chaos engine power ignores load.
- Fuel cut-off zeroes `ThrottleInput` before `Super::Update`; no idle creep exists since engine torque is throttle-scaled.
- Every tunable ImGui float pairs a drag with a typed field (user asked twice).

### What we learned
- Chaos engine torque = throttle² × curve(RPM); at 50 % throttle the jeep doesn't slip in medium mud, so power-only consumption made mud cheaper per hour than fast firm driving (1.86× per 100 m only from being slower).
- A client reconnecting to the MCP port during a headless run logs `Call to unknown method "server/discover"` as an error and fails whichever test is running.
- `StopEditor.ps1` stalls on a save dialog if the user's playtest left dirty packages; ask before saving or discarding.

### Open follow-ups
- S2-15 fuel playtest: tune the placeholder rates (6 / 240 / 300 L/h, 50 cm).

---

## 2026-10-26 — Sprint 3–4 planning (mud + winch first)

### Context
The user reordered the phases after Phase 1/G to prove mud + winch first: On foot and winch (was Phase 4) is now Phase 2, River stays 3, Damage and vegetation (was 2) is now 4, Scenario stays 5. VISION.md gained a "Muddy slope" situation: a slope the jeep climbs on firm ground but not on mud, solved with the winch. The plan went through `/plan-new`, `/plan-analyse` (needs revision: slope standstill bug, winch vs drag caps, missing ability plumbing, two sprints), `/plan-replan` (ready) and is archived as [plans/2026-10-26-mud-winch-first.md](plans/2026-10-26-mud-winch-first.md). Sprint 2 was not changed. Sprint 3 ends with a mud + winch checkpoint (hook attached by cheat, pull from the car) before any on-foot work in Sprint 4.

### Tasks created
- S3-01 through S3-16 in SPRINTS.md (slopes, winch core, debug, rigs, checkpoint)
- S4-01 through S4-09 in SPRINTS.md (outlined; ability plumbing, character, enter/exit, hook carry, log, phase playtest)

### Decisions made
- Phase reorder and renumbering; ROADMAP keeps "was Phase N" notes. Machete moved to Phase 4, the winched ford crossing to Phase 3, the physics log to Phase 2; Phase 5 gets a muddy uphill.
- ADR-001 amended (user decision): on a muddy slope a stopped jeep slides down slowly; the mud's hold depends on absolute sink depth, not the preset (barely sunk slides, deeply sunk holds but can't climb, Deep slides until sunk in). `SlopeHoldVsSinkDepth` on the movement component; standstill rules use the net force along the ground. Flat ground unchanged.
- ADR-007 amended: winch is its own force path (test force stays), applied at the winch point, counted as push in the drag caps, native analog pull input; log force via async physics tick.
- ADR-014 (proposed): ability sets granted on the PlayerState ASC, input tags for abilities, native input for continuous controls.
### What was built
- VISION.md: new "Muddy slope" situation under the on-foot section.
- ROADMAP.md reordered and renumbered; Phase 2 (On foot and winch) fully scoped.
- SPRINTS.md: Sprint 3 (S3-01..S3-16) detailed, Sprint 4 (S4-01..S4-09) outlined. Sprint 2 untouched.
- DECISIONS.md: ADR-001 and ADR-007 amendments, ADR-014 (proposed).
- No code, assets or ARCHITECTURE.md changes.

### What we learned
- The mud standstill rules (`B1VehicleSimulation.cpp:229-235` rolling resistance, `:293-296` belly drag) take the tire force as "the push". On a slope the tire force is mostly holding against gravity, so the current rules would push a resting jeep downhill and brake a slide hard. Fixed by design in S3-03 (net force along the ground).
- Same caps would block a winch pull on a belly-stuck jeep unless the winch counts as push (S3-10).
- `AB1MudPatch` already builds with the actor rotation (`B1MudPatch.cpp:88`); only the vertical hard-ground cap trace needs care on slopes.
- Nothing in the code grants abilities yet (no `GiveAbility`); Sprint 4 starts with that plumbing (ADR-014).
- `CableComponent` isn't enabled in `MudDriver.uproject`.
- Resting sink depths that drive the slope hold curve: Medium ~16 cm, Heavy ~22 cm, Deep ~31 cm (`MaxSinkDepth × StaticSinkRatio 0.45`).

### Open follow-ups
- Sprint 2 still open (S2-10..S2-16); Sprint 3 starts after it closes.
- Sprint 4 needs a `/sprint` refinement after the S3-16 checkpoint.
- Slope angle, hold curve and anchor distances are picked in S3-05; if no angle in 15–30° separates firm from medium, ask the user about a per-material slope grip factor.

---

## 2026-10-26 — Mud state, unstuck, ShowDebug B1Mud (S2-10..S2-12)

### Context
Sprint 2 `/work` from S2-10 up to the S2-12 user check. GAS and fuel were done; the mud polish tasks needed the ASC on the jeep (S2-04).

### What was built
- S2-10: `Vehicle.State.InMud` loose tag from `AB1Jeep::UpdateInMud` (any wheel on mud, 0.5 s release, cleared on unpossess). Rig metric `inMud=`, threshold `MinInMudTime` (set to 2 s on `B1Fuel_Medium`), error log if the tag survives unpossess.
- S2-11: `B1Unstuck` cheat → `UB1VehicleMovementComponent::Unstuck()`: teleports level with wheel bottoms 50 cm above the `B1Mud` surface, zeroes velocity, one-shot sink reset through `FB1VehicleSimInput::SinkResetSerial`. After the user's playtest (the jeep sank right back): a 4 s assist window, 15 kN × (raw throttle − raw brake) along the heading, applied in the sim. Rig step flag `bUnstuck`, new rig `B1Mud_UnstuckDeep`.
- S2-12: `ShowDebug B1Mud` (`Debug/B1MudShowDebug`): per-wheel mud state, belly and InMud over the jeep (`AMudDriverPawn::DrawShowDebug` is now virtual); preset, depth, bottom and ramps over each `AB1MudPatch` within 50 m (each patch binds `AHUD::OnShowDebugInfo`).
- Gym 22/22, `Lvl_Offroad` 8/8. The user checked S2-11 and S2-12 in the editor and signed them off.

### Decisions made
- One-shot game→physics commands use a counter in `FB1VehicleSimInput`, not a bool: input is latest-wins, so a one-frame bool can be overwritten before a physics step reads it. The sim echoes the applied serial in its output.
- The unstuck assist reads `RawThrottleInput`/`RawBrakeInput`, so it ignores the fuel cut-off (it's a cheat) and brake works as reverse.

### What we learned
- A plain teleport out of deep mud isn't enough: the jeep re-sinks from its weight within seconds and can't get going at 50 % throttle (3.4 m in 3 s). With the assist it does 14.2 m.
- `AssetTools.save_assets` needs `asset_paths` (not `assets`); `save_actor` failed on an existing gym rig actor ("Asset does not exist"), but `save_assets([])` saved it.
- An agent `-game` run can confirm a ShowDebug category draws without errors, but the start camera faces the sky, so readability stays a user check.

### Open follow-ups
- S2-13 Niagara spray, S2-14 trails, S2-15 fuel tuning, S2-16 Phase 1 close.

---

## 2026-10-26 — Niagara mud spray (S2-13), first trails pass (S2-14)

### Context
`/work` through S2-13 and S2-14. The user signed off the spray. The trails worked but looked flat, so the user asked for ruts that look dug in (still visual only) as a new task, S2-14b.

### What was built
- S2-13: `UB1MudSprayComponent` on the jeep, `/Game/B1/FX/NS_B1MudSpray` + `M_B1MudSpray`, `SprayColor` per mud preset, `spray=` in the `B1Mud` telemetry, CVar `b1.Mud.Spray`. Committed.
- S2-14 (first pass, not committed yet): per-patch 512² trail render targets, `UB1MudTrailSubsystem` + `UB1MudTrailComponent`, `M_B1Mud` darkens and glosses along the trail. Two clear tracks after a rig drive, but flat.

### Decisions made
- ADR-008 amended: visual ruts get real depth (depth-encoded trail target, dense displaced surface mesh, lip, normals from the gradient). Parallax-only, Nanite tessellation and physics ruts were rejected.

### What we learned
- The MCP Niagara toolset can build a system end to end (template system/emitter, modules, user params, linked and HLSL stack inputs, renderer data, stack-issue fixes). MaterialTools builds material graphs node by node.
- The Collision module's `Kill Occluded Particles` (default on) silently killed every particle born behind the tire. Particles spawned at the wheel hit sit under the visible mud surface, because the hit is sunk (ADR-012).
- The default Niagara sprite material reads as glowing dots; mud needs a lit masked material.
- `CaptureEditorImage` after `StartPIE(bSimulate)` with an auto-run rig and a preset camera is a workable visual check for effects.
- With no shadow maps (ADR-011), depth on the ground has to come from normals and geometry; colour and roughness changes alone read flat.

### Open follow-ups
- S2-14b ruts with depth; S2-14 code is committed with it.

---

## 2026-10-26 — Ruts with depth (S2-14b)

### Context
After the flat first pass of S2-14, the user asked for ruts that look dug in, still without physics. Built as S2-14b on top of the S2-14 trail targets, and signed off.

### What was built
- `MudVisual` procedural grid per patch (the slab is hidden, collision only), R16F depth target, depth-aware stamps with width growing with depth, `M_B1Mud` with WPO displacement, lip and gradient normals.
- S2-14 and S2-14b committed together.

### Decisions made
- ADR-008 amendment applied as written. Translucent blend toward the stamp depth instead of max (the canvas has no max blend).

### What we learned
- Without shadow maps, ground depth reads through normals; WPO alone would still look flat-lit.
- Narrow deep ruts look like black trenches; widening with depth (sloped walls) reads much better.
- An R8 depth target stripes gradient normals (0.4 cm steps); R16F fixes it.
- `ObjectTools.set_properties` can't grow a Custom node's `inputs` array and change elements together: read the array back and append one element at a time. `ProgrammaticToolset.execute_tool_script` builds a whole material graph in one call.

### Open follow-ups
- S2-15 fuel tuning, S2-16 Phase 1 close playtest.

---

## 2026-10-27 — Mud and fuel portability refactor (S2-18..S2-25)

### Context
After /validate, mud and fuel had to become copyable into another project (different movement component, fuel attribute, mud from PCG) before Sprint 3 edits the same sim code. Pure refactor: behaviour must not change. Baseline: gym 22/22, `Lvl_Offroad` 8/8 run on the pre-refactor binaries; every task re-ran both and compared all metrics (outputs in `Saved/B1Refactor/`).

### What was built
- ADR-015 mud surface contract (S2-18).
- `Mud/Sim/`: Chaos-free `B1MudWheelModel` and `FB1MudSimulation` composed into `FB1VehicleSimulation` via 6 hook calls (S2-19).
- `UB1MudVehicleComponent` + `IB1MudSimHost` on the movement component; all mud consumers (spray, trail, ShowDebug, Mud window, cheat, telemetry, rigs) read the component (S2-20). InMud tag and mud strain moved onto it (S2-21).
- `Fuel/UB1FuelConsumptionComponent`, `UB1FuelConsumptionData` (CoreRedirect from `UB1VehicleTuningData`), `IB1EngineLoadSource`, `IB1FuelLoadSource`; `AB1Jeep` keeps only the throttle cut-off binding. `BP_B1Jeep` got `Data` + `DrainEffect` via MCP (S2-22).
- `UB1MudTrailSurfaceComponent`; the trail subsystem no longer knows patches and prefilters by world box (S2-23).
- `UB1MudSettings` (channel, profile, spray asset), `LogB1Mud` / `LogB1Fuel`, `Mud/Debug/`, collision macros deleted (S2-24).
- `MIGRATION.md` and `.claude/rules/portability.md` (S2-25).

### Decisions made
- ADR-015 added. ADR-001 (mud config and future `SlopeHoldVsSinkDepth` on the mud component) and ADR-010 (fuel component, loads by name, `LoadRates["Mud"]`) amended.
- `Mud/` includes the interface-only `Fuel/B1FuelLoadSource.h` (one-way); the alternative put fuel code on the jeep.
- Project tags/attribute are set in `AB1Jeep`'s constructor; asset refs on the BP.

### What we learned
- The Chaos vehicle manager refreshes `WheelStatus` and calls `Update` inside `OnPhysScenePreTick`. A PrePhysics tick or another PreTick binding (multicast broadcasts in reverse) runs before that refresh; mud learned new materials a frame late and the rigs moved up to 5 % (`DeepRunUp` 57.0 → 54.5 m). `OnPhysSceneStep` fires right after on the game thread and reproduces the old timing exactly.
- Run-to-run noise on the rigs is ~0.4 % (firm `FullThrottle5s`), and much larger relative noise on near-zero values of stuck rigs.
- Comparing rig lines in PowerShell: capture the test name before nested `-match` (it overwrites `$Matches`), and parse numbers with `InvariantCulture` (the machine uses a decimal comma).
- `AActor::GetVelocity` on the jeep isn't the physics velocity check the old code used; the clearance log measured at spawn until it used the body's forward physics speed.
- Toggling a WP rig property for a capture dirties its external actor package; `StopEditor` then blocks on the save dialog and `-Force` needs the user. Next time: avoid property edits for captures or save the actor package via `AssetTools.save_assets` with its `__ExternalActors__` path before stopping.

### Open follow-ups
- User validation of S2-18..S2-25 (drive the gym, Vehicle Status rates/save, Project Settings → B1 Mud, read MIGRATION.md).
- `B1Mud_CrossMedium`'s external actor package was re-saved by the dialog (no intended change); revert if it bothers.
- S2-15 fuel tuning, S2-16 Phase 1 close playtest.

---

## 2026-10-27 — Playtest feedback: tank, HUD fuel bar, spray look (S2-26..S2-28)

### Context
The user drove the gym after the portability refactor and asked for three changes: a 20 L tank so consumption shows, a fuel bar in the HUD next to speed and gear, and mud spray that reads as mud, not smoke.

### What was built
- S2-26: `UB1VehicleAttributeSet` Fuel/MaxFuel 20 L. Fuel rigs unchanged (rates are per hour).
- S2-27: `UMudDriverUI::UpdateFuel` fills optional bound `FuelBar` / `FuelText` (green → red by hue down to 15 %); `AB1PlayerController::Tick` feeds it from the ASC. HUD duplicated to `/Game/B1/UI/WBP_B1VehicleHUD` via the UMG toolset (`FuelBox` column between gear and speed); `BP_B1PlayerController.VehicleUIClass` points at it.
- S2-28: one dark brown `SprayColor` on every preset; `M_B1MudSpray` gets a flattened dome normal (Custom node), roughness 0.2, specular 0.6; sprites 10–28 cm.
- Fixed a unity-build break from cf7c2cc: duplicate anonymous-namespace unit constants in `B1MudWheelModel.cpp`, `B1MudSimulation.cpp`, `B1VehicleSimulation.cpp`. The mud ones are now public in `B1MudWheelModel`.

### Decisions made
- Template HUD isn't edited; the B1 copy carries project UI.
- Spray colour is the same on every preset (it was per preset); `SprayColor` stays on the material for later variation.

### What we learned
- UBT adaptive unity takes files with uncommitted changes out of the unity blobs, so name clashes between .cpp files only show after commit. Check with `Build.bat MudDriverEditor Win64 Development -Project=... -DisableAdaptiveUnity` after adding .cpp files.
- Flat, unlit-looking dark sprite discs read as smoke; a dome normal plus low roughness makes the same sprites read as wet clumps. In bright sun, linear 0.26 brown looks pale tan on shaded sprites.
- MCP: `StartPIE` blocks ~2 s (warmup), so a 4 s rig is half over when it returns; capture immediately. `save_assets` with `[]` writes OFPA rig packages that `save_actor` reports as missing. PowerShell's `R` is an alias (`Invoke-History`), don't name helpers `R`.
- The UMG toolset (`AddWidget`, `ToggleWidgetAsVariable`, `CompileWidgetBlueprint`) plus `ObjectTools` on slots/widgets builds designer layouts end to end.

### Open follow-ups
- User checks: fuel bar drains and turns red, spray reads as mud on every preset.
- `B1Mud_DigInMedium`'s external actor package was re-saved (bAutoRun/SettleTime toggled and restored); revert if unwanted.
- S2-15 fuel tuning (now with the 20 L tank), S2-16 Phase 1 close playtest, user validation of S2-18..S2-25.

---

## 2026-10-27 — Sprint 2 closed

### Context
The user checked the portability refactor (S2-18..S2-25), the 20 L tank, the HUD fuel bar and the new spray: all good.

### What was built
- S2-15 and S2-16 marked done; Sprint 2 closed. ROADMAP Phase 1 (Mud) and Phase G (GAS + fuel) ticked and signed off; the fuel gauge stretch item is done.

### Decisions made
- Fuel rates stay at 6 / 240 L/h, `Mud` 300 L/h with the 20 L tank.

### What we learned
- PowerShell: in an array literal the comma binds tighter than `+`, so `@('a', 'b' + "`n" + 'c')` yields four elements. A find/replace loop over such pairs replaced every newline in SPRINTS.md; reversing that one substitution restored it. Use the Edit tool for doc edits, or build each pair in parentheses.

### Open follow-ups
- Sprint 3 (mud + winch core) is next.
- `B1Mud_DigInMedium`'s external actor package was re-saved with no intended change; left out of the commit.

---

## 2026-10-27 â€” S3-02 slope baseline (before any slope rule)

`Lvl_Gym_Winch`, 20° slope pair (S3-01), headless. Rigs in `B1Tests/Slope`, all 15 s from X 2 m on the run-up (18 m of flat before the foot) unless noted. Plateau top is +9.26 m above a run-up rig.

| Rig | dist | vmax | maxH | endH | sink F/F/R/R | Notes |
|---|---|---|---|---|---|---|
| `B1Slope_Base_Firm50` | 58.5 | 8.99 | 9.34 | 9.27 | – | climbs, stops at the kerb |
| `B1Slope_Base_Medium30` | 26.7 | 5.22 | 1.79 | 1.79 | 17/17/21/21 | stalls low on the slope |
| `B1Slope_Base_Medium50` | 46.8 | 8.96 | 8.67 | 8.67 | 18/17/23/23 | still climbing at 15 s (vend 1.92) |
| `B1Slope_Base_Medium100` | 56.4 | 16.84 | 9.34 | 9.24 | 35×4 | tops out; 16.8 m/s at the foot carries ~14 m of height |
| `B1Slope_Base_MediumIdle` | 0.06 | 0.19 | – | −0.74 | 14/14/20/20 | mid-slope, 10 s, zero input: holds |
| `B1Slope_Base_MediumIdleAwake` | 0.10 | 0.19 | – | −0.76 | 14/14/20/20 | same with `bDisableVehicleSleep`: still holds |

- Why it holds: below 10 cm/s with no input Chaos brakes fully (`StopThreshold`), and the vehicle sleeps on slopes under 30° (`SleepThreshold` 10 cm/s, `SleepSlopeLimit` 0.866). Even awake, locked wheels on medium at resting depth have μ ≈ 0.5 × 0.78 ≈ 0.39 > tan 20° = 0.36.
- Why medium climbs: at 50 % the wheels barely dig (17–23 cm), so grip stays above tan 20°; at 100 % the run-up's momentum does it.
- Rig: new `maxH=` (peak height above the rig) with `MaxPeakHeight` + toggle, and `bDisableVehicleSleep`.
- The duplicate of `Lvl_Gym_Mud` had carried over its five `B1Fuel_*` rigs (root folder); removed.

---

## 2026-10-27 — Sprint 3 slopes: S3-01..S3-06

### Context
Sprint 3 opened with the muddy slope from the replan: firm climbs, medium can't, a stopped jeep slides. S3-01 built a 20° test pair; the user's first drive showed medium climbing easily and a stopped jeep never sliding. The user asked to run S3-02..S3-06 in one go and batch the user checks at S3-06.

### What was built
- `Lvl_Gym_Winch`: final slope lanes at 20°, 40 m long: firm / medium / deep, a shared plateau with a kerb, anchor posts, three spawns and labels (S3-01, S3-05).
- Rig `maxH` + `MaxPeakHeight`, `bDisableVehicleSleep`, `pull=`; baseline and final slope rigs (S3-02, S3-06; 8 in `B1Tests/Slope`).
- Net-force standstill rule in rolling resistance and belly drag (S3-03).
- Keep-awake on muddy slopes: `IB1MudSimHost::SetMudKeepAwake` → `SleepSlopeLimit` 1 (S3-03).
- No engine braking on mud wheels on a slope with no input (S3-04).
- Slope pull: `SlopePull` × gravity along the ground on every mud wheel on a slope (S3-04 rework). `BP_B1Jeep` 0.3.
- `p.Chaos.Suspension.SlopeThreshold=1.0` in `DefaultEngine.ini` (ADR-016).
- Mud window `Pull N` column, `Slope pull` and slope-angle fields; telemetry `pull=`.

### Decisions made
- Chaos suspension anti-slide off project-wide (ADR-016).
- Release the brake on muddy slopes with no input (the user's choice over a per-material grip factor).
- Slope pull replaces the depth-based slope hold (the user's call after the S3-06 playtest: a dug-in jeep couldn't climb or reverse). ADR-001 amendment updated.
- θ = 20°, 40 m: steepest angle where firm 50 % climbs without the anti-slide; 40 m so the medium 100 % run can't clear the top on run-up momentum.

### What we learned
- Three Chaos features hold a stopped vehicle on slopes:
  - sleep (under 10 cm/s, slopes up to 30°);
  - engine braking as brake torque (RPM × `EngineBrakeEffect`, 180 Nm at idle), whose brake model locks a near-stopped wheel;
  - the suspension constraint's anti-slide (pushes straight up below 10 mph on slopes up to 45°). This one also helps every slow climb.
- `FSimpleWheelSim` produces no longitudinal force with zero drive and brake torque: free wheels roll without resistance except ours.
- `GetRoadSpeed()` is the wheel's rolling speed; a braked wheel reads ~0 while the jeep slides. Use body ground speed for sliding.
- A rig's `lastDisp` includes coasting from the previous step; add a settle step before the measured one.
- MCP: `set_properties` can't change and resize an array in one call in either direction (set the elements at the current size first). A `StopEditor` save dialog blocks MCP; `save_assets([])` clears it and the close then finishes.
- `RunGymTests.ps1 -ExtraArgs '-dpcvars=b1.Mud.Telemetry=1,...'` gets telemetry from headless runs.

### Open follow-ups
- `Lvl_Offroad` moved with the anti-slide off (FullThrottle5s 85.3 → 89.0 m, Push drift 7.6 → 0.7 m); all PASS. Worth a feel check on its hills and the swamp.
- `SlopePull` is per vehicle, not per mud preset; move it onto `PM_B1Mud_*` if medium and deep should differ.
- Next: S3-07 winch + anchor components.

---

## 2026-10-27 — Sprint 3 winch core: S3-07..S3-10

### Context
With the slopes signed off, the winch came next: components and a cable visual (S3-07), then tension in the sim, pull input and the drag caps (S3-08..S3-10) in one go, with the user check batched after S3-10. The user picked the pad layout: D-pad Up pulls, D-pad Down toggles attach/release.

### What was built
- `Source/MudDriver/Winch/`: `UB1WinchComponent` (winch point on `AB1Jeep`'s nose, config, attach/detach/toggle, `Pull`, tag, sim I/O), `UB1WinchAnchorComponent` (`FindNearest`), `AB1WinchTree` → `/Game/B1/Winch/BP_B1WinchTree`. Cable: a fresh `UCableComponent` per attach, 7 cm, red `MI_B1WinchCable`, length = rest length.
- Winch force in `FB1VehicleSimulation` (S3-08): capped one-sided spring-damper at the winch point from the body handle; physics-side rest length with reel-in and a stalling drum. `FB1WinchSimInput/Output` through the movement component; keep-awake while attached.
- `Vehicle.State.Winching` loose tag; cheats `B1WinchAttachNearest`, `B1WinchDetach`; placed-jeep `StartAnchor` + editable `Pull` for MCP checks.
- Input (S3-09): `IA_B1WinchPull` (E / D-pad Up), `IA_B1WinchToggle` (Q / D-pad Down) in `IMC_Vehicle_Default`, bound natively in `AB1Jeep`.
- Drag caps (S3-10): portable `FB1MudSimulation::SetExternalForce`; rolling resistance and belly drag count it as push. MIGRATION.md updated.
- Plugin `CableComponent`, module deps `CableComponent`, `Chaos`.

### Decisions made
- Reel-in also takes up slack; the drum stalls when the spring force reaches `MaxPullForce`, so the cable never stores more than the cap (ADR-007 amendment).
- One toggle input (attach nearest / release) instead of release-only, until the on-foot hook carry (user's call).
- Winch point and velocity come from the Chaos body handle at the start of the step, so the force is ready before the mud drag caps run.

### What we learned
- Before S3-10, mud barely resisted the winch: the drag caps only let the tire push through, so the winch dragged the jeep almost freely. The ADR premise ("a belly-stuck jeep wouldn't move") was the wrong way round; the change is right either way.
- `FRigidBodyHandle_Internal` accessors need the `Chaos` module at link time (`TThreadSingleton<FPhysicsThreadContext>`).
- `UCableComponent` with the default single solver iteration sags a 13 m cable through the floor; 16 iterations + stiffness looks like steel.
- MCP: `DataAssetTools.create` makes `UInputAction`s; `ProgrammaticToolset` scripts can append to an IMC's mapping array (get, append, set the whole struct) and keep instanced modifiers. MCP can't call functions, so placed-actor properties (`StartAnchor`, `Pull`) stand in for calls in Simulate checks.
- Adding and removing temporary actors leaves a WP map dirty and blocks `StopEditor`; save the map after removing them (no change on disk).
- At world teardown the anchor can be destroyed before the winch; the cable pointer, not the anchor, marks "attached".

### Open follow-ups
- Dug-in belly case under winch untested (needs the S3-11 rig); covered by S3-15 `B1Winch_DeepStrip`.
- No trees in the gyms yet (S3-14); user tests by dropping `BP_B1WinchTree` into a level.
- Pad pull is digital (D-pad); analog pull only if a trigger gets mapped later.
- Next: S3-11 rig winch support.
## 2026-10-27 — Winch debug, deep strip, winch rigs, checkpoint (S3-11..S3-16)

### Context
The winch pulled (S3-08..S3-10) but had no rig support, debug UI or gym layout. This session built S3-11..S3-15 in one go with the user's checks batched into the S3-16 playtest. The user then playtested, tuned the winch from the new window and asked to close the checkpoint.

### What was built
- Rig winch support (S3-11): `WinchAnchor`, per-step `WinchPull`, metrics `tensionMax / tensionOsc / anchorDistMax / rest` and thresholds; detach at finish.
- Winch ImGui window (S3-12, `Debug/B1ImGuiWinch`): anchor, attach/detach, length and tension bars, tag, per-wheel net push vs drag, typed params with "Save to asset" into the BP component archetype. New mud sim output `NetPush` per wheel. CVar `b1.Winch.Telemetry` (`B1Winch` line).
- `ShowDebug B1Winch` (S3-13, `Debug/B1WinchShowDebug`).
- `Lvl_Gym_Winch` (S3-14): deep strip `Mud_DeepStrip` in a gap cut by splitting the floor into four tiles, trees at the strip and on the plateau (posts removed), `Spawn_4_DeepStrip`.
- Winch rig suite (S3-15, `B1Tests/Winch`): FirmPull, Stability20/120, CableLimit, CableSnatch (measurement), DeepStrip, SlopeMedium, SlopeDeep.
- S3-16: user's tuning on `BP_B1Jeep` (60 kN, 80000 cm cable); rig tension limit 63 kN; all suites PASS (winch gym 16/16, gym 22/22, offroad 8/8). ADR-007 accepted.

### Decisions made
- ADR-007 accepted (stability rigs pass; same tension at 20 and 120 fps).
- Stability `MaxPeakOscillation` is 0.7, not 0.5: the deep-mud rigs without a winch show 0.655 from the drop into deep at script start.
- Slope winch rigs start 15 m up the slope: the plateau trees are about 48 m from the slope foot, which was past the 40 m cable.
- Winch window edits are live on the jeep; Save copies them to the BP's component archetype (the component isn't an asset object).

### What we learned
- Tension is clamped at `MaxPullForce` even while the cable is being stretched, so driving into a taut cable overruns it (`B1Winch_CableSnatch`: 42.7 m at 40 kN, 41.2 m at 60 kN, rest 38.3).
- Because rest = distance at attach, `CableLimit` from standstill can't move the jeep at all; it only proves the trivial case.
- MCP: `save_actor` and `save_assets([actor package])` fail with "Asset does not exist" on these OFPA rig packages, even when they're on disk; only `save_assets([])` wrote them. `set_properties` on `InputScript` needs the same-size write first (scripted helper does that).
- `FCStringAnsi::Snprintf` needs a literal format (static_assert); UE treats a local shadowing a member as an error, so the sim member is `WheelNetPush`.
- On firm ground a rig 1 m above the floor reads `endH ≈ −0.98`; the jeep's actor origin sits at the ground.

### Open follow-ups
- Winch snatch overrun (Sprint 3 backlog): cap only the reel-in force and let a stretched cable hold harder, or accept it as drum slip.
- `MaxCableLength` on `BP_B1Jeep` is 80000 cm (800 m); trees were hooked at 49–52 m, so 8000 may have been meant. Confirm.
- `ShowDebug B1Winch` hasn't been seen on screen by the agent.
- Next: Sprint 4 (on foot).

## 2026-10-27 — Retire Lvl_Offroad, rigs to Lvl_Gym_Core

### Context
The template's `Lvl_Offroad` (World Partition landscape, ~55 MB of external actors) was too large to keep, but it still held 8 functional tests: the vehicle baseline and stepping rigs on the landscape, and the mud-channel rigs on their own floors at Z≈5000.

### What was built
- `/Game/B1/Gym/Lvl_Gym_Core`: a duplicate of `Lvl_Gym_Winch` stripped to lighting, one 60 × 180 m floor (`G_Floor`, top at Z 0) and `Spawn_Player`. Game mode `BP_B1GymGameMode`.
- The 8 rigs recreated with identical config (checked property by property against the originals): `B1Tests/Step` (FullThrottle5s, Spring20/120, Push20/120 at the origin, same yaws) and `B1Tests/MudChannel` (WheelsOnPlane, ChassisThroughRail, PatchSurface). The mud-channel geometry (`Gym/MudChannel`) was moved as a group by (+2563, −771, −5000), with the patch `Size` 800 × 800.
- `Lvl_Offroad.umap` and its `__ExternalActors__` / `__ExternalObjects__` folders deleted (`git rm`); nothing else referenced it. `RunGymTests.ps1` defaults to `Lvl_Gym_Core`.
- Headless: 8/8 PASS (`core_migrate.log`).

### Decisions made
- A separate core map instead of merging the rigs into `Lvl_Gym_Mud`, so the suites stay apart (core 8, gym 22).
- Recreate rigs rather than duplicate `Lvl_Offroad`: a WP duplicate would carry the unloaded landscape proxies along.

### What we learned
- New baselines on flat ground: FullThrottle5s 89.2 → 77.0 m (old run went 7.8 m downhill), Push20/120 0.7 → 12.7/12.8 m (no slope holding it), Spring settleTaut 2.95/2.93 → 1.55/1.58 s. The mud-channel rigs are unchanged (10.22 m, endH −0.31/−0.31). The 20 vs 120 fps pairs still agree.
- MCP: `AssetTools.duplicate` on a WP map copies its external actors; the copy is dirty until `save_assets([new map])`, and `load_level` refuses to open it before that. Empty outliner folders disappear, so `get_actors_in_folder` on them throws.
- `BP_B1MudPatch` placed fresh defaults to `Size` 2000 × 2000 and spatially loaded; copy instance overrides explicitly.

### Open follow-ups
- The rest of `Variant_OffRoad/` (landscape materials, RVT, template BPs) may now be partly unused; not checked.

## 2026-10-27 — Sprint 4 refined (on foot)

### Context
Sprint 3 closed with the mud + winch checkpoint. Sprint 4 had been outlined on 2026-10-26, before Sprint 3 shipped, so parts of it no longer matched the code (`Lvl_Offroad` deleted, `IA_B1WinchRelease` replaced by a toggle, keep-awake and the fuel components' unpossessed behaviour already in place). Refined with `/sprint`; no code changes.

### What was built
- SPRINTS.md: Sprint 4 rewritten as S4-01..S4-10. New S4-01 winch housekeeping; ability-input and teleport rig steps so enter/exit and hook carry get headless rigs; park rigs on the firm and medium slopes; carry state in the Winch window and `ShowDebug B1Winch`; log lane in S4-09. Sprint 3 marked closed; its snatch backlog item resolved.
- DECISIONS.md: ADR-007's hook-carry reference S4-06 → S4-07.

### Decisions made
- `MaxCableLength` on `BP_B1Jeep` changes from 800 m (80000 cm) to 80 m (8000 cm), user's pick. The 80 m cable is the on-foot leash. Applied in S4-01.
- Getting out parks the jeep with the handbrake: it holds on firm ground but must still slide on a muddy slope (user). Built in S4-05.
- Winch snatch overrun accepted as drum slip; ADR-007 gets the line in S4-01.
- In the car, `IA_B1WinchToggle` becomes release-only once the hook carry works (per the ADR-007 amendment); attach-nearest stays as a cheat and an ImGui button.

### What we learned
- Chaos `bRequiresControllerForInputs` clears inputs on an unpossessed vehicle, so the parked handbrake needs `SetParked` / `ParkingEnabled` or the sim (S4-05 research).
- `FB1MudSimulation`'s no-input brake release already treats handbrake and parking as input, so a parked jeep keeps its brake torque; whether `SlopePull` still slides it on medium is untested.
- The template adds mapping contexts in `AMudDriverPlayerController::SetupInputComponent`, and `OnPawnDestroyed` possesses a new jeep even when the player is on foot (S4-04).

### Open follow-ups
- Next: `/work` on S4-01.

## 2026-10-27 — S4-01 winch housekeeping, S4-02 ability set + input config

### Context
`/work` on Sprint 4, through the first task with a user check (S4-02). Starting state: Sprint 3 closed and Sprint 4 refined; no ability granting or ability input existed yet (ADR-014 proposed).

### What was built
- S4-01: `BP_B1Jeep` was already at `MaxCableLength` 8000 cm. `B1Winch_CableLimit` / `CableSnatch` moved to X 95.5 m, Y −50 m (77.8 m from `Tree_FirmPull`), CableLimit limit 80.5 m. Winch gym 16/16 PASS (`s401_winchgym.log`).
- S4-02: `UB1AbilitySet`, `UB1InputConfig`, `UB1AbilitySystemComponent::AbilityInputTagPressed/Released`, `AB1PlayerController::InputConfig` + bindings, `UB1GASSettings::AbilitySets` granted in `AB1PlayerState::BeginPlay`, `UB1GameplayAbility_TestPing` / `GA_B1TestPing`, tag `InputTag.Debug.TestPing`, log `LogB1Ability`. Assets `AbilitySet_B1Player`, `InputConfig_B1Player`, `IA_B1TestPing`, `IMC_B1Abilities` (P), added to `BP_B1PlayerController`'s `DefaultMappingContexts`.
- Rig: `FB1GymInputStep::AbilityInputTag`, metric `abilities=`, thresholds `Min/MaxAbilityActivations`; `B1Ability_TestPing` in `Lvl_Gym_Core`. Core 9/9 PASS (`s402_core.log`).

### Decisions made
- Snatch overrun = drum slip, recorded in ADR-007.
- ADR-014 accepted: sets from project settings (no pawn data), input config on the controller for both pawns, immediate activation without Lyra's per-frame input queue or activation policies.

### What we learned
- Snatch at 80 m: 82.69 m peak vs 77.79 rest (4.9 m slip at 60 kN).
- MCP: `save_actor` / `save_assets([path])` can fail with "Asset does not exist" for an external actor package that is on disk and tracked; `save_assets([])` saved it, and `git status` showed only the two rig files.
- MCP: setting an array whose first element has an empty `FGameplayTag` fails as an ambiguous resize unless the first element is read back (`tagName: "None"`) and reused verbatim.

### Open follow-ups
- S4-02 user check pending: press P in PIE, `LogB1Ability: TestPing activated #n`.
- `Lvl_Gym_Mud` (22) not rerun after S4-02; no physics change.
- Next: S4-03 `AB1Character`.

## 2026-10-27 — S4-03 on-foot character

### Context
`/work` on Sprint 4 after S4-01/02. Starting state: the controller accepted any pawn but only the jeep existed; no on-foot input, no character in the rigs.

### What was built
- `AB1Character` (`Player/`) + `BP_B1Character`: first-person capsule 35 × 90, camera at +70 cm with pawn control rotation, ASC from the PlayerState. Adds `IMC_B1OnFoot` at priority 1 in `PawnClientRestart`, removes it and clears `State.Avatar.OnFoot` in `UnPossessed`.
- Input assets duplicated from the template's `/Game/Input`: `IA_B1Move`, `IA_B1LookMouse`, `IA_B1LookStick`, `IA_B1Jump`, `IMC_B1OnFoot` (`/Game/B1/Input`).
- `AB1PlayerController::OnPossess` sets `State.Avatar.OnFoot`; `AB1GameMode::CharacterClass` (`BP_B1GameMode` and `BP_B1GymGameMode` → `BP_B1Character`); cheat `B1SpawnCharacter`.
- Rig: `StartOnFootClass`, `FB1GymInputStep::Move`, metrics `pawn= avatar= charZmin=`, threshold `CharZminRange`. `B1OnFoot_WalkFirm` (core, 29.2 m) and `B1OnFoot_WalkDeep` (winch gym, charZmin −0.669 on the 70 cm bottom). Core 10/10, mud 22/22, winch 17/17 headless.

### Decisions made
- Until S4-04, the character adds its own mapping context, above the vehicle contexts so shared keys (WASD) walk.
- Mouse look keeps the template path (`AddControllerYaw/PitchInput`, legacy input scales on); stick look writes deg/s into `RotationInput` (Lyra's 300 / 165), so the legacy ×2.5 and the frame rate don't apply.
- Stick pitch sign flipped after the user's pad check (stick up looked down).

### What we learned
- The capsule already ignores mud surfaces: `B1MudSurface` blocks only WorldDynamic, so no collision change was needed for walking on the hard bottom.
- MCP: a parent BP CDO edit doesn't reach child BPs, and a saved child keeps the old value as an override (cause of "no CharacterClass" in the gyms). Rule added to `mcp-tools.md`.
- MCP: adding or removing rows in an IMC's `DefaultKeyMappings` scrambles the per-row modifier lists; changing rows in place at the same size works. The Right-arrow row became `Mouse2D`, the Left-arrow row has key None.
- The legacy `InputPitchScale` is −2.5, so mouse look needs no Negate modifier when the code negates Y.

### Open follow-ups
- Stick pitch fix is live-coded only; the next full build bakes it in.
- Mouse sensitivity untuned (`MouseLookScale` on `BP_B1Character` if needed).
- Next: S4-04 mapping-context swap on possession.

## 2026-10-27 — S4-04 mapping-context swap

### Context
Same session, after the S4-03 commit and the pad pitch fix. Starting state: the template added every vehicle context at startup; the character added its own on-foot context.

### What was built
- `AB1PlayerController::VehicleMappingContexts` / `OnFootMappingContexts`, swapped in `OnPossess` (`ApplyPawnMappingContexts`, logged on `LogB1Vehicle`). `BP_B1PlayerController`: `DefaultMappingContexts` = `IMC_B1Abilities`; vehicle set = `IMC_Vehicle_Default`, `IMC_Vehicle_MouseLook`, `IMC_SteeringWheel`; on-foot set = `IMC_B1OnFoot`; `MobileExcludedMappingContexts` empty, `bUseSteeringWheelControls` off.
- `AB1Character` lost its context handling.
- `AMudDriverPlayerController::OnPawnDestroyed` respawns only for the possessed vehicle.
- Core 10/10, mud 22/22 headless (`s404_*.log`).

### Decisions made
- Pawn-type contexts live on the controller (Lyra keeps them per pawn data; we have two pawn types and one controller).

### What we learned
- `APawn::Destroyed` unpossesses before `OnDestroyed` broadcasts, so a destroy handler can't compare against `GetPawn()`; the template's `VehiclePawn` (cleared by `OnPossess` on foot) works.

### Open follow-ups
- Touch controls no longer exclude the vehicle mouse-look context (desktop only for now).
- Next: S4-05 seat, engine off, parked on exit.

## 2026-10-27 — S4-05 park, S4-11 interaction, S4-06 enter/exit

### Context
`/work` run to the S4-06 user check. Starting state: the character could only be spawned by a cheat; nothing parked the jeep, cut its engine or put the player back in it.

### What was built
- S4-05: `AB1Jeep::DriverSeat`; `AB1PlayerController::OnFootCharacter` follows possession (seated in any possessed jeep, spawned once from `CharacterClass`). `AB1Character::SitIn` / `LeaveSeat`. `AB1Jeep::SetParked`: Chaos parking + `EB1EngineCut::EngineOff` + `Vehicle.State.EngineOff` + park sleep (`ParkedSleepThreshold` 50 cm/s). `IB1EngineLoadSource::IsEngineRunning` stops fuel use. Rigs `B1Park_FirmSlope` / `B1Park_MediumSlope`, step `bParked`, metric `seated=`, threshold `MaxFuelUsed`.
- S4-11: `Interaction/` (`IB1Interactable`, `FB1InteractionOption`, `UB1InteractionScanComponent`), `GA_B1Interact`, `IA_B1Interact`, `AB1TestInteractable`, rig `B1OnFoot_Interact`. ADR-017.
- S4-06: `ExitVehicle` / `EnterVehicle` on the controller, `GA_B1ExitVehicle` / `GA_B1EnterVehicle`, jeep "Enter" option at `DriverDoor`, cheats `B1ExitVehicle` / `B1EnterVehicle`. Rig step `ExpectAvatar`, metric `fuelOutside=`. Rigs `B1OnFoot_ExitEnter10` (core), `B1OnFoot_ExitDeepSlope` / `ExitDeepStrip` (winch).
- Input after the user check: pad X = interact (removed from vehicle throttle; RT stays), F on keyboard.
- Headless: core 12/12, mud 22/22, winch 21/21 (`s406b_*.log`). User check passed.

### Decisions made
- ADR-017: interaction options + one interact ability (Lyra-style scan).
- Parked jeep held by Chaos sleep off mud, not a hold force (S4-05 note; creep measured 1.31 m / 10 s).
- One ability input press activates only specs that could activate before the press (ADR-014 amendment).
- Exit never fails: lifts, `FindTeleportSpot`, then a drop above the door (user: walking is the most prototype feature).

### What we learned
- Locked wheels on a firm 20° slope creep at ~14 cm/s, just above Chaos's 10 cm/s sleep speed.
- Unpossessed Chaos vehicles replay `ReplicatedState` (last driver input); parking must clear it.
- The jeep's physics body blocks a capsule 1.7 m from its centre sideways; door spots sit at 2.3 m.
- On a sunk or sloped jeep, a ground trace starting 1.5 m above the spot can start inside a 1 m hard-bottom slab and miss it.
- MCP `set_properties` array resize needs element 0 resent bit-identical (0.3 round-trips as 0.30000001192092896); long scripts go through the HTTP endpoint from PowerShell.

### Open follow-ups
- Next: S4-07 winch hook carry.
- Exit spot search could use the jeep's bounds instead of fixed offsets if other vehicles come.

## 2026-10-27 — Sprint 4 critique and winch additions

### Context
After the first Sprint 4 refinement, `/plan-analyse` reviewed the open tasks (S4-03..S4-10) against ARCHITECTURE.md and ADR-004/007/014. The critique went into PLAN.md (there was no active plan) and was applied with `/sprint`; the S4-03..S4-06 and S4-11 work since then built on it. After S4-06 the user asked for two winch additions.

### What was built
- PLAN.md: critique round 1 for Sprint 4 (verdict: needs revision). Applied in SPRINTS.md:
  - new task S4-11 (interaction scan, ADR-017);
  - S4-03 gained a rig move step and walk rigs;
  - S4-05 seats the character in whichever jeep is possessed;
  - S4-06 does the swap in the controller;
  - S4-07 gained hook states, leash and enter-with-hook rigs;
  - S4-08 gained the Abilities panel;
  - S4-09 got a research order;
  - the sprint got an out-of-scope list.
- SPRINTS.md: new tasks before S4-07.
  - S4-13: signed winch pull (reel-out up to `MaxCableLength`), detach on its own key, keyboard rebind, rigs `B1Winch_ReelOutSlope` and `B1Winch_ReelOutLimit`.
  - S4-12: the cable lies on the ground and the mud surface. Research `UCableComponent` collision first, trace-based drape as the fallback, three Simulate captures.
  - S4-07, S4-08 and S4-10 dependencies updated.

### Decisions made
- On-foot interact goes through a Lyra-style interaction scan (user; became ADR-017 in S4-11).
- At full cable length, a sliding jeep drags the character; getting in while carrying the hook stows it (user).
- `State.Avatar.*` stays in `AB1PlayerController::OnPossess`. Enter/exit abilities are thin and instant, with the swap in the controller. A carried hook is visual only and never reaches the sim.
- Winch controls (user): pad D-pad Up reels in, D-pad Down reels out, Y detaches, X interacts. Keyboard: W/S/A/D drive, Arrow Up/Down reel in/out, Q detaches, E interacts; Arrow Left/Right are unbound in the jeep. The attach/detach toggle goes away; ADR-007 gets amended in S4-13.
- Powered reel-out is in scope; unpowered free-spool stays out.

### What we learned
- Reel-out makes a long slack cable the normal case, so the cable has to rest on the surface rather than sag through it.

### Open follow-ups
- Next: S4-13, then S4-12, then S4-07.
- Check `IMC_Vehicle_Default` for template bindings on the arrows, Q, E and Y before rebinding (S4-13).
