# DECISIONS

ADRs. Each entry: problem / decision / rationale / alternatives. Newest at the bottom.

---

## ADR-001 — Mud: dynamic per-wheel sinking

**Status:** accepted (2026-10-25)

**Problem.** Mud must punish spinning the wheels and reward driving slowly and with intent (VISION: "the faster they try to go, the more wheels spin and the player can't drive").

**Decision.** Each wheel keeps a `SinkDepth` state. It grows with wheel slip while the wheel is on a mud surface and shrinks when the wheel has traction and is rolling. Traction, rolling resistance and the chassis height all depend on the current depth. Each mud surface sets a `MaxSinkDepth`. When the depth goes past ground clearance, belly drag kicks in. That gives the full stop.

**Rationale.** Matches the Snowrunner feel: wheelspin digs you in, careful throttle gets you out. A single state value per wheel is cheap and easy to tune.

**Alternatives.** Authored fixed-depth zones (simpler, but wheelspin has no consequence). Layered authored and dynamic depth (more parameters, no clear gain for the prototype).

---

## ADR-002 — Mud parameters live in a custom physical material

**Status:** proposed (2026-10-25)

**Problem.** Mud parameters need to be per-surface and readable on the physics thread from the wheel contact.

**Decision.** `UB1PhysicalMaterial_Mud : UPhysicalMaterial` holds the mud tuning (max sink depth, sink and rise rates, traction and resistance curves vs depth). Chaos already reports the contact physical material per wheel (`FWheelStatus::PhysMaterial`).

**Rationale.** No extra volume queries. Landscape layers and meshes get mud through the usual physical material assignment.

