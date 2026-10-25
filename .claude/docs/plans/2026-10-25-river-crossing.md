# PLAN — River Crossing prototype (Huta → Bagnisko)

## Problem

VISION.md describes one full situation: drive from Huta to Bagnisko, find the bridge over the Rwąca collapsed, take the muddy, bumpy forest path, and cross the river at a ford. Either you fight the current in deep water, or you get stuck in mud in the shallows and get out with the winch. Along the way you can clear obstacles on foot: winch a log off the path, cut branches with a machete.

Today the project is the UE vehicle template plus a NetImGui menu and a low-end render budget (T0.1, done). None of the mechanics exist.

## Design decisions

| Topic | Decision | ADR |
|---|---|---|
| Mud depth | Dynamic per-wheel sinking from wheelspin | ADR-001 |
| Mud params | Custom physical material on the mud surface mesh | ADR-002 (amended) |
| Mud collision | Wheel-trace-only mud surface + hard bottom; sinking via suspension trace offset | ADR-012 |
| Physics forces | Inside the Chaos vehicle simulation; stepping mode decided in T0.4 | ADR-003 (amended) |
| GAS | Single ASC on PlayerState; the jeep holds a weak ref to it | ADR-004 |
| Damage | Single `Condition` attribute + penalty effects | ADR-005 |
| Obstacles | Lightweight component + data asset, no ASC | ADR-006 |
| Winch | Uncharted-style: carry the hook on foot, fixed max cable length, capped tension force, analog pull from inside the jeep only | ADR-007 |
| Mud trails | Visual only (revisit after the mud gym) | ADR-008 |
| Workflow | Test gyms first, scenario level last | ADR-009 |
| Fuel | `Fuel` attribute, consumed by engine work. Refuel via cheat/ImGui only for now | ADR-010 |
| Rendering | No Lumen/RT/VSM/distance fields/Substrate | ADR-011 |
| Fail state | None. At 0 condition the jeep keeps driving with max penalties | ADR-005 |
| On foot | Walk, enter/exit the jeep, winch hook (incl. pulling logs), machete for branches | — |
| Scouting tool | None. Obstacle and mud parameters are shown through `ShowDebug` only | — |

Confirmed assumptions: single-player only (no replication work), third-person chase camera while driving, no swimming (deep water blocks or pushes the player on foot), no day/night.

## Scope

Each phase has a **minimum** (must ship for the phase to close) and a **stretch** (can slip to a polish pass before Phase 5). Anything not listed in either is out of scope until a replan.

| Phase | Minimum | Stretch |
|---|---|---|
| 0 Foundation | Render budget, B1 rename + log categories, wheel debug, simulation subclass + stepping decision, mud gym + test rig | — |
| 1 Mud | Mud material, per-wheel sinking (ADR-012), traction/resistance curves, belly drag, simple Niagara spray, visual trails, Mud debug | Spray colour/size by surface, dirt mask on the jeep body, fake trail depth (parallax/WPO), character slowed in mud |
| GAS + fuel | B1 GameMode/Controller/PlayerState, single ASC, Condition + Fuel attributes, fuel consumption, Vehicle Status debug | Fuel gauge in the vehicle UI |
| 2 Damage and vegetation | Impact damage from chassis hits, condition penalties, large tree, small tree (topples), bush (drag + scratch), breakable branch, physics log, `ShowDebug B1Obstacles` | Bush/branch WPO bending, branch break VFX, hit sounds |
| 3 River | Box river volume, hull-point side force, traction loss, deep and shallow-mud ford in gym, flow debug | Spline river, splash VFX, water surface shading |
| 4 On foot and winch | `AB1Character`, enter/exit, carry/attach hook to tree, in-car pull, log pull, machete, Winch debug | Cable sag visuals, hook-in-hand mesh, winch/machete animations |
| 5 Scenario | Greybox route Huta → collapsed bridge → forest path → ford (two routes) → Bagnisko, built from modular meshes + existing landscape, arrival trigger | Vegetation dressing, landmarks for Huta and Bagnisko |

