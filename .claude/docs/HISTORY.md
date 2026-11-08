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

## 2026-10-27 — S4-13 winch reel-out, detach key, rebind

### Context
The winch could only shorten the cable, and the in-car controls still used the S3 attach/detach toggle. S4-13 (added mid-sprint) brings signed pull, a detach-only key and the user's keyboard/pad layout before S4-12 and S4-07.

### What was built
- Signed winch pull (−1..1) through `UB1WinchComponent`, `FB1WinchSimInput` and `FB1VehicleSimulation::UpdateWinch`: reel-out grows the rest length at `ReelOutSpeed` (new, `BP_B1Jeep` 300 cm/s) up to `MaxCableLength`, ignoring the stall.
- Reel-out damps the cable's stretch rate (body speed minus payout); reel-in keeps body-speed damping.
- `IA_B1WinchDetach` (renamed from `IA_B1WinchToggle`), `AB1Jeep::WinchDetachAction` → `Detach`. `IMC_Vehicle_Default`: arrows Up/Down and D-pad Up/Down reel in/out, Q / pad Y detach; template arrow drive/steer and E pull removed. Interact F → E.
- Winch window Reel-out field, signed pull in the window, ShowDebug and telemetry.
- Rig thresholds `MaxLastStepSpeed`, `MinLastStepTension`, `MaxRestLength`; rigs `B1Winch_ReelOutSlope` and `B1Winch_ReelOutLimit` in `Lvl_Gym_Winch`.

### Decisions made
- ADR-007 amended: signed pull, reel-out ignores the stall, stretch-rate damping on reel-out only, detach key.
- Start-up overshoot on reel-out (≈5 m/s for a second or two, then 3.0 m/s) accepted by the user; the rig measures the last 3 s.

### What we learned
- Damping on body speed alone fights any steady reel: a jeep descending at 3 m/s read as +60 kN and snatched in a loop. Using stretch rate on reel-in made every pull step to the cap, so reel-in keeps the old term.
- MCP `set_properties` can create an instanced subobject (e.g. an Input Modifier) by passing the class path as the refPath; array shrink needs every remaining element resent unchanged, which is easiest from a `ProgrammaticToolset` script that reads, slices and writes back.
- New rigs start with one default `InputScript` element; grow the array by repeating it before setting real values.

### Open follow-ups
- Mud gym and full core not rerun this session (untouched); run with the full suite.
- Next: S4-12 cable on ground and mud.

## 2026-10-27 — S4-12 cable on ground and mud

### Context
After S4-13 a slack winch cable had no collision, so it sagged through the floor and into the mud, and from the seat you couldn't tell you were hooked.

### What was built
- `UB1WinchComponent::SetupCableCollision`: the `UCableComponent` sweeps as WorldDynamic, QueryOnly, blocking WorldStatic, WorldDynamic and the mud channel from `UB1MudSettings`, ignoring vehicles and pawns.
- `CableSegmentLength` (100 cm, replaces `CableSegments`), `CableSolverIterations`, `bCableCollision`, `CableCollisionFriction` in `Winch|Visual`; segments sized for `MaxCableLength`.
- `UB1VehicleMovementComponent::SetWinchInput` wakes a sleeping body while the winch reels.

### Decisions made
- `UCableComponent` collision instead of a trace-based drape (ADR-007 amendment). The cable stays visual only.

### What we learned
- `UCableComponent` collision sweeps on the component's own object type and responses, with no owner ignore; filter by type. Mud surfaces block only WorldDynamic, so the cable must be WorldDynamic.
- Chaos island sleep can still sleep a still jeep with `SleepThreshold` 0, and a sleeping body skips the vehicle sim, which froze reel-out at 54 m.
- In Simulate, World Partition cells far from the editor camera (X −5 m, Y −65 m in the winch gym) aren't loaded, so placed vehicles there fall.
- Live Coding only patches the running editor; rebuild before headless runs.

### Open follow-ups
- Cable sweep cost not measured (~4000 sweeps/s per 80 m cable).
- Next: S4-07 winch hook carry.

## 2026-10-27 — S4-07 winch hook carry

### Context
After S4-12/13 the winch could only be hooked from the seat (debug cheat / Winch window). S4-07 adds the Uncharted-style on-foot loop: take the hook at the jeep, carry it to a tree, attach, get back in and winch.

### What was built
- `EB1WinchHook` (stowed / carried / attached) on `UB1WinchComponent`: `TakeHook`, `DetachToCarrier`, `AttachTo` from any state, `Detach` stows; carried cable pays out with walk-out distance; leash swept back onto `MaxCableLength`.
- Interaction options: jeep front "Take hook" / "Return hook", anchors "Attach" / "Detach"; abilities `GA_B1TakeHook`, `GA_B1ReturnHook`, `GA_B1AttachHook`, `GA_B1DetachHook` (one C++ base + four subclasses) in `AbilitySet_B1Player`.
- `AB1Character::HookHand`; getting into a jeep stows a carried hook (`UpdateOnFootCharacter`).
- Rig: teleport step, Move steps for the player's exited character, `hook=` / `carryMax=` metrics, `ExpectHook` / `MaxCarryDistance`. Rigs `B1OnFoot_HookCarry`, `B1OnFoot_Leash`, `B1OnFoot_EnterWithHook` in `Lvl_Gym_Winch`.
- Headless: winch gym 26/26 (old rigs match S4-12 within 1 kN), core `B1OnFoot*` 3/3. User check passed.

### Decisions made
- Hook carry recorded as an ADR-007 amendment: carried hook is visual only, leash lives in the winch tick, one instant ability per hook action.
- Hook state has one owner (the winch); anchors and the jeep look it up with `FindCarriedBy` / `FindAttachedTo` instead of storing back-pointers.

### What we learned
- `UCharacterMovementComponent::SlideAlongSurface` is protected, but the `UMovementComponent` override is public; call it through the base pointer.
- `TWeakObjectPtr::IsExplicitlyNull` tells "was hooked, anchor now destroyed" apart from "never hooked", which keeps the detach log at world teardown.
- `StartEditor.ps1` after a failed build opens the editor on the old binaries without warning; build first.

### Open follow-ups
- `HookHand` placement and the on-screen prompt are S4-08.
- Mud gym not rerun (untouched); full suite after S4-08.

## 2026-10-27 — S4-08 interaction prompt and debug panels

### Context
After S4-07 the hook carry worked, but nothing on screen said what the interact key would do (Enter vs Take hook depends on which point is nearer), and the debug views didn't know about the carried hook.

### What was built
- `UB1InteractionPromptWidget`: "[E / X]  Take hook" from the scan's best option, keys queried from the interact action; added by `AB1PlayerController::BeginPlay`.
- Winch window and `ShowDebug B1Winch` show hook state and carry distance; on foot the window follows the carried hook.
- NetImGui Debug → Abilities: specs, input tags, active state, owned tags, interaction options.

