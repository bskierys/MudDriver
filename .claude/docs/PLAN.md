# PLAN — River Crossing prototype (Huta → Bagnisko)

## Problem

VISION.md describes one full situation: drive from Huta to Bagnisko, find the bridge over the Rwąca collapsed, take the muddy, bumpy forest path, and cross the river at a ford. Either you fight the current in deep water, or you get stuck in mud in the shallows and get out with the winch. Along the way you can clear obstacles on foot: winch a log off the path, cut branches with a machete.

Today the project is the UE vehicle template plus a NetImGui menu. None of the mechanics exist.

## Design decisions (from Q&A, 2026-10-25)

| Topic | Decision | ADR |
|---|---|---|
| Mud depth | Dynamic per-wheel sinking from wheelspin | ADR-001 |
| GAS | Single ASC on PlayerState; the jeep holds a weak ref to it | ADR-004 |
| Damage | Single `Condition` attribute + penalty effects | ADR-005 |
| Fuel | `Fuel` attribute, consumed by engine work. Refuel via cheat/ImGui only for now | ADR-010 |
| Rendering | No Lumen/RT/VSM/distance fields/Substrate | ADR-011 |
| Fail state | None. At 0 condition the jeep keeps driving with max penalties | ADR-005 |
| Winch | Uncharted-style: carry the hook on foot, fixed max cable length, analog pull from inside the jeep only | ADR-007 |
| On foot | Walk, enter/exit the jeep, winch hook (incl. pulling logs), machete for branches | — |
| Mud trails | Visual only (revisit after the mud gym) | ADR-008 |
| Workflow | Test gyms first, scenario level last | ADR-009 |
| Scouting tool | None. Obstacle and mud parameters are shown through `ShowDebug` only | — |

Assumptions (confirmed 2026-10-25): single-player only (no replication work), third-person chase camera while driving, no swimming (deep water blocks or pushes the player on foot), no day/night.

## Scope

1. **Foundation**: low-end render budget, GAS bootstrap (condition + fuel), a B1 vehicle movement/simulation subclass with an environment-force hook, per-wheel debug.
2. **Mud**: mud physical material, per-wheel sink state, traction and resistance vs depth, belly drag, Niagara mud spray, visual tire trails.
3. **Damage and vegetation**: impact damage from bumps and collisions, condition penalties, bushes (bend, drag, scratch), small trees (knock over), large trees (hard), breakable branches.
4. **Water**: river volume with flow, depth-scaled side force and traction loss. Shallow ford bed uses mud.
5. **On foot and winch**: first-person character, enter/exit vehicle, winch hook carry/attach to anchors (trees, logs), in-car pull control, machete.
6. **Scenario level**: greybox Huta → collapsed bridge → forest path → ford (deep and shallow routes) → Bagnisko.
7. **Debug for every feature**: NetImGui window + `ShowDebug` category + cheats.

### Out of scope

- Fail state, checkpoints, save/load
- Physics ruts in mud (trails are visual only)
- Per-part damage, repair
- Scouting tool for the player
- Multiplayer / replication
- Real water simulation, swimming
- Final art, audio beyond placeholders

## Approach

**Vehicle.** `AB1Jeep` derives from `AMudDriverOffroadCar` so the template stays untouched. It uses `UB1VehicleMovementComponent`, whose `CreatePhysicsVehicle`/simulation is replaced by `FB1VehicleSimulation : UChaosWheeledVehicleSimulation`. The simulation reads per-wheel contact physical materials and applies mud, water and winch forces on the physics thread (ADR-003). The game thread sends only small inputs (active river volume params, winch anchor/length/pull).

**Mud.** `UB1PhysicalMaterial_Mud` carries the tuning (ADR-002). Per wheel, every sim step:
- `SinkDepth += SinkRate * Slip * dt` while on mud. `SinkDepth -= RiseRate * dt` when rolling with traction. The result is clamped to `MaxSinkDepth`.
- Friction multiplier and rolling resistance are sampled from curves vs `SinkDepth / MaxSinkDepth`.
- The wheel contact is lowered by `SinkDepth`, so the jeep visibly sits in the mud. Above ground clearance, belly drag scales with `(SinkDepth - Clearance)`.
- "Drive slowly with intent" comes from slip feeding depth: full throttle means high slip, so the jeep digs in.