Every phase ships its debug: a NetImGui window, a `ShowDebug` category where it makes sense, cheats, and gym tests (see Testing).

### Out of scope

- Fail state, checkpoints, save/load
- Physics ruts in mud (trails are visual only)
- Per-part damage, repair
- Refuelling interaction (cheat/ImGui only)
- Scouting tool for the player
- Multiplayer / replication
- Real water simulation, swimming
- Final art, audio beyond placeholders

## Approach

**Vehicle.** `AB1Jeep` derives from `AMudDriverOffroadCar`. To swap in `UB1VehicleMovementComponent`, the template constructors `AMudDriverPawn` and `AMudDriverOffroadCar` change to take `FObjectInitializer`. That's a small, deliberate template edit; everything else in the template stays. `UB1VehicleMovementComponent::CreatePhysicsVehicle` installs `FB1VehicleSimulation : UChaosWheeledVehicleSimulation`, which applies mud, water and winch forces on the physics thread (ADR-003). The game thread sends only small inputs: active river volume params and winch anchor/length/pull. T0.4 picks the physics stepping: async physics with a fixed dt (recommended) or substepping.

**Mud (ADR-001, 002, 012).**
- A mud patch is a surface mesh with the `B1MudSurface` collision profile and a `UB1PhysicalMaterial_Mud`. Only wheel traces hit it. Under it sits the hard bottom (landscape or mesh) at `MaxSinkDepth`.
- Per wheel, every sim step:
  - `SinkDepth += SinkRate * Slip * dt` while on mud. `SinkDepth -= RiseRate * dt` when rolling with traction. Clamped to `MaxSinkDepth`.
  - `PerformSuspensionTraces` is overridden: on mud, the hit is offset down by `SinkDepth`, so the jeep sits in the mud with an unchanged wheel radius.
  - Friction multiplier and rolling resistance come from curves vs `SinkDepth / MaxSinkDepth`.
  - Belly drag is analytic once the average sink depth goes past ground clearance.
- "Drive slowly with intent" comes from slip feeding depth: full throttle means high slip, so the jeep digs in.

**GAS (ADR-004, 005, 010).**
- `AB1GameMode` sets `AB1PlayerState` (owns the single ASC), `AB1PlayerController` and the default pawn.
- `AB1PlayerController` replaces the template controller's vehicle-only assumptions: `OnPossess` works for any pawn, and the vehicle UI only shows while driving.
- On possession, `InitAbilityActorInfo(PlayerState, NewPawn)` swaps the avatar. The jeep keeps a weak ref to the ASC and implements `IAbilitySystemInterface`.
- Attributes: `UB1VehicleAttributeSet { Condition, MaxCondition, Fuel, MaxFuel }`.
- Damage: `GE_B1Damage_Impact` / `GE_B1Damage_Scratch` with SetByCaller. Condition bands apply `GE_B1ConditionPenalty`, which the jeep pushes into torque/steering.
- Fuel: consumption = `IdleRate + WorkRate * NormalizedEnginePower` (engine torque × RPM vs max power), with rates in `UB1VehicleTuningData`. It's applied as a batched `GE_B1FuelDrain` about 4×/s. At 0 fuel: `Vehicle.State.OutOfFuel` and no engine torque.
- Tags live in `B1GameplayTags` (`State.Avatar.OnFoot`, `State.Avatar.InVehicle`, `Vehicle.State.InMud`, `Vehicle.State.InWater`, `Vehicle.State.Winching`, `Vehicle.State.OutOfFuel`, `Ability.Vehicle.Enter`, …).

**Obstacles (ADR-006).** `UB1ObstacleComponent` + `UB1ObstacleData` (integrity, break speed, damage-to-car per km/h, drag):
- *Bush*: overlap. Drag + scratch damage above a speed threshold. Bending (stretch) is material WPO driven by the jeep position in a material parameter collection.
- *Small tree*: static until the hit impulse exceeds the threshold, then it switches to simulate and topples.
- *Large tree*: hard static. Impact damage scales with normal speed. Can be a winch anchor.
- *Branch*: breakable. The car breaks it and takes damage scaled by speed. The machete breaks it with no damage to the car.
- *Log*: simulated physics actor with a winch anchor.