### Decisions made
- Prompt widget built in C++ (no WBP) to skip UMG asset work over MCP; restyle later if needed.
- Pad face buttons shown as Xbox letters (user).

### What we learned
- NetImgui renders only in its remote server app, so PIE captures can't verify ImGui windows; the user checks those.
- Collapsed `UUserWidget`s don't tick: keep the root visible and collapse only the child text.
- A throwaway auto-run rig (created, run in PIE, removed, never saved) is a cheap way to stage the player for a screenshot.

### Open follow-ups
- S4-08 not committed yet; full suite (core, mud, winch) after the user check.
- Next: S4-09 winchable log.

## 2026-10-28 — S4-09 winchable log

### Context
`/work` on the last build task of Sprint 4. Starting state: the winch only pulled the jeep toward static anchors (trees); the ADR-007 plan for logs was an async tick on the log reading the published tension. The unreal-mcp server wasn't connected at session start, so editor work went through the HTTP fallback.

### What was built
- Opposite cable force on simulating anchors from the vehicle sim, in the same physics step: `FB1WinchSimInput::AnchorProxy` / `AnchorPointLocal`, `UpdateWinch` reads the log's pose and point velocity (relative closing-speed damping), `FB1VehicleSimulation::ApplyDeferredForces` applies −force + torque.
- `UB1WinchAnchorComponent::GetSimulatingBody`; anchors unhook their winches in `EndPlay`.
- `AB1WinchLog` → `BP_B1WinchLog` (cylinder, 1000 kg, anchor at one end); `Log_FirmLane` in `Lvl_Gym_Winch` (X 57.5 m, Y −57 m, 50 cm × 5 m).
- Rig threshold `MinAnchorTravel`, metric `anchorTravel=`; rig `B1Winch_Log` (log moves 11.7 m, tension peak 19.1 kN, jeep 0.42 m).
- Headless: winch 27/27, core 12/12, mud 22/22 (`s409_*.log`).

### Decisions made
- ADR-007 amended: log force from the vehicle sim's deferred-force pass, replacing the planned async tick on the log.
- Log mass 1000 kg (≈ green 50 cm × 5 m trunk); heavier logs would out-pull the braked jeep's grip. Up to the user's playtest.

