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