**Water.** `AB1RiverVolume` (box): flow direction, flow speed, depth. The force at hull sample points scales with submerged fraction × (flow velocity − point velocity), mostly sideways. Wheel friction scales down with submersion (fake buoyancy). Deep ford = strong current. Shallow ford = weak current over a mud bed (ADR-012 layering).

**On foot / winch (ADR-007).** `AB1Character` (first-person, simple). Enter/exit is a GAS ability that swaps possession. `UB1WinchComponent` on the jeep front, `UB1WinchAnchorComponent` on trees and logs. `GA_B1CarryWinchHook` (on foot, the cable limits distance), `GA_B1WinchControl` (in car, analog pull), `GA_B1Machete` (trace in front, damage to obstacle integrity). The tension is a capped, one-sided spring-damper. A simulating anchor gets the opposite force, which is how the log gets pulled. Cable visuals use `UCableComponent`.

**Debug, per feature.**
- NetImGui windows: Vehicle/Wheels, Mud, Vehicle Status (condition, fuel), River, Winch, plus live editing of the data-asset params.
- `ShowDebug B1Vehicle`, `ShowDebug B1Mud`, `ShowDebug B1Obstacles` (text over obstacles: type, integrity, break speed, damage).
- Cheats: `B1Unstuck`, `B1SetCondition`, `B1DamageVehicle`, `B1AddFuel`, `B1Teleport <gym point>`, `B1WinchAttachNearest`.
- Each debug window also writes a compact telemetry line to its log category, so ImGui and the logs show the same numbers.

## Testing

Two verifiers, with different jobs.

**Agent (Claude): numbers and regressions.** The MCP can't run console commands or drive the jeep, so everything the agent verifies goes through:
- **`AB1GymTestRig : AFunctionalTest`**, placed in gym maps. It holds a scripted input sequence (`{Duration, Throttle, Steer, Brake}` steps), the measured metrics (distance, max sink depth per wheel, time-to-stuck, fuel used, condition lost) and pass thresholds. It logs one `LogB1Test` result line.
  - **In the running editor**, the agent sets `bAutoRun` via `set_properties`, runs `StartPIE` and reads the result with `LogsToolset`.
  - **Headless**, from PowerShell: `UnrealEditor-Cmd.exe MudDriver.uproject <gym map> -ExecCmds="Automation RunTests Project.Functional" -unattended -nullrhi -log`, with the editor closed.
- **Build + viewport captures** for visual sanity (spray, trails, cable).

**User (you): feel and acceptance.**
- Each task has a **User check**: a short hands-on test (a minute or two) with the tool to use (ImGui window, cheat, gym spot). It answers "does this feel right?", which numbers can't.
- Each phase closes with a **playtest session** in its gym against the phase checklist (Validation below). Tuning you change live in ImGui gets written back into the data assets by the agent.
- A phase is done when the agent tests pass **and** you're satisfied. If they disagree (tests pass, feel is off), the feel wins and the thresholds get updated.
- Performance: you check `stat unit` on the target machine at each phase playtest.

## Phases and tasks

### Sprint 1 — Foundation + mud core