**GAS.** A single ASC on `AB1PlayerState` (ADR-004). The avatar swaps between `AB1Character` and `AB1Jeep` on possession, and the jeep keeps a weak ref to the ASC so its attributes live on while the player is outside.
- Attributes: `UB1VehicleAttributeSet { Condition, MaxCondition, Fuel, MaxFuel }`.
- Damage: `GE_B1Damage_Impact` / `GE_B1Damage_Scratch` with SetByCaller. Condition bands apply `GE_B1ConditionPenalty`, and the jeep pushes torque/steering scales into the movement component when the attribute changes.
- Fuel (ADR-010): consumption = `IdleRate + WorkRate * NormalizedEnginePower`, rates in `UB1VehicleTuningData`. It's applied as a batched `GE_B1FuelDrain` about 4×/s. At 0 fuel: `Vehicle.State.OutOfFuel` and no engine torque.
- Native tags live in `B1GameplayTags` (`State.Avatar.OnFoot`, `State.Avatar.InVehicle`, `Vehicle.State.InMud`, `Vehicle.State.InWater`, `Vehicle.State.Winching`, `Vehicle.State.OutOfFuel`, `Ability.Vehicle.Enter`, …).

**Obstacles.** `UB1ObstacleComponent` + `UB1ObstacleData` (integrity, break speed, damage-to-car per km/h, drag) (ADR-006):
- *Bush*: overlap. Drag + scratch damage above a speed threshold. Bending is material WPO driven by the jeep position in a material parameter collection.
- *Small tree*: static until the hit impulse exceeds the threshold, then it switches to simulate and topples.
- *Large tree*: hard static. Impact damage scales with normal speed. Can be a winch anchor.
- *Branch*: breakable. The car breaks it and takes damage scaled by speed. The machete breaks it with no damage to the car.
- *Log*: simulated physics actor with a winch anchor.

**Water.** `AB1RiverVolume` (box, spline later): flow direction, flow speed, depth. The force at hull sample points scales with submerged fraction × (flow velocity − point velocity), mostly sideways. Wheel friction scales down with submersion (fake buoyancy). Deep ford = strong current. Shallow ford = weak current over a mud bed.

**On foot / winch.** `AB1Character` (first-person, simple). Enter/exit is a GAS ability that swaps possession. `UB1WinchComponent` on the jeep front, `UB1WinchAnchorComponent` on trees and logs. `GA_B1CarryWinchHook` (on foot, the cable limits distance), `GA_B1WinchControl` (in car, analog pull), `GA_B1Machete` (trace in front, damage to obstacle integrity). The tension force is capped and one-sided (ADR-007). A simulating anchor gets the opposite force, which is how the log gets pulled. Cable visuals use `UCableComponent`.

**Debug, per feature.**
- NetImGui windows: Vehicle/Wheels, Mud, Condition, River, Winch, plus live editing of the data-asset params.
- `ShowDebug B1Vehicle`, `ShowDebug B1Mud`, `ShowDebug B1Obstacles` (text over obstacles: type, integrity, break speed, damage).
- Cheats: `B1Unstuck`, `B1SetCondition`, `B1Teleport <gym point>`, `B1WinchAttachNearest`.

## Phases and first tasks

### Phase 0 — Foundation (Sprint 1, proposed)

| # | Task | Done when |
|---|---|---|
| T0.1 | **Low-end render budget (ADR-011).** Lumen GI/reflections, hardware RT, VSM, mesh distance fields and Substrate are off; SSR on. *Config done 2026-10-25; needs an editor restart + measurement.* | `Lvl_Offroad` runs noticeably faster on the target machine; `stat unit` before/after recorded in HISTORY.md |
| T0.2 | **Housekeeping.** Log categories `LogB1Vehicle`, `LogB1Physics`. Rename debug classes: `UMudImGuiSubsystem` → `UB1ImGuiSubsystem`, `UMudCheatManager` → `UB1CheatManager`, `MudImGui` namespace → `B1ImGui` (files too). | Builds; no `Mud*` custom classes left |
| T0.3 | **Wheel debug.** Extend NetImGui "Vehicle Basics" with a per-wheel table: contact, phys material, slip, skid, suspension length, drive/brake torque. Add `ShowDebug B1Vehicle` text over the jeep. | Table updates live while driving on `Slippery` vs `NonSlippery` |
| T0.4 | **Simulation spike (ADR-003).** `UB1VehicleMovementComponent` + `FB1VehicleSimulation`, wired into `AB1Jeep` / `BP_B1Jeep`. Add a test force (constant sideways push toggled by cheat), applied on the physics thread. | Jeep drifts sideways at the same rate at 30 and 120 fps (`t.MaxFPS`). ADR-003 → accepted or fallback chosen |
| T0.5 | **GAS bootstrap.** Add the GameplayAbilities/Tags/Tasks modules, `AB1PlayerState` with the single ASC, the jeep's weak ref + `IAbilitySystemInterface`, `UB1VehicleAttributeSet` (Condition, Fuel), `B1GameplayTags`. | Cheat damages the jeep and the ImGui window shows it |
| T0.5b | **Fuel (ADR-010).** `UB1VehicleTuningData` with idle/work rates, batched `GE_B1FuelDrain`, out-of-fuel → no torque. NetImGui "Vehicle Status" window (condition, fuel, live consumption rate, editable rates, add-fuel button). Cheats `B1AddFuel`, `B1SetCondition`, `B1DamageVehicle`. | Full throttle stuck in place drains fuel visibly faster than idling or cruising; at 0 fuel the jeep can't drive |
| T0.6 | **Mud gym map.** `Lvl_Gym_Mud`: flat greybox, strips of shallow/medium/deep mud, a slope out of a mud pit, spawn points. Mud physical material placeholder assigned. | Map loads with `BP_B1Jeep` and the per-wheel table shows the mud material |