**Alternatives.** Mud overlap volumes (extra overlaps, and they don't follow the terrain shape). Data asset looked up by surface type (an extra indirection).

---

## ADR-003 — Environment forces run inside the Chaos vehicle simulation

**Status:** proposed, needs a spike (2026-10-25)

**Problem.** Mud resistance, river current and winch tension are continuous forces. If they are applied from the game thread, they depend on frame rate and get unstable on a low-end PC.

**Decision.** Subclass `UChaosWheeledVehicleMovementComponent` (`UB1VehicleMovementComponent`) and `UChaosWheeledVehicleSimulation` (`FB1VehicleSimulation`). Environment forces are applied in the simulation step on the physics thread. The game thread only sends inputs: active water volume, winch anchor and length.

**Rationale.** Forces get the same timestep as the tire and suspension model, and stay stable with substepping.

**Alternatives.** `AddForce` from the component Tick (fastest to write, frame-rate dependent). It's the fallback if the spike shows that subclassing the simulation is too brittle in 5.8.

---

## ADR-004 — GAS ownership: single ASC on PlayerState

**Status:** accepted (2026-10-25)

**Problem.** The player switches between the jeep and the on-foot character. The jeep's attributes (condition, fuel) must persist when the player gets out.

**Decision.** One `UAbilitySystemComponent` on `AB1PlayerState` (Lyra pattern). It holds the player abilities and `UB1VehicleAttributeSet`. Only one avatar is active at a time: the character or the jeep. On possession, `InitAbilityActorInfo(PlayerState, NewPawn)` swaps the avatar. The jeep keeps a `TWeakObjectPtr` to the PlayerState ASC and implements `IAbilitySystemInterface` through it. This way its attributes stay readable and damageable while the player is outside.

**Rationale.** One place for all gameplay state, and no syncing between two ASCs. The prototype has a single player and a single jeep, so jeep attributes on the player's ASC are fine. The user has used this setup before.

**Alternatives.** A separate ASC on the jeep (cleaner if there are ever multiple vehicles, but needs routing between two ASCs). An ASC per pawn (player abilities would be lost on swap).

**Consequences.** Abilities must check the avatar type: on-foot abilities need `AB1Character`, in-car ones need `AB1Jeep`, gated by tags (`State.Avatar.OnFoot` / `State.Avatar.InVehicle`). Vehicle state tags (`Vehicle.State.InMud`, …) live on the player ASC and must be cleared by the jeep, not by possession. Supporting several vehicles later means revisiting this ADR.

---

## ADR-005 — Vehicle damage: single Condition attribute + penalty effects

**Status:** accepted (2026-10-25)

**Problem.** Impacts, bumps and branches must damage the jeep when driving fast.

**Decision.** A single `Condition` attribute. Damage comes from `GE_B1Damage_*` effects with SetByCaller magnitude. Low condition applies penalty effects (engine torque, steering), which the vehicle pushes into the movement component. There is no fail state in the prototype: at 0 the jeep keeps driving with maximum penalties.

**Rationale.** Cheap, and can grow into per-part attributes later without changing who applies the damage.

**Alternatives.** Per-part damage (engine, tires, suspension): more design work than the prototype needs.

---

## ADR-006 — Obstacles use a lightweight component, not an ASC

**Status:** proposed (2026-10-25)

**Problem.** There will be many bushes, branches and trees. Each needs integrity, a break threshold and damage-to-car rules.

**Decision.** `UB1ObstacleComponent` + `UB1ObstacleData` (data asset). No ASC on obstacles. The car and the machete reduce integrity through `IB1Damageable`. Damage *to the car* still goes through GAS effects on the PlayerState ASC (ADR-004).

**Rationale.** An ASC per bush is overkill and costs memory and tick. Obstacles don't need abilities or effects stacking.

**Alternatives.** An ASC per obstacle (consistent with GAS, but heavy).

---

## ADR-007 — Winch: tension force, not a physics constraint

**Status:** proposed (2026-10-25)

**Problem.** The winch must pull the jeep toward an anchor (tree) or pull a log toward the jeep, with analog pull control.

**Decision.** `UB1WinchComponent` models the cable as a one-sided spring-damper. It produces force only when taut, is capped at `MaxPullForce`, and reels in at a rate set by player input. The force is applied in the vehicle simulation (ADR-003). If the anchor simulates physics, it gets the opposite force. Cable visuals use `UCableComponent`. It works Uncharted-style: the hook is carried on foot to an anchor, and pull is controlled from inside the jeep only.

**Rationale.** Physics constraints with changing length are stiff and tend to explode with heavy vehicles. A capped force is predictable and easy to tune.

**Alternatives.** `UPhysicsConstraintComponent` with a linear limit (hard to animate the length). A rope made of physics segments (expensive).

---

## ADR-008 — Mud trails are visual only

**Status:** accepted, revisit after the mud gym (2026-10-25)

**Problem.** Tires should leave trails in mud.

**Decision.** Trails are drawn into a render target or runtime virtual texture and used by the mud material (darkening, fake depth). They have no effect on physics.

**Rationale.** Cheap on a low-end PC. Physics ruts are a separate system and can be added later.

**Alternatives.** Ruts that store depth and affect sinking and steering (memory per mud zone, much more work).

---

## ADR-009 — Mechanics are built in test gyms before the scenario level

**Status:** accepted (2026-10-25)

**Problem.** The Huta → Bagnisko scenario depends on five mechanics that each need tuning.

**Decision.** One small greybox map per mechanic (`Lvl_Gym_Mud`, `Lvl_Gym_Obstacles`, `Lvl_Gym_River`, `Lvl_Gym_Winch`), tuned through NetImGui. The scenario level is built only after the mechanics feel right.

**Rationale.** Fast iteration, so each mechanic is tuned on its own.

**Alternatives.** Greybox scenario first. A mud + winch vertical slice first.

---

## ADR-010 — Fuel attribute, consumed by engine work

**Status:** accepted (2026-10-25)

**Problem.** Fuel should add pressure: fighting mud costs more than cruising. There's no refuelling interaction yet.

**Decision.**
- `Fuel` and `MaxFuel` attributes in `UB1VehicleAttributeSet`.
- Consumption per second = `IdleRate + WorkRate * NormalizedEnginePower`, where power comes from engine torque × RPM, normalised to max power. Both rates sit on a designer data asset (`UB1VehicleTuningData`) and can be live-edited from NetImGui.
- The jeep accumulates consumption and applies `GE_B1FuelDrain` (SetByCaller `Data.Fuel`) about 4 times a second, not every frame.
- At 0 fuel, `Vehicle.State.OutOfFuel` is set and engine torque goes to 0. That isn't a fail state.
- Fuel can be added through the `B1AddFuel` cheat and the ImGui window.

**Rationale.** Mud already makes the engine work hard (high throttle and RPM while slipping), so extra consumption in mud falls out without special cases. The batched effect keeps GAS traffic low.

**Alternatives.** Distance-based consumption (doesn't punish wheelspin). A periodic infinite GE with a dynamic magnitude (harder to drive from per-frame engine state).

---

## ADR-011 — Low-end render budget

**Status:** accepted (2026-10-25)

**Problem.** The template ships with Lumen, hardware ray tracing, virtual shadow maps and Substrate. That's at odds with the "physics, not graphics" target on a low-end PC.

**Decision.**
- No dynamic GI (`r.DynamicGlobalIlluminationMethod=0`). Screen-space reflections (`r.ReflectionMethod=2`).
- Hardware ray tracing off. Shadow maps instead of VSM. Mesh distance fields off. Substrate off.
- Virtual textures stay on: landscape RVT now, and maybe mud trails later (ADR-008).

**Rationale.** These are the biggest GPU costs in the default UE5 setup and the game doesn't need them.

**Alternatives.** Keep the features and lower scalability per machine (still pays the shader and memory cost).

**Consequences.** Lighting is flatter: sky light and SSAO only. Niagara particle collision must use depth-buffer collision, not distance fields.