| # | Task | Done when (agent) | User check |
|---|---|---|---|
| T0.1 ✅ | **Low-end render budget (ADR-011).** | Config applied | Done 2026-10-25: user restarted the editor and is happy with the result |
| T0.2 | **Housekeeping.** Log categories `LogB1Vehicle`, `LogB1Physics`, `LogB1Test`. Rename `UMudImGuiSubsystem` → `UB1ImGuiSubsystem`, `UMudCheatManager` → `UB1CheatManager`, `MudImGui` → `B1ImGui` (files too). | Builds; no `Mud*` custom classes left | NetImGui menu and `NetImGuiLaunch` still work |
| T0.3 | **Wheel debug.** Per-wheel table in NetImGui (contact, phys material, slip, skid, suspension length, drive/brake torque) + telemetry line. `ShowDebug B1Vehicle` text over the jeep. | Builds; telemetry line appears in the log during PIE | Drive on `Slippery` vs `NonSlippery`: the table makes sense and is readable |
| T0.4 | **Simulation spike (ADR-003).** Template constructors → `FObjectInitializer`. `AB1Jeep` / `BP_B1Jeep` with `UB1VehicleMovementComponent` + `FB1VehicleSimulation`. Pick the physics stepping (async fixed dt vs substepping). Test forces: a constant sideways push and a stiff spring pulling the jeep toward an anchor point (winch stand-in), toggled by cheat/rig. | Rig test: the spring settles without oscillation growth at 20 and 120 fps (`t.MaxFPS`). Sideways drift differs by < 10 % between the two. ADR-003 → accepted or fallback | Toggle the spring and drive against it: the jeep doesn't jitter or explode, and normal handling feels unchanged |
| T0.6 | **Mud gym + test rig.** `Lvl_Gym_Mud`: firm strip, shallow/medium/deep mud patches built per ADR-012 (surface mesh + hard bottom), a mud pit with a slope out, spawn points. `AB1GymTestRig` (AFunctionalTest) with a smoke test: drive 20 m on firm ground. | Smoke test passes in PIE and headless | Walk through the gym: layout is good for testing, and patches are readable at a glance |
| T1.1 | **Mud sinking core.** `UB1PhysicalMaterial_Mud`, `B1MudSurface` collision profile, per-wheel `SinkDepth`, trace offset, traction/resistance curves, belly drag. NetImGui "Mud" window: per-wheel depth, slip, friction, resistance; live-edit of the material params. | Rig tests: full throttle digs in on medium mud in 2–4 s. ≤50 % throttle crosses a 20 m medium strip. Deep mud stops the jeep (belly drag). Stuck with zero throttle, the jeep creeps < 5 cm in 10 s (Chaos low-speed check) | Playtest the mud gym: digging in at full throttle, crawling out with feathered throttle, feeling stuck in deep mud. Tune live in ImGui |

### Sprint 2 — GAS + fuel + mud polish

| # | Task | Done when (agent) | User check |
|---|---|---|---|
| T0.5 | **GAS bootstrap.** GameplayAbilities/Tags/Tasks modules. `AB1GameMode`, `AB1PlayerController` (OnPossess safe for any pawn), `AB1PlayerState` with the single ASC, jeep weak ref + `IAbilitySystemInterface`, `UB1VehicleAttributeSet`, `B1GameplayTags`. Gym maps use `AB1GameMode`. | Builds; the GAS inspector toolset shows the ASC with the attribute set on the PlayerState during PIE | `B1DamageVehicle` lowers condition in ImGui; driving still works as before |
| T0.5b | **Fuel (ADR-010).** `UB1VehicleTuningData` (idle/work rates), batched `GE_B1FuelDrain`, out-of-fuel → no torque. NetImGui "Vehicle Status" (condition, fuel, live consumption rate, editable rates, add-fuel button). Cheats `B1AddFuel`, `B1SetCondition`. | Rig tests: fuel per 100 m is ≥ 2× higher in medium mud than on firm ground. At 0 fuel the jeep moves < 1 m under full throttle | Drive the gym: does fuel pressure feel right? Adjust the rates in ImGui |
| T1.2 | **Mud state + debug.** `Vehicle.State.InMud` tag, `B1Unstuck` cheat, `ShowDebug B1Mud`. | Tag set/cleared in rig test; builds | `B1Unstuck` gets you out; ShowDebug readable |
| T1.3 | **Mud spray.** Niagara GPU sprites per wheel, spawn rate ∝ slip, mud only, depth-buffer collision. | Viewport capture shows spray only on mud wheels | Spray reads well and doesn't tank fps (`stat unit`) |
| T1.4 | **Visual trails (ADR-008).** Render target / RVT drawn at wheel contacts, used by the mud material. | Viewport capture shows trails after a rig drive | Trails look acceptable; decide whether physics ruts are needed (ADR-008 revisit) |