Recommended order: T0.1 → T0.2 → T0.3 → T0.4 → T0.5 → T0.5b → T0.6.

### Phase 1 — Mud (`Lvl_Gym_Mud`)
Mud physical material + per-wheel sink → traction/resistance curves → belly drag → Niagara spray (spawn rate ∝ slip, mud only, GPU sprites) → visual trails → `Vehicle.State.InMud` tag → Mud ImGui window with live tuning, `B1Unstuck`.

### Phase 2 — Damage and vegetation (`Lvl_Gym_Obstacles`)
Impact damage from chassis hits → condition penalties → obstacle component/data → bush, small tree, large tree, branch, log → `ShowDebug B1Obstacles`.

### Phase 3 — River (`Lvl_Gym_River`)
River volume → hull sample forces → traction loss → deep vs shallow-mud ford → flow debug arrows + River ImGui window.

### Phase 4 — On foot and winch (`Lvl_Gym_Winch`)
`AB1Character` → enter/exit ability → winch component + anchors → carry hook → in-car pull → log pulling → machete → Winch ImGui window.

### Phase 5 — Scenario level
Greybox landscape for Huta → collapsed bridge → forest path (mud, bumps, branches, a log) → ford with two routes → Bagnisko arrival trigger.

## Validation

- **Mud**: at full throttle the jeep digs itself in on medium mud within ~3 s. Feathered throttle gets it through. Deep mud stops it fully (belly drag) unless winched.
- **Damage**: driving the forest path at 40+ km/h costs clearly more condition than at 15 km/h. Low condition is noticeable in acceleration.
- **Fuel**: spinning in mud burns noticeably more fuel than driving the same distance on firm ground. Rates are tunable without recompiling.
- **Vegetation**: bushes bend and slow the jeep. Small trees fall. Large trees stop it and deal damage scaled by speed. Cut branches cost no condition, driven-through branches do.
- **River**: in the deep ford, an unwinched jeep gets pushed downstream to the shallow side most of the time.
- **Winch**: hook to a tree, pull from the jeep, and the jeep gets out of deep mud and across the deep ford. Hook to a log and the log is dragged off the path.
- **Scenario**: the whole Huta → Bagnisko route is playable start to finish, on the target machine, at a stable frame rate (target to be set in T0.1).
- Every mechanic has a NetImGui window and a `ShowDebug` view.

## Risks

1. **Subclassing the Chaos vehicle sim is underdocumented and may change between engine versions.** Mitigation: T0.4 spike first; fallback is game-thread forces with substepping on.
2. **The mud feel may need many tuning passes.** Mitigation: all params in physical material / data assets, live-editable from NetImGui, a dedicated gym.
3. **Winch tension may get unstable with a ~2 t jeep at low fps.** Mitigation: force cap, damping, applied in the physics step (ADR-003/007).
4. **Possession swap with a Chaos vehicle** (input contexts, vehicle sleeping, engine idle, camera blend). Mitigation: do enter/exit early in Phase 4, before the winch.
5. **Low-end performance with Niagara + render-target trails + vegetation WPO.** Mitigation: T0.1 budget first, GPU sprites, cap particle counts, `stat unit` check per phase.

## Dependencies

- UE 5.8 ChaosVehiclesPlugin (engine source available at `C:\Program Files\UE_5.8`)
- GameplayAbilities plugin (enabled, modules not yet linked)
- NetImgui plugin (present)
- Unreal MCP editor toolsets (available: Blueprint, Material, Niagara, GAS inspectors, Config, Scene; no console-command exec, so `stat` measurements are manual)
- gdep MCP (still failing to connect as of 2026-10-25)