### What we learned
- Chaos runs vehicle sims in a `PhysicsParallelFor`; forces on other bodies are only safe in the single-threaded `ApplyDeferredForces` pass right after it. A sleeping vehicle skips `UpdateSimulation` but still gets `ApplyDeferredForces`, so per-step forces must be cleared once applied.
- Chaos body frames have no scale: `GetRelativeTransform` on a scaled body gives a scaled offset (put the log's hook 1.9 m off and stepped to the cap). Use an unrotated world offset.
- `FPhysicsActorHandle` is `Chaos::FSingleParticlePhysicsProxy*` in 5.8 (forward-declare inside `namespace Chaos`).
- A placed actor's root component scale is the actor scale; `add_to_scene_from_asset` with scale 1 overrides the BP root's scale.
- PowerShell `Get-Content -Raw | Set-Content -Encoding utf8` mangles UTF-8 source (→ became mojibake) and adds a BOM; use the Edit tool for source.

### Open follow-ups
- S4-09 user check: winch the log off the lane, see if it slides and rolls believably.
- Next: S4-10 Phase 2 playtest.

## 2026-10-28 — Fallen tree replan (S4-09 playtest)

### Context
The user didn't accept S4-09. Driving into the log rolled it, the cable hooked to its end, and reeling pulled the jeep in with no way to hold it. They rewrote the situation in VISION.md ("Fallen tree"): a tree across a banded road, ramming doesn't move it, hook near one end, hold the handbrake, it swings open like a door. They also asked for free hook placement with a ghost band, a handbrake, controls hints and no in-car detach.

### Tasks created
- Sprint 4 section "Playtest feedback (S4-09, 2026-10-28)", S4-14..S4-19:
  - S4-14 handbrake + winch hold, no in-car detach;
  - S4-15 anchor areas;
  - S4-16 hook placement ghost;
  - S4-17 fallen log frozen until pulled;
  - S4-18 road lane + tuning;
  - S4-19 controls hints.
- S4-10 now requires them and includes the scenario.
- Plan archived as [plans/2026-10-28-fallen-tree.md](plans/2026-10-28-fallen-tree.md). The old Sprint 4 critique moved from PLAN.md to `plans/2026-10-27-sprint4-critique.md`. ROADMAP Phase 2 scope and done-when updated (the "log moves ≥ 3 m" check was replaced).

### Decisions made
- Log frozen until pulled (user, from four options), ADR-018 proposed.
- Anchors are areas, the hook is placed by aiming, and the force acts on the trunk axis: ADR-019 proposed.
- Handbrake: normal while driving; with the hook attached it adds a standstill hold up to 80 kN (user). No in-car detach (user). ADR-007 amendment proposed.
- Hints show both keys, like the prompt (user).

## 2026-10-28 — Sprint 4 planning: fallen tree critique and revision

### Context
`/plan-analyse` → `/plan-replan` → `/plan-roadmap` on the fallen tree plan, which had been converted to tasks straight away. The critique found:
- the log's scaled root would distort every new child (the S4-09 scale bug class);
- the aim state needed one owner;
- the template's pad LB handbrake and pad A brake would break the hints;
- the tip-vs-middle force ratio is about 2 for a log turning about its root end, and the root has to resist about the pull force to stay the pivot;
- the breakaway lower bound was unmeasured;
- `logGap` was undefined;
- the handbrake hold must use the same step's winch force.

### Tasks created
- S4-14..S4-19 in SPRINTS.md rewritten from revision 1 (no new IDs):
  - S4-14 adds the input cleanup and `B1Winch_HandbrakeDetach`;
  - S4-15 makes the cable block PhysicsBody;
  - S4-16 stores the aim on the winch;
  - S4-17 rebuilds the log body around an unscaled box root and adds `ShowDebug B1Log`;
  - S4-18 (3 → 4 h) gets an ordered tuning procedure, `logPivot` / `logDrift`, a defined `logGap` and `B1Log_DriveThrough`.
- ROADMAP Phase 2 done-when updated; plan re-archived at `plans/2026-10-28-fallen-tree.md`; PLAN.md cleared.

### Decisions made
- Pad handbrake LB → B; pad A no longer brakes (user).
- **Physics first (user):** the log moves by Chaos alone, with no hook-position rules or scripted motion. Hooking at the middle probably won't open a passable gap, and that's the puzzle. Tuning uses physical knobs only. Authored root-end drag or a free-log standstill term need the user's OK (deferred from the critique).
- Hold not scaled on mud (user). The user creates `PM_B1Log`; the band material is checked on MCP first, then asked for.
- ADR-018, ADR-019 and the ADR-007 amendment (all proposed) updated in place.

## 2026-10-28 — S4-14: handbrake winch hold, no in-car detach

### Context
First task of the fallen tree replan. Reeling in pulled the jeep toward any heavy anchor with no way to stop it, and detach was on Q / pad Y in the jeep.

### What was built
- `FB1VehicleSimulation::UpdateHandbrakeHold`: with the handbrake on, the hook attached and ≥ 3 wheels down, it cancels this step's cable pull. Below `HandbrakeHoldSpeed` (30 cm/s) it also cancels gravity along the ground plus a 0.1 s drift term. Capped at `HandbrakeHoldForce` (80 kN), at the COM. Winch + hold → `Mud.SetExternalForce`.
- `IMC_Vehicle_Default`: handbrake Space + pad B; pad LB handbrake, pad A brake and Q / Y detach removed. `IA_B1WinchDetach` and `AB1Jeep::WinchDetachAction` deleted.
- Debug: hold in `ShowDebug B1Winch`, `B1Winch` telemetry and the Winch window (typed fields + Save to asset).
- Rig: step `bWinchDetach`, threshold `MinTension`, metric `holdMax`. Rigs `B1Winch_HandbrakeHold`, `HandbrakeDetach`, `HandbrakeLate` (all PASS). Winch 30/30, mud 22/22, core 12/12 headless.

### Decisions made
- ADR-007 amendment built. After the user check (winch first, then B slid), the cable cancel runs at any speed and only gravity + drift are speed-gated, so the order doesn't matter.
- The hold rigs use the handbrake alone. `B1Winch_FirmPull` dropped its handbrake to stay a drag rig (36.1 → 29.2 kN).

### What we learned
- `UChaosVehicleSimulation::VehicleInputs` is only filled with network physics prediction on. Read the step's controls from `InputData.PhysicsInputs.NetworkInputs.VehicleInputs`.
- In Chaos, brake at a standstill is reverse throttle: brake + handbrake creeps back about 0.2 m/s against the hold.
- MCP `set_properties` can grow an array if the appended elements equal an existing element. Append copies first, then edit them, and zero every field (the default step has throttle 1).

### Open follow-ups
- Suspected: the mud idle check (`FB1MudSimulation::PreFriction`) reads the same empty `VehicleInputs`, which may zero the brake torque on muddy slopes. In the Sprint 4 backlog.
- User re-check of the late-B order.

## 2026-10-28 — S4-15: anchor areas, strap, pull at the trunk axis

### Context
The S4-09 check: the cable hooked a fixed point, not the trunk. S4-16 (aim) and S4-17 (log) both need a hook point anywhere along a trunk.

### What was built
- `UB1WinchAnchorComponent` area (`AreaStart` / `AreaEnd` / `TrunkRadius`, unscaled component frame) with point, closest-point and ray helpers.
- `UB1WinchComponent::AttachTo(Anchor, Alpha)`: the sim pulls at the axis point, and the cable ends on the strap surface facing the winch. A black strap ring (`MI_B1WinchStrap`) shows while attached. The cable now rests on PhysicsBody too.
- Tree area 40–160 cm, radius 25. Rig `WinchAnchorAlpha` (1/3 = old height), `StartAnchorAlpha`. ShowDebug draws the areas and the hook point.
- Winch 30/30 headless, tree numbers unchanged. User check passed ("works well").

### Decisions made
- Area points ignore component scale (the S4-09 scale bug class); an empty area = the old single point, so the log keeps working until S4-17.
- On-foot attach uses the point nearest the hand until S4-16's aim.

### What we learned
- `UCableComponent::EndLocation` is in the end component's space with scale, so it's set through `InverseTransformPosition`.
- MCP: a BP's C++ default subobject is reachable as `/Game/...BP.Default__BP_C:<Name>`. `list_properties` on a material instance is ~30k chars; go straight to `get_properties` (`Parent`, `VectorParameterValues`).

### Open follow-ups
- S4-16 hook aim, S4-17 log rebuild on an unscaled box root with its own area.

## 2026-10-28 — S4-16: hook placement ghost

### Context
S4-15 gave anchors areas; the player needed to choose the point. ADR-019 proposed a ghost ring driven by the view ray.

### What was built
- `UB1HookAimComponent` on `AB1Character`: picks the anchor and alpha from the control-rotation ray and writes them on the carried winch (`SetAimTarget`). Shows a ghost ring (`MI_B1WinchStrapGhost` on `M_B1WinchGhost`, translucent unlit, built through MCP).
- "Attach" only on the aimed anchor, `GA_B1AttachHook` uses the aim. `GetRingTransform` is shared by strap and ghost.
- Rig: `LookAtActor` / `LookAtOffset`, per-step hook height check, `hookAlpha` / `hookPoint`. `B1OnFoot_HookAim` PASS (1.200 m and the 1.600 m clamp). User check passed.

### Decisions made
- ADR-019 accepted. Alpha = closest point between the ray and the axis, always. A ray hit only decides which anchor wins (a surface hit reads high when looking down).

### What we learned
- MCP can build materials: `MaterialTools.create_material`, `add_expression` (`/Script/Engine.MaterialExpressionVectorParameter`), `connect_to_output`, `recompile(material_or_function)`, then `MaterialInstanceTools.create`. BlendMode/ShadingModel go through `ObjectTools.set_properties` on the material.
- For a PIE capture of a scripted moment, auto-run a rig (`bAutoRun`), stretch the step and capture after a matching `warmupSeconds`, then revert.

### Open follow-ups
- S4-17 fallen log (the user made `PM_B1Log`).

## 2026-10-28 — S4-17: fallen log, frozen until pulled

### Context
S4-09's log rolled when driven over and was dragged too easily. ADR-018: kinematic until the cable tension reaches breakaway, then Chaos alone.

### What was built
- `AB1WinchLog` rebuilt: an unscaled `UBoxComponent` root (`Body`), `TrunkVisual` (no collision), a `RootBall` that welds when free, and an `Anchor` area. `Log|Shape` (1000 × 50 radius, root ball 100) is applied in `OnConstruction`; `Log|Config` holds masses, breakaway and refreeze. The tick switches frozen ↔ free.
- `ShowDebug B1Log`, the Winch window "Log" section, cheat `B1LogReset`. Rig `TargetLog` plus log metrics and thresholds; rigs `B1Log_Ram` and `B1Log_RamHooked`. `B1Winch_Log` retired.
- User check: the winch on the log works. After it, the log grew to 1 m (the jeep drove over 50 cm) and the user moved it to Y −47.5 m. Both ram rigs now stop the jeep without moving the log. Winch 32/32.

### Decisions made
- Switch logs go on `LogB1Physics` (no `LogB1Winch`). A slow push for the hooked ram: at full throttle, driving off snatched the slack cable and freed the log, which is right but not a ram.

### What we learned
- `SetSimulatePhysics` keeps the proxy (kinematic flag only), and freeing welds the children.
- Renaming or removing a BP's C++ root orphans the placed instances' children (the `Anchor` sat at world coordinates). Check `get_parent_component` after a component layout change.
- PowerShell variables are case-insensitive: `$o` overwrote `$O`.

### Open follow-ups
- S4-18 tuning (masses, breakaway, friction); S4-19 hints first (user).

## 2026-10-28 — S4-19: controls hints

### Context
The user moved S4-19 ahead of S4-18. After S4-14's mapping cleanup, the hints show the real first keys.

### What was built
- `UB1ControlHintSet` data asset (`HintSet_B1Player` on `BP_B1PlayerController`) and the code-only `UB1ControlHintsWidget` in the lower-left corner, filtered by the PlayerState ASC's tags, rebuilt every 0.2 s.
- `B1InputKeyText::GetKeyText`, shared with the interaction prompt: keyboard first, short pad names, `KeysPerDevice`.
- `Vehicle.State.WinchHooked` set by the winch (`HookedTag`). PIE captures in the jeep (unhooked, hooked) and on foot; core 12/12, winch 32/32. User check passed.

### What we learned
- The template maps VR controllers and a racing wheel; `IsGamepadKey()` is true for VR keys. Filter by `FKey::GetMenuCategory()` against `EKeys::NAME_KeyboardCategory` / `NAME_MouseCategory` / `NAME_GamepadCategory`. The applied context order changes on re-possession.
- A BP CDO change made through MCP showed up in PIE only after an editor restart (the first captures had no hints).
- A modal save dialog blocks MCP too. A rig toggled for a capture and set back still leaves a dirty package, so save right after reverting.

### Open follow-ups
- S4-18 road lane and fallen-tree tuning.

## 2026-10-28 — S4-18: road lane + fallen tree tuning, taut winch cable

### Context
Last task before the Phase 2 playtest. The frozen log from S4-17 needed a road around it, physics-only tuning and the four pull rigs. The user then asked for a wider road and a cable that pulls straight under load.

### What was built
- Banded road in `Lvl_Gym_Winch` (`Gym/Road`, cube panels + cylinder posts, `M_B1RoadBand` made over MCP from a world-position Custom node). Built at 6 m, rebuilt at 8 m on the user's ask, then widened by hand to 12 m by the user (north band Y −43.1 m, straight south band Y −55.1 m). `Log_FirmLane` at X 57.5 m, Y −48.8 m.
- Rig metrics `logGap` (`LogFreeBandOffset`), `logGapT`, `logPivot`, `logDrift`, `logCross`; thresholds `MinLogGap`, `MaxLogGap`, `MaxLogGapTime`, `MaxLogPivot`, `MinLogCross`, `MaxDisplacement`; step `bSteerToTarget` / `SteerTarget` (waypoint in the rig's frame).
- Rigs `B1Log_PullNoHandbrake`, `PullEnd`, `PullMiddle`, `DriveThrough` (`B1Tests/Log`, X 42.5 m, Y −50 m).
- Log tuning: breakaway 49 kN (1.2 × the 40.6 kN a braked jeep holds), trunk 10 t, root ball 5 t, `PM_B1Log` unchanged. ADR-018 accepted.
- `UB1WinchComponent::CableTautTension` (5 kN): visual cable gravity fades with tension, so it pulls straight under load and still sags when slack. User: looks good.
- Headless on the user's road: winch 36/36, core 12/12, mud 22/22.

### Decisions made
- No root-end drag or standstill term: the root ball holds the pivot (0.38 m from the root end) and middle creep (~3 cm/s at 60 kN) is accepted physics.
- Pull rigs stand toward the root side of the road: the swing stops where the cable lines up with the trunk, so position matters more than mass.
- Roads in gyms and showcases fit two cars (~12 m).

### What we learned
- The door angle is geometry-limited: trunk mass moved it only a few degrees; road width and jeep position moved it 10°+.
- A rig that stalls at the same angle for any mass is hitting something: the first notch's corner post caught the trunk at 48°.
- Open-loop steering can't thread a 3 m gap with a 2.5 m jeep; waypoint steering can. A pursuit step that ends before its waypoint is reached sends the jeep diagonally at the next one.
- `Select-Object -First 1` stops the upstream PowerShell pipeline: it silently skipped the second `set_properties`.
- `SceneTools.save_actor` refuses some external-actor packages ("Asset does not exist") even when the file is on disk; the close dialog's Save works.
- The outliner folder list is empty after a fresh editor start; `find_actors` by name still works.

### Open follow-ups
- S4-10 Phase 2 playtest, with the fallen tree scenario on the user's road.
- Backlog: mud idle check reading empty inputs (S4-14) is still unverified.

## 2026-10-28 — S4-10: Phase 2 closed

### Context
Last Sprint 4 task: the Phase 2 playtest after the fallen tree work (S4-14..S4-19) and the taut-cable fix.

### What was built
- Agent half: all suites headless on the final build (winch 36/36 on the user's 12 m road, core 12/12, mud 22/22).
- User playtest of the full loop and sign-off. ROADMAP Phase 2 ticked and marked done.

### Open follow-ups
- Next is Phase 3 (River): replan with `/plan-new` before starting.
- Backlog: mud idle check reading empty inputs (S4-14) is still unverified.

## 2026-11-07 — Wrangler look for the player jeep

### Context
Before Phase 3 the user wanted the prototype to look better: drive a Jeep Wrangler (Sketchfab "jeep-wrangler-1997-reworked" FBX, concept shot `Concept/jeep-wrangler.png`) instead of the template buggy, keep the buggy switchable, and lay the files out like `Vehicles/OffroadCar`. The user had dumped a raw Interchange import (~100 part meshes, Lambert MIs, 4K textures) into `Vehicles/Wrangler/textures`.

### What was built
- `Scripts/VehicleImport/` (Python + numpy/pillow, see its README): binary FBX reader, scene baker, ASCII FBX writer, `convert_wrangler.py` (drops the turntable; writes `SM_Wrangler_Body`, `SM_Wrangler_Tire` = front-left tire + rim centred on the axle, `SKM_Wrangler` = axles on `OffroadCar` + hubs on `PhysWheel_*`) and `pack_textures.py` (2K D / N with green flipped / MRA).
- `/Game/Vehicles/Wrangler`: `SKM_Wrangler` (shares `SKM_Offroad_Skeleton`, mesh sockets `VisWheel_*` on `PhysWheel_*`), `SKM_Wrangler_PhysicsAsset` (root boxes for tub / cabin / spare tire, kinematic 46.3 cm spheres on the wheels, free constraints like the template), `Wrangler_AnimBP` (copy of `Offroad_AnimBP` without the ControlRig node), `SM_Wrangler_Body`, `SM_Wrangler_Tire`, `Materials/M_Wrangler_Base` (BaseColor × Tint, Normal, MRA) + `MI_Wrangler_*` (CarPaint, Bumper, MetalPart, PlasticPart, RoofTop, Sheets, Tire; Glass is a child of `M_SportsCar_Lights`), `Materials/Textures/T_Wrangler_*`. The raw import folder was deleted on the user's OK.
- `/Game/B1/Vehicles/BP_B1Jeep_Wrangler` (child of `BP_B1Jeep`) and `BP_B1WranglerWheelFront/Rear` (radius 46.3, width 36.4). `BP_B1GameMode`, `BP_B1GymGameMode` and `BP_B1PlayerController` now spawn the Wrangler.
- Size after the user's "too small" note: 19.5 cm per model unit, wheels ×1.2 about their centres, 6 cm lift, 3 cm wider per side → 4.6 m long, 2.2 m wide, 2.73 m wheelbase, 1.61 m track, body raised 13.7 cm. COM override z 65, winch (205, 0, 80), chase arm 680 / socket z 160 / camera pitch −7°, in-car camera (−25, −42, 175).
- Checked: PIE in `Lvl_Gym_Winch` (all wheels in contact, no anim/vehicle warnings), viewport captures from both sides, chase-camera capture.

### Decisions made
- Wrangler shares the Offroad skeleton and gets its own pawn BP (ADR-020).

### What we learned
- MCP `import_file` uses the legacy FbxFactory: `.fbx` / `.obj` only, and it won't overwrite an existing asset (delete, re-import, reassign).
- A hand-written ASCII FBX skeleton needs `DefaultAttributeIndex` on every Model, or UE says "'X' has no bones".
- Importing against an existing skeleton fills the mesh with all the skeleton's bones at the skeleton's ref pose; mesh sockets resolve before bones, so same-named sockets redirect attachments.
- No MCP path sets an AnimBP's `TargetSkeleton`, and folder `duplicate` doesn't remap internal references.
- `CaptureEditorImage` shows the content browser when it's the front tab; PIE in a floating window captures the game view.

### Open follow-ups
- User playtest: handling with the new size (track, wheel radius, COM), steering/spin visuals, normal map direction, fender clearance at full suspension raise (20 cm).
- Body is ~360k tris and each wheel ~52k; no LODs yet (prototype first, ADR-011 budget later).
- `Config/DefaultEditor.ini` picked up an editor preview-profile block; don't commit it.

## 2026-11-07 — Sprint 5 planning (Phase V Visuals)

### Context
Before Phase 3 (River) the user wants the prototype to look better in two ways:
- Mud spray: smaller drops of mixed sizes that feel like mud, with `Concept/mud-car-drive.mp4` as the reference. `mud-car-splash.mp4` is aspirational only.
- Mud building up on the Wrangler: clean at the start (`Concept/jeep-wrangler.png`), tires first, then more and more spots on the body, capped at `Concept/jeep_wrangler_mud_reference.jpg`.

The plan went through `/plan-new` → `/plan-analyse` (round 1) → `/plan-replan` (revision 1). It's archived at `plans/2026-11-07-visuals.md`. The new Phase V sits before Phase 3 in ROADMAP.md and takes over Phase 1's "dirt mask on the jeep" stretch item.

### Tasks created
- S5-01 through S5-19 in SPRINTS.md:
  - spikes S5-01..03;
  - dirt look S5-04..08, then checkpoint 1 (stills, S5-09);
  - dirt accumulation S5-10..12;
  - spray S5-13..17;
  - docs S5-18, then checkpoint 2 (Swamp playtest, S5-19).

### Decisions made
- **ADR-021 (user choice):** mud on the vehicle is a material layer, not decals.
  - `MF_B1MudDirt` (local-space triplanar, region-weighted reveal), driven by Custom Primitive Data (slot 0 = own dirt, slots 1–4 = wheels).
  - Written by the portable `UB1MudDirtComponent` onto `B1MudDirt`-tagged components, which are matched to wheels by position.
  - Dirt only grows, capped at 1, and is authored in seconds-to-full.
  - Dirt and the spray rate share the `B1MudThrow` intensity, but dirt doesn't read the spray.
- **Textures:** generated by a script (`Scripts/MudTextures`), not from Fab (user choice). Fab textures can replace them later behind the same material inputs.
- **Phase naming:** "Phase V", so the phase numbers don't shift (user choice).
- **Look-first order:** the user signs off stills from a photo booth (`PreviewDirt`, two concept camera angles) before any accumulation logic is built.

### What we learned
- The live MCP has `NiagaraToolset_System` (system/emitter/module editing) and `TextureTools.import_file` (PNG → Texture2D). Both were checked 2026-11-07.
- `MI_Wrangler_Glass` is parented to the template's `M_SportsCar_Lights`, not `M_Wrangler_Base`, so the glass needs its own Wrangler material to get dirt (S5-06).
- No ffmpeg on the machine. The reference videos were read through OpenCV installed into the session scratchpad (`pip --target`).

## 2026-11-08 — Phase V: dirt look up to checkpoint 1

### Context
Sprint 5 started with `/work`. It ran the look-first path: spikes, the texture generator, the dirt material, the component and the photo booth, ending at the user's checkpoint 1 on stills. The Niagara spike (S5-02) was left for later, since it only feeds the spray.

### What was built
- **S5-01:** tags and CPD work on `BP_B1Jeep_Wrangler`'s inherited C++ components through `ObjectTools` (`…Default__BP_B1Jeep_Wrangler_C:Tire Front Left`). `Chassis` and the four tires carry `B1MudDirt`.
- **S5-03:** body-space numbers (arch centres = wheel centres ±136.5 / ±80.4 / 46.3 cm), from `wrangler_info.json` plus `get_bounds`.
- **S5-04:** `Scripts/MudTextures/gen_mud_textures.py`: three grayscale masks and `SM_B1MudClump` (48 tris), imported.
- **S5-05/06:** `MF_B1MudDirt`, wired into `M_Wrangler_Base`, the new `M_Wrangler_Glass` and `MI_Wrangler_Rim`.
- **S5-07:** `UB1MudDirtComponent` on `AB1Jeep`; `UB1MudVehicleComponent::GetWheelLocation`.
- **S5-08:** photo booth + `photobooth.py` / `stage_sheet.py`.
- **S5-09:** two rounds. User feedback in round 1:
  - effect scaled to 0.8 (`DirtScale`);
  - a clean cheat (S5-11);
  - a fast tire mud layer that coats on mud contact, thickens, sheds and dries (S5-20, material part done).
  - Signed off on round 2.
- Headless after the C++ change: core 12/12, mud 22/22, numbers unchanged.

### Decisions made
- ADR-021 amended: tire CPD slots 1–2 (coat, wetness), which can decrease; `DirtScale`; the clean cheat; rims need a tire material.
- Editor scalability stays at the user's medium / high shadows for judging Phase V visuals.
- The booth jeep faces yaw 115. The two concept angles look at opposite sides, so at any other yaw one of them is backlit.

### What we learned
- `MaterialInstanceTools.set_parent` doesn't mark the package dirty: the reparent isn't saved. Set any parameter afterwards, then save.
- CPD in a material is a `ScalarParameter` with `bUseCustomPrimitiveData`, not `PerInstanceCustomData`.
- Custom node inputs through MCP: set element 0, append copies, then rename (the same array rule as other struct arrays).
- `CaptureViewport` returns ~2 MB of base64 per image. More than ~2 images in one `ProgrammaticToolset` reply closed the MCP connection. Shoot one stage (2 images) per call.
- Right after a material recompile, captures can show translucent materials missing while shaders compile.
- Glass with metallic 1 / specular 1 turns milky white when mud roughens it; keep glass smooth under mud.
- Dark mud on black rubber is invisible: tires need a lighter mud tone.
- Python with numpy/pillow isn't installed system-wide; use a scratchpad venv.

### Open follow-ups
- S5-02 Niagara spike (before S5-14).
- S5-10 accumulation, then S5-20 tire layer runtime, S5-11 debug + `B1DirtClean`, S5-12 rigs.
- The photo-booth stills live in the session scratchpad only.

## 2026-11-08 — Phase V: dirt accumulation section

### Context
`/work` on the whole "Dirt accumulation" section of Sprint 5 (S5-10, S5-11, S5-12, S5-20), with the user checks batched at the end. Start state: the dirt look signed off at checkpoint 1, the component writing CPD but with nothing accumulating.

### What was built
- **S5-10:** `Mud/B1MudThrow` (intensity from sink, tread speed, slip; 0 off mud). New mud sim output `TreadSpeed` → `UB1MudVehicleComponent::GetWheelTreadSpeed`. `UB1MudDirtComponent` accumulates wheel and body dirt (belly term in deep mud) and logs each new tenth.
- **S5-11:** Mud window "Dirt" section (bars, Clean / Max, preview, typed tunables, Save to the pawn BP's component archetype), `SetDirt` / `Clean`, cheats `B1Dirt <v>` and `B1DirtClean`, `dirt=` on the `B1Mud` telemetry line, a dirt line in `ShowDebug B1Mud`.
- **S5-12:** rig `StartDirt`, `dirtW= dirtBody= dirtLast= dirtTargets=`, dirt thresholds, step option `ShuttleDistance`. Rigs `B1Dirt_Firm`, `_Medium`, `_Drive90`, `_Cap` in `Lvl_Gym_Mud` `B1Tests/Dirt`.
- **S5-20:** tire coat + wetness per wheel (tire CPD slots 1–2), tunables under `Mud|Dirt|Tire`, rig fields `tireCoat= coatPeak= coatDrop= coatDist=`, rig `B1Dirt_TireCoat`.
- Headless: mud 27/27, core 12/12.

### Decisions made
- Seconds-to-full tuned to the rigs, not the thresholds: wheel 20 → 15 s, body 80 → 45 s (body ≈ 1 after ~87 s of continuous medium mud).
- Tire coat grows and sheds by tread distance (|ω·r|), so spinning coats and sheds faster than rolling.
- "Save to asset" for dirt leaves `PreviewDirt` out: a saved preview would show on every jeep.
- No new ADR; ADR-021's CPD table now lists the tire slots.

### What we learned
- Open-loop forward/reverse legs can't keep a jeep in a 20 m patch for 90 s (4/4 s drifted +54 m, 3/4 s drifted out backwards in 15 s). A closed-loop shuttle on position does.
- Full throttle from rest in the medium lane gets out of the patch over 30 s; use the deep lane for "stays in mud" rigs.
- A `LogHttp` timeout logged as an error can fail a headless functional test whose own line says PASS.
- `BuildEditor.ps1` can't close an editor with unsaved packages from the user's own session; they had to handle the dialog.

### Open follow-ups
- User checks for S5-11 (bars, Clean / Max / `B1Dirt 0.5`, Save across a restart) and S5-20 (tires coat and darken fast, lighten and thin on firm).
- Spray section next: S5-13 (rate from `B1MudThrow`), S5-02 spike, S5-14..17.

## 2026-11-08 — Phase V: spray rework (S5-13..S5-16)

### Context
`/work` on S5-13..S5-16 with the user checks at the end. Start state: dirt accumulation done; the spray was still the S2 sprite emitter, driven by slip only. S5-14 needed the S5-02 Niagara spike, which was folded into it.

### What was built
- **S5-13:** spray rate = `B1MudThrow` intensity × `MaxSpawnRate`, × `RollingRateFactor` (0.4) below `MinSlipSpeed`; `SpawnRatePerSlip` removed. `User.Intensity`, `GetWheelIntensity`, telemetry `spray=rate/intensity`. A rolling wheel throws backward along its travel.
- **S5-16:** Mud window "Spray" section (rate bars, typed `Mud|Spray` fields, Save to the pawn BP archetype).
- **S5-02 + S5-14:** `NS_B1MudSpray` `Spray` emitter now renders `SM_B1MudClump` with new `M_B1MudClump`: skewed 2–12 cm sizes scaled by intensity, random orientation, tumble that stops at rest, scale-out, no-bounce depth-buffer collision.
- **S5-15:** `Droplets` (GPU velocity-aligned sprites, 2× rate) and `Haze` (CPU, ≤ 8/s, new `M_B1MudHaze`) emitters.
- Fix: `NetImGuiLaunch` crashed the editor (UE 5.8 `CreateProc` asserts on null Parms); now passes `TEXT("")`.
- Fix: unity-build clash of `CmPerM` between `B1MudThrow.cpp` and `B1VehicleSimulation.cpp` → `B1MudThrowPrivate::CmPerM`.
- Headless: mud 27/27, core 12/12; with `b1.Mud.Spray=0` dirt is unchanged.

### Decisions made
- Spray rate is a pure function of throw intensity (one cap, one rolling factor) instead of slip excess, so what's thrown matches what sticks (plan revision 1).
- Clump tumble axis/rate come from the velocity (no per-particle random state), so clumps stop turning when they rest.
- No new ADR.

### What we learned
- The Niagara toolset can build the whole spray: renderers, modules, enum/linked/HLSL inputs; HLSL expressions accept `rand(1.0f)`, `User.*`, `Particles.*`. Tool arg names differ per call (`renderer`, `rendererToRemove`, `emitter`, `emitterRef`), and every stack ref needs all six fields.
- `GetEmitterTopology` / `GetEmitterInputValues` are ~60k chars: filter them inside `execute_tool_script`.
- Capturing a rig: temporarily set `bAutoRun` (and the Wrangler class) on an existing rig, Simulate with the camera set first, `CaptureEditorImage`, then revert in the same script.
- New `.cpp`/header edits can regroup unity files and expose clashing anonymous-namespace names.
- If unreal-mcp failed at session start, ask the user to reconnect it (`/mcp`) instead of raw JSON-RPC (user preference).

### Open follow-ups
- Spray look tuning (clumps a bit grey in sun, droplets tiny, haze faint): S5-17 capture rig + reference sheet, then checkpoint 2.
- `stat Niagara` counts (user-side).
- S5-17, S5-18 docs pass, S5-19 checkpoint 2.

## 2026-11-08 — Phase V close-out: spray capture rig, docs pass, checkpoint 2 start

### Context
`/work` on the last Sprint 5 tasks. Start state: dirt and spray features done (S5-01..16, S5-20); S5-17 capture rig, S5-18 docs pass and S5-19 checkpoint 2 left. The user started the Swamp playtest and asked how to tune the spray and tire colours, then asked for two new spray controls.

### What was built
- **S5-17:** rig `B1Spray_Medium` (`Lvl_Gym_Mud`, `B1Tests/Spray`, X 15 / Y 29 m, Wrangler, 5 s full throttle, no thresholds). `Scripts/MudTextures/spray_sheet.py` + README "Spray sheet" section: decodes Simulate `CaptureEditorImage` results, crops the 3D view, sheet next to `mud-car-drive.mp4` frames. Camera (3300, 2450, 110, −3, 158).
- **S5-18:** MIGRATION.md gaps filled (spray emitters, debug, dirt/spray rigs); ADR-021 "As built" note.
- **S5-19 (agent part):** headless core 12/12, mud 28/28, winch 36/36. Editor opened for the user's playtest.
- **Playtest request:** `UB1MudSprayComponent` gets `SpawnBackOffset`, `SpawnLift` (replace the hard-coded 20 cm `SurfaceLift`), `SpraySpread`, `SpraySpeedSpread`; new `NS_B1MudSpray` user params `SpraySpread`/`SpeedSpread` drive the Spray and Droplets cone angle and per-particle speed. Mud window Spray section: typed fields + Save to asset for all four. Full rebuild OK; system compiles clean.
- Tuning answers given (no code): spray colour = `SprayColor` on the `PM_B1Mud_*` assets (also tints haze); tire mud colour = `DirtMudColor` / `DirtTireWetDarken` overrides on `MI_Wrangler_Tire`/`MI_Wrangler_Rim` (tire mud is 1.6× in the Custom node); haze colour = the Haze Initialize Particle colour expression; droplet/clump size = the Sprite Size / Mesh Scale expressions.

### Decisions made
- `B1Spray_Medium` is saved with `bAutoRun` off; capture scripts toggle it. Saved on, it would also start in headless suite runs and drive into the medium lane next to other rigs.
- Spray randomness lives in the component (passed as user params), not hard-coded in the Niagara asset, so it's tunable from the Mud window. Haze keeps its own cone and speed.
- No new ADR.

### What we learned
- The clump emitter had a fixed 25° cone and no speed variation: that's why the spray read as an even fountain.
- Comparison vs the reference: our spray is a low flat trail at wheel height; the clip's is a rooster tail above the roof with dark clumps and a big tan haze. The Wrangler reaches 13 m/s within 5 s in medium mud, so captures at 2–3 s show mostly rolling throw.
- `CaptureEditorImage` is only ~680×440 of 3D view inside the editor shot; after an editor restart, restored asset-editor tabs sit in front and the capture shows them instead of the level.
- `SceneTools.load_level` refused `/Game/B1/Gym/Lvl_Gym_Swamp` ("level does not exist") although the `.umap` is on disk; `StartEditor.ps1 -Map` loads it fine. Not investigated.
- Inline Python in PowerShell `-c @'...'@` loses double quotes; write the script to a file.

### Open follow-ups
- S5-19: user Swamp playtest with the new spray controls, then sign-off → ROADMAP Phase V.
- Optional `DirtTireMudBoost` parameter (replace the 1.6 in `MF_B1MudDirt`) if the tire MI overrides aren't enough.
- Spray look not yet re-captured after the spread change (capture blocked by the open Niagara tab).
- The user's colour edits (`PM_B1Mud_*`, `MI_Wrangler_Tire`) are on disk, uncommitted.

## 2026-11-08 — Sprint 5 addition: mud on the lens

### Context
Before signing off Phase V (checkpoint 2, S5-19), the user asked for one more effect: mud on the camera lens. Big spray shots from the tires dirty the chase camera, the mud builds up like the tire coat and fades after a while, capped at `Concept/mud-on-the-screen.jpg`. The trigger should be cheap, tied to the large Niagara throw, not real drops hitting the camera. Planned through `/plan-new` → `/plan-analyse` → `/plan-replan` → `/plan-roadmap`; archived at `plans/2026-11-08-mud-lens.md`.

### Tasks created
- S5-21 through S5-28 in SPRINTS.md (section "Lens mud"): component core, texture generator, material, preview stills, lens checkpoint, debug, rigs, docs. S5-19 now requires them.

### Decisions made
- ADR-022: lens mud is a post-process MID on `B1MudLens`-tagged cameras, driven by a portable `UB1MudLensComponent` reading the spray's launch data; 4 splash slots with hold + fade; `LensSecondsToFull`.
- Critique round 1 found the first-draft ballistic reach gate would never fire: the last PIE log shows medium wheelspin at slip 4.2–5.0 m/s → launch ~510 cm/s, which can't reach a camera ~5.5 m back. Replaced by a launch-speed band (350 → 500 cm/s).
- User feedback: wide aim (full to 75°, zero at 110°), so the chase camera gets splashed more often than not.
- No body occlusion trace; in-cabin camera out of scope (windshield glass dirt covers it).
- Look loop on a bound, saved-disabled preview post-process volume in `Lvl_Gym_Core` (editor `CaptureViewport`), plus one PIE capture to confirm the camera path.

## 2026-11-08 — Lens mud: component, textures, material, look checkpoint rounds 1–3

### Context
`/work` on Sprint 5's lens tasks (ADR-022). Starting state: S5-21..S5-28 planned, nothing built. The user asked to continue through to the next user check (S5-25, the lens look) with few test runs, then gave three rounds of look feedback.

### What was built
- **S5-21:** `UB1MudSprayComponent` read-only `GetNumWheels` / `GetWheelSprayOrigin` / `GetWheelSprayVelocity`. New portable `Mud/B1MudLensComponent` on `AB1Jeep` as `MudLens` (exposure, seeded splash slots, hold/fade, MID on `B1MudLens`-tagged cameras, preview/debug calls, CVar `b1.Mud.Lens`). `AB1Jeep` tags `BackCamera`. `UB1MudSettings::LensMaterial` (ini → `MI_B1LensMud`). Headless mud 28/28: lens lines on medium spin (`B1Spray_Medium` up to 0.52, buggy DigInMedium 0.32) and deep spin (cap rig 1.0), none on firm or rolling rigs.
- **S5-22:** `Scripts/MudTextures/gen_lens_textures.py` → `T_B1LensDrops` (2048×1024, normal xy, 1 − radius, reveal id).
- **S5-23:** `M_B1LensMud` (post-process after tonemapping, one Custom node = `M_B1LensMud.hlsl`), `MI_B1LensMud`, `MI_B1LensMud_Preview`.
- **S5-24:** `B1PhotoBooth_LensPreview` volume in `Lvl_Gym_Core`, `photobooth.py` lens mode, `stage_sheet.py --lens`. PIE check: mud on the back camera, HUD clean on top. Core 12/12 with the volume placed.
- **S5-25 rounds 1–3** (signed off on round 3): specks removed; drops translucent, blurred, irregular (wobble + satellite blobs), brown towards lower edges, varied per drop; more and larger at the bottom; medium drops first and a front-loaded reveal so 0.25 is visible. Latest sheet `Saved/MudTextures/lens/stills4/lens_sheet.png`.

### Decisions made
- Drops only, no specks (user: the concept's dots were an image artifact). Recorded as an interim "As built" note on ADR-022.
- Lens look params live as defaults on `M_B1LensMud` (both MIs inherit), except `LensDropFill`, which `MI_B1LensMud` also overrides (5).
- `LensMaterial` was left unset in the ini until the MI existed (a missing asset path would warn on every load).
- Generated lens/dirt texture output moved from the session scratchpad to `Saved/MudTextures/` (git-ignored), because the scratchpad disappeared mid-session.

### What we learned
- Custom node `Inputs` over MCP grow only by appending an element **as read back** (full struct); a bare `{inputName}` copy is "ambiguous". Replacing the whole node (delete, add, rewire) is simpler than shrinking it.
- `SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(uv, 14), 14), 14, true)` works in a 5.8 post-process Custom node when a `SceneTexture` PostProcessInput0 node is also wired in.
- A `PostProcessVolume` spawned with `SceneTools.add_to_scene_from_class` has a brush (bound volumes work); `Brush` isn't readable via `get_properties`.
- `StartPIE` takes `{"options": {"bSimulate", "playMode"}}`; `CaptureEditorImage` returns `{data, mimeType}`; a floating PIE window captures the game view.
- A capture right after a material recompile can show a transient (a brown blurred frame); wait or re-shoot.
- Zooming a screen-space tile to enlarge drops also spreads them out: density has to beat zoom² to get *more* drops there.
- The lens log line repeated at the cap (fade between splashes crossed a tenth); one tenth of slack fixed it (51 → 10 lines).
- Also seen: the 50 % shuttle and other rolling rigs never splash with the start band (good sign for S5-27).

### Open follow-ups
- S5-25 signed off by the user on the round 3 sheet (marked after /save).
- S5-26 (lens debug section, cheats, telemetry, ShowDebug), S5-27 (lens rig metrics, measure launch speeds, thresholds), S5-28 (docs), then S5-19 checkpoint 2.
- An unknown save-changes dialog blocked `StopEditor.ps1` once (none of the lens assets were dirty); the core run went with the editor open.
- `Lvl_Gym_Core/7/J4/…` (booth Wrangler) got re-saved by a save-all after a reverted BP CDO preview edit; same values, check the diff before committing if in doubt.

## 2026-11-08 — Lens debug, lens rigs, Phase V sign-off

### Context
`/work` after the lens look sign-off (S5-25). Starting state: lens component, textures, material and stills done; S5-26 (debug), S5-27 (rigs), S5-28 (docs) and the S5-19 checkpoint open.

### What was built
- **S5-26:** Mud window "Lens" section (`DrawLens`: bar, slot table, per-wheel exposure table, Splash / Clean / Max, typed preview and 10 `Mud|Lens` floats, Save to the pawn BP archetype). Cheats `B1Lens <v>` / `B1LensSplash`. Telemetry `lens=d/exp`; ShowDebug `lens d exp slots`. User checked it in PIE: all works.
- **S5-27:** `AB1GymTestRig` lens metrics (`StartLensDirt`, six thresholds, `lensPeak lensEnd lensSplashes lensExpPeak throwPeak lensCams`). Five rigs in `Lvl_Gym_Mud` `B1Tests/Lens`: Firm, Roll, Spin, Cap, Fade. Headless mud 33/33, core 12/12, winch 36/36.
- **S5-28:** ADR-022 final "As built" note; MIGRATION.md §7 (lens debug, cheats) and §9 (lens rigs).
- **S5-19:** user signed off the Swamp playtest; Phase V ticked in ROADMAP.

### Decisions made
- `LensMinThrowSpeed` / `LensFullThrowSpeed` stay 350 / 500 cm/s: measured rolling throw ≤ 291, wheelspin ≥ 547 (ADR-022 "As built").
- `throwPeak` is read from the spray, not from the lens's per-wheel exposure, so it's measured even without an active camera.

### What we learned
- Wrangler medium wheelspin throws at 675 cm/s (the plan guessed ~510); deep 900; buggy medium 547–555.
- An unpossessed rig jeep's `BackCamera` is active (`lensCams=1`), so lens rigs need no controller.
- `SceneTools.save_actor` keeps failing ("Asset does not exist") on a WP actor created this session, even after `save_assets([])` wrote it and the editor restarted; `save_assets([])` works.
- `RunGymTests.ps1` for core + winch back to back exceeds the 10 min tool timeout; run them as separate calls or in the background.

### Open follow-ups
- A save-changes dialog for packages not touched by the agent blocked `BuildEditor.ps1` again; the user closed the editor. Source still unknown.
- Phase V done. Next: plan the next phase (Phase 3 River per ROADMAP) with `/plan-new`.