**Phase 1 close:** mud playtest session against the Mud checklist.

### Later phases (to be broken into tasks in their own replan)

- **Phase 2 — Damage and vegetation** (`Lvl_Gym_Obstacles`): minimum and stretch as in Scope.
- **Phase 3 — River** (`Lvl_Gym_River`): minimum and stretch as in Scope.
- **Phase 4 — On foot and winch** (`Lvl_Gym_Winch`). Do enter/exit first, then the winch, then the machete. Open design questions are listed below.
- **Phase 5 — Scenario level.** Greybox from modular meshes and spline tools on a small landscape. You do the landscape sculpting and any foliage painting; the agent places set pieces, volumes, anchors and triggers via MCP.

## Validation

| Mechanic | Criterion | Verified by |
|---|---|---|
| Mud | Full throttle digs in on medium mud in 2–4 s; ≤ 50 % throttle crosses a 20 m medium strip; deep mud stops the jeep unless winched; a stuck jeep creeps < 5 cm in 10 s at zero throttle | Rig tests + your playtest |
| Fuel | Fuel per 100 m in medium mud ≥ 2× firm ground; rates editable without recompiling | Rig test + your playtest |
| Damage | Branch section driven at 40 km/h costs ≥ 3× the condition lost at 15 km/h; at < 30 % condition, 0–30 km/h takes ≥ 25 % longer | Rig tests + your playtest |
| Vegetation | Bush drag + scratch above the threshold speed; small tree topples above the break impulse; large tree stops the jeep and deals speed-scaled damage; cut branches cost 0 condition | Rig tests + your playtest |
| River | Deep ford: ≥ 8 of 10 unwinched rig crossings end downstream on the shallow side | Rig test + your playtest |
| Winch | Winched to a tree, the jeep leaves deep mud and crosses the deep ford; a winched log moves ≥ 3 m off the path | Rig tests + your playtest |
| Scenario | Huta → Bagnisko playable start to finish, both ford routes possible | Your playtest |
| Performance | 60 fps at 1080p Medium holds through each gym and the scenario on the target laptop (RTX 3050 Ti 4 GB) | You (`stat unit`) |
| Debug | Every mechanic has a NetImGui window, a telemetry log line and, where it makes sense, a `ShowDebug` view | Agent review |

All numeric thresholds are starting values and get adjusted at playtests.

## Risks

1. **Mud layering and sinking feel (ADR-012).** The trace offset may produce suspension pops when entering or leaving mud, or odd behaviour at patch edges. Mitigation: ramp the offset with distance into the patch, test patch edges in T1.1. Fallback: radius reduction with a compensating drive-torque scale.
2. **Chaos simulation subclass + physics stepping (ADR-003).** It's an underdocumented API and switching to async physics changes game-thread timing. Mitigation: T0.4 spike before any feature work; fallback is game-thread forces with substepping.
3. **Chaos low-speed / high-slip behaviour.** Stuck in mud means near-zero velocity and high slip: creep, jitter, sleeping bodies. Mitigation: creep test in T1.1. Keep the body awake while throttle > 0, and use low-speed friction tweaks in the simulation.
4. **Winch tension stability** with a ~2 t jeep at low fps. Mitigation: capped force, damping, physics-thread application, and the T0.4 spring test as an early signal.
5. **Possession swap with a Chaos vehicle** (input contexts, vehicle sleeping, engine state, camera blend). Mitigation: `AB1PlayerController` in Sprint 2, and enter/exit as the first Phase 4 task.
6. **Phase 5 content production is manual.** MCP can't sculpt landscape or paint foliage. Mitigation: small greybox, modular meshes + spline tools, you do the landscape work; plan it as its own sprint.
7. **Low-end performance** with Niagara + trails + vegetation WPO. Mitigation: T0.1 budget done, GPU sprites, capped particle counts, `stat unit` at every phase playtest.

