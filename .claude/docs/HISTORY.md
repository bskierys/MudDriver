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