## Dependencies

- UE 5.8 ChaosVehiclesPlugin (engine source at `C:\Program Files\UE_5.8`)
- GameplayAbilities plugin (enabled, modules not yet linked)
- FunctionalTesting module (engine) for `AB1GymTestRig`
- NetImgui plugin (present)
- Unreal MCP editor toolsets: Blueprint, Material, Niagara, GAS inspectors, Config, Scene, Logs, PIE control. No console-command exec, so `stat`, cheats and ShowDebug are run by you
- gdep MCP (failing to connect as of 2026-10-25)

## Resolved questions (2026-10-25)

1. **Target machine:** i7-12700H, 32 GB RAM, RTX 3050 Ti Laptop (4 GB VRAM). Target is 60 fps at 1080p Medium. The 4 GB of VRAM is the tight budget: keep render targets (trails) and texture pools small.
2. **Character while driving:** hidden and attached to a seat socket. It isn't destroyed.
3. **Engine on exit:** switched off on exit (no fuel use), restarted on enter. The winch pulls only from inside (ADR-007).

---

## Critique (round 1) — 2026-10-25

### Architectural alignment — 4/5
The direction holds up. The single ASC on PlayerState (ADR-004) is Lyra-consistent. Physics-thread forces (ADR-003) are the right call for mud, water and winch. Keeping obstacles off GAS (ADR-006) is a justified exception. Three places are wrong or missing, though:
- "`AB1Jeep` derives from `AMudDriverOffroadCar` so the template stays untouched" is false. `AMudDriverPawn()` and `AMudDriverOffroadCar()` are default constructors, so a subclass can't swap the movement component class via `SetDefaultSubobjectClass`. Both template constructors must change to take `FObjectInitializer`.
- `AMudDriverPlayerController::OnPossess` does `CastChecked<AMudDriverPawn>(InPawn)` (`MudDriverPlayerController.cpp:112`). The first time the player possesses `AB1Character`, it crashes. Its respawn and UI logic also assume the pawn is always a vehicle.
- Nothing in the plan says how the PlayerState class gets set. There's no `AB1GameMode` / `AB1PlayerController` task, yet ADR-004 depends on them.

### Scope clarity — 3/5
The out-of-scope list is sharp, and the Q&A closed most design questions. In-scope is the problem: Phase 0 is spelled out task by task, but Phases 1–5 are one-line arrow chains. Each arrow hides open-ended work: "bush bending", "small tree topples", "visual trails", "Niagara spray", a first-person character. Scope has already grown once during planning (machete, log pulling, fuel). That's fine, but it shows that without per-phase "minimum vs stretch" lines, every phase will grow. Phase 5 is the biggest hidden item. Landscape sculpting, layer painting and foliage aren't available through MCP (`mcp-tools.md`), so the scenario level is mostly manual work for you, and the plan doesn't budget for it.

### Validation specificity — 2/5
Phase 0 "done when" lines are mostly testable. The feature-level validation is not. "Clearly more condition", "noticeable in acceleration", "noticeably more fuel" and "most of the time" can't fail. Concrete issues:
- The T0.1 frame-rate target is "to be set", but the target machine's specs aren't recorded anywhere.
- The T0.4 criterion ("same drift at 30 and 120 fps") would likely pass even with naive game-thread `AddForce`, because a dt-scaled constant force drifts the same either way. It doesn't test what ADR-003 is about: stability of stiff, state-dependent forces such as winch tension and mud drag at low fps.
- The project has `bSubstepping=False` and no async physics tick. So "stable with substepping" (ADR-003) currently isn't true either way: physics steps once per frame with a variable dt.

### Risk realism — 3/5
Risks 1, 3 and 4 are real. Risk 2 ("mud needs tuning") is the easy-to-name one: tuning is a cost, not a risk. The actual top risks are missing:
- **Mud collision layering.** This is the biggest technical gap in the core mechanic. If the jeep "sinks" by lowering the wheel contact, the chassis body hits the same mud surface collision and can never get below ground clearance. The real collision then fights the analytic belly drag. Mud needs two surfaces: a mud surface that only the wheel traces hit (`WheelTraceCollisionResponses` exists in 5.8) and a hard bottom below it for the chassis. That also changes ADR-002: mud can't simply be a landscape layer, it needs a separate mud mesh or volume on top of the landscape.
- **How sinking is applied.** Lowering the effective wheel radius is the obvious hack, but drive force = torque / radius, so the jeep would pull *harder* the deeper it sinks. The plan should name the hook: override `PerformSuspensionTraces` (virtual in 5.8) and offset the hits.
- **Chaos at low speed and high slip.** Being stuck in mud means living in the regime Chaos handles worst (near-zero velocity, high slip ratio): creeping, jitter, sleeping bodies. It needs a test in the mud gym early on.
- **Content production for Phase 5.** See Scope.

### Agent ergonomics — 2/5
The MCP can start and stop PIE, read logs, set properties and capture the viewport. It can't run console commands, cheats, `stat` or `ShowDebug`, and it can't drive the jeep. As planned, every debug tool is human-facing (NetImGui, ShowDebug, Exec cheats), so the agent can verify almost nothing except "it builds" and "it looks like this in a screenshot". Two cheap fixes give numeric outputs the agent can check:
- A **gym test rig** actor (`AB1GymTestRig`): a placed actor with a scripted input sequence and a `bAutoRun` flag. The agent turns it on via `set_properties`, runs `StartPIE`, and reads `LogB1Physics` lines via `LogsToolset`.
- **Headless functional tests**, run from PowerShell with the editor closed (`UnrealEditor-Cmd ... -ExecCmds="Automation RunTests B1" -unattended -nullrhi`).

Debug windows should also log a compact telemetry line, so ImGui and the logs show the same numbers.

### Implementability — 2/5
The whole plan is a multi-month program (my estimate is 6–10 sprints for a solo dev), which is fine for a prototype roadmap. But "Phase 0 = Sprint 1" is over-packed. T0.4 (simulation subclass, still an unknown) and T0.5 (GAS bootstrap + PlayerState + controller rework) are each multi-day tasks. With T0.5b (fuel), T0.3, T0.6 and the rename, that's more than two weeks. Mud has no dependency on GAS or fuel, so Sprint 1 can go straight at the two biggest unknowns: the physics hook and mud sinking. GAS can follow in Sprint 2, before damage needs it.

### Recommended changes
1. **Add ADR-012 "mud collision layering".** The mud surface uses a `B1MudSurface` collision profile that only wheel traces hit. A hard bottom sits `MaxSinkDepth` below it for the chassis. Sinking offsets the suspension trace hits in an overridden `PerformSuspensionTraces`, not the wheel radius. Belly drag is analytic. Update ADR-002: the mud physical material goes on the mud mesh/volume. Build T0.6 this way.
2. **Make T0.4 decide the physics stepping**: async physics tick (fixed dt) vs substepping. Change its criterion to a stiff spring-to-anchor test (a winch stand-in) that stays stable at 20 fps and 120 fps (`t.MaxFPS`), plus the drift check.
3. **Be honest about the template**: T0.4 changes the `AMudDriverPawn` / `AMudDriverOffroadCar` constructors to `FObjectInitializer`. Add `AB1GameMode` (PlayerState and pawn classes) and `AB1PlayerController` to T0.5, with `OnPossess` safe for non-vehicle pawns.
4. **Re-slice the sprints.** Sprint 1: T0.1, T0.2, T0.3, T0.4, T0.6, plus the core per-wheel mud sinking (Phase 1 first half). Sprint 2: T0.5 + T0.5b (GAS, fuel), the rest of Phase 1 (spray, trails, `InMud` tag).
5. **Put numbers on validation and record the target machine spec.** Suggested starting targets:
   - Medium mud, full throttle: digs in within 2–4 s.
   - Medium mud, ≤50% throttle: crosses a 20 m strip.
   - Branch section: at 40 km/h costs ≥3× the condition lost at 15 km/h.
   - Fuel per 100 m: ≥2× higher in mud than on firm ground.
   - Deep ford: 8 of 10 unwinched attempts end on the shallow side.
   - Frame rate: 60 fps at 1080p Medium on the target machine (to confirm).
6. **Rewrite Risks**: replace #2 with mud collision layering, sink-application method and Chaos low-speed/high-slip behaviour. Add Phase 5 content production (manual landscape and foliage) with a mitigation: modular meshes + spline tools, a small greybox.
7. **Add agent-verifiable tests**: `AB1GymTestRig` (scripted input + telemetry to `LogB1Physics`) in T0.6, and one headless functional test per gym from Phase 1 on.
8. **Add per-phase minimum/stretch lines** for Phases 1–5, e.g. Phase 2 minimum = impact damage + large tree + breakable branch; stretch = bush WPO bending, toppling small trees.
9. **Phase 4 design gaps**: what happens to the character while driving (hide and attach to a seat socket, or destroy and respawn), and whether the engine stays on or off while the player is outside. With the engine off there's no fuel use, and pull is in-car only per ADR-007.

### Verdict
- [ ] Ready to convert to roadmap (run `/plan-roadmap`)
- [x] Needs revision (run `/plan-replan`)

---

## Revision 1 — 2026-10-25

### Changes applied from critique
- **#1 Mud layering:** new ADR-012 (wheel-trace-only mud surface + hard bottom, `PerformSuspensionTraces` offset, analytic belly drag). ADR-002 amended rather than superseded: the custom physical material still holds, only *where* it's applied changed. T0.6 builds patches this way.
- **#2 Physics stepping:** ADR-003 amended. T0.4 now picks async fixed dt vs substepping, and is judged by a stiff spring-to-anchor test at 20/120 fps plus a drift check.
- **#3 Template honesty:** the template constructor change to `FObjectInitializer` is now part of T0.4 and stated in Approach. `AB1GameMode` and `AB1PlayerController` (any-pawn `OnPossess`) added to T0.5.
- **#4 Sprint re-slice:** Sprint 1 = T0.1–T0.4, T0.6, T1.1 (mud core). Sprint 2 = T0.5, T0.5b, T1.2–T1.4.
- **#5 Numbers:** a Validation table with numeric thresholds and who verifies each. The frame target is set to 60 fps at 1080p Medium pending the machine spec (Open question 1).
- **#6 Risks:** rewritten. Mud layering, the simulation subclass/stepping and Chaos low-speed behaviour are now the top 3; Phase 5 manual content production was added; "mud needs tuning" moved into the playtest loop.
- **#7 Agent tests:** a new Testing section. `AB1GymTestRig : AFunctionalTest` runs in PIE (driven via MCP) or headless. A `LogB1Test` category was added. Telemetry log lines are required for every debug window.
- **#8 Minimum/stretch:** a per-phase table in Scope.
- **User testing (your request):** every task now has a User check column. Each phase closes with a playtest session; a phase needs both the agent tests and your sign-off, and feel wins over thresholds.

### Changes deferred (with reason)
- **#9 Phase 4 design gaps:** listed as Open questions 2–3 with recommendations. Nothing before Phase 4 depends on them, so they don't block the roadmap.
- **Target machine spec (part of #5):** Open question 1. T0.1 was accepted on your judgement; the spec only matters for the performance check at the Phase 1 playtest.
- **Breaking Phases 2–5 into tasks:** done in each phase's own replan, once Sprint 1 has validated the simulation hook that they all depend on.

### ADR handling
- ADR-002 and ADR-003 were **amended**, not superseded: their core decisions hold, and the critique only corrected implementation details.
- ADR-012 is **new** because mud collision layering is a separate decision with its own alternatives.

### Verdict
- [x] Ready to convert to roadmap
