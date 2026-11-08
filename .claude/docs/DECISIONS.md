# DECISIONS

ADRs. Each entry: problem / decision / rationale / alternatives. Newest at the bottom.

---

## ADR-001 — Mud: dynamic per-wheel sinking

**Status:** accepted (2026-10-25)

**Problem.** Mud must punish spinning the wheels and reward driving slowly and with intent (VISION: "the faster they try to go, the more wheels spin and the player can't drive").

**Decision.** Each wheel keeps a `SinkDepth` state. It grows with wheel slip while the wheel is on a mud surface and shrinks when the wheel has traction and is rolling. Traction, rolling resistance and the chassis height all depend on the current depth. Each mud surface sets a `MaxSinkDepth`. When the depth goes past ground clearance, belly drag kicks in. That gives the full stop.

**Rationale.** Matches the Snowrunner feel: wheelspin digs you in, careful throttle gets you out. A single state value per wheel is cheap and easy to tune.

**Alternatives.** Authored fixed-depth zones (simpler, but wheelspin has no consequence). Layered authored and dynamic depth (more parameters, no clear gain for the prototype).

**Amended 2026-10-25 (S1-19 playtest).** Slip-only sinking felt wrong: only the spinning front wheels sank, and careful driving didn't sink at all. Sinking now has two parts:
- **Weight:** every wheel on mud settles to a static depth (`MaxSinkDepth·StaticSinkRatio`, scaled by the wheel's load relative to the average), at any speed.
- **Wheelspin:** digs below that, up to `MaxSinkDepth`. Rolling with grip climbs back only to the static depth.

Digging also scales with load, so an unloaded wheel spinning in the air doesn't dig. Slow, careful driving still pays off: it avoids the dig, not the settle.

**Amended 2026-10-26 (S1-20 playtest).** Medium mud is a slog, not a trap. Grip at full depth stays above rolling resistance, so the jeep can always crawl out, and full stops are left to belly drag in deep mud. Rolling resistance is low (0.02–0.25 × load). A speed drag (`SpeedDragCoefficient` × normalized depth × v²) slows fast driving hard and barely touches slow driving. At a standstill, rolling resistance acts like static friction, so spinning wheels don't creep. Presets come in four sizes: Shallow (S, 25 cm), Medium (M, 35), Heavy (L, 50) and Deep (XL, 70).

**Amended 2026-10-26 (mud + winch replan, user decision): slopes.** Built in the Sprint 3 slope spike; values are starting points.
- **Behaviour.** A stopped jeep on a muddy slope slides down slowly: the mud is slippery and gravity wins. How much the mud holds depends on how deep the jeep has sunk, not on the preset. Barely sunk slides; deeply sunk is held but still can't climb. A jeep on Deep slides until it has sunk in, then stops.
- **Slope hold.** Per mud wheel, a force opposes the net downhill force along the ground (gravity along the ground minus the tire's uphill push), up to `SlopeHoldVsSinkDepth(SinkDepth cm) × wheel load`. It only cancels a net downhill force, so it never helps the jeep up. The curve is over absolute depth and sits on `UB1VehicleMovementComponent` (`Mud|Config`, next to `BellyClearance`). Start: ~0 up to 15 cm, full hold around 30 cm.
- **Standstill rule.** Rolling resistance and belly drag judge "push" from the net force along the ground (tire force + gravity along the ground + winch pull), not the tire force alone. On a slope the old rule read the tire's holding force as push and applied resistance downhill.
- **Flat ground is unchanged.** Gravity along the ground is zero there, so the S1-24 tuning holds; the flat mud suite guards it.
- **No sleep on muddy slopes (S3-03, 2026-10-27).** Chaos puts a vehicle to sleep under 10 cm/s on slopes up to 30°, which holds it regardless of mud. While mud under a wheel is steeper than `KeepAwakeSlope` (3°), the mud component asks the host to stay awake (`IB1MudSimHost::SetMudKeepAwake`); flat mud still sleeps.
- **Rolling back on a muddy slope (S3-04, 2026-10-27, user decision).** With no throttle, brake or handbrake on mud steeper than `KeepAwakeSlope`, the mud wheels' brake torque is zeroed. Chaos's engine braking (RPM × `EngineBrakeEffect`) would otherwise lock them. Holding the brake pedal still brakes. Chaos's suspension anti-slide is off (ADR-016).
- **Superseded after the S3-06 playtest (2026-10-27, user decision): slope pull instead of slope hold.** Dug in on medium, the hold left the jeep unable to climb *or* reverse. The intended feel: mud is slippery, the jeep keeps sliding down, and the low wheel grip makes climbing very hard. So the hold curve and slide drag are gone. Every mud wheel on a slope gets an extra pull down the slope: `SlopePull` (on `UB1MudVehicleComponent`; 0.3 on `BP_B1Jeep`, tuned by the user) × gravity along the ground for its weight share. It's always on, and deep mud's other mechanics (rolling resistance, belly drag) stay as they are. The "Slope hold" and "dug in holds / deep slides then stops" lines above no longer apply. Slope angle 20°.
- **Alternatives.** Hold by preset (rejected by the user: a Deep jeep that hasn't sunk yet should still slide). Lowering grip on slopes (doesn't give "held when deep"). A per-material slope grip factor stays as the fallback if no slope angle gives "firm climbs, medium fails".

**Amended 2026-10-27 (S2-20, portability).** Mud config moved off the movement component onto `UB1MudVehicleComponent` (`BellyClearance`, unstuck settings). `SlopeHoldVsSinkDepth` goes there too (`Mud|Config`), not on `UB1VehicleMovementComponent`; the force itself lives in `FB1MudSimulation`.

---

## ADR-002 — Mud parameters live in a custom physical material

**Status:** accepted (2026-10-25, built in S1-17)

**Problem.** Mud parameters need to be per-surface and readable on the physics thread from the wheel contact.

**Decision.** `UB1PhysicalMaterial_Mud : UPhysicalMaterial` holds the mud tuning (max sink depth, sink and rise rates, traction and resistance curves vs depth). Chaos already reports the contact physical material per wheel (`FWheelStatus::PhysMaterial`).

**Rationale.** No extra volume queries. Mud meshes get mud through the usual physical material assignment.

**Alternatives.** Mud overlap volumes (extra overlaps, and they don't follow the terrain shape). Data asset looked up by surface type (an extra indirection).

**Amended 2026-10-25 by ADR-012.** The material goes on a separate mud surface mesh, not on a landscape paint layer. A landscape layer can't have its own collision channel, which sinking needs.

**Implementation (2026-10-25, S1-17).** Chaos already applies `PhysMaterial->Friction` as the tire's surface friction. The mud material's base `Friction` is therefore the zero-depth grip, and the depth curve is a multiplier on top. The engine reads `HitResult.PhysMaterial` on the physics thread too; our own mud params still get copied into the sim input (S1-18 note), because ImGui edits them live.

---

## ADR-003 — Environment forces run inside the Chaos vehicle simulation

**Status:** accepted (2026-10-25; stepping decided in S1-12)

**Problem.** Mud resistance, river current and winch tension are continuous forces. If they are applied from the game thread, they depend on frame rate and get unstable on a low-end PC.

**Decision.** Subclass `UChaosWheeledVehicleMovementComponent` (`UB1VehicleMovementComponent`) and `UChaosWheeledVehicleSimulation` (`FB1VehicleSimulation`). Environment forces are applied in the simulation step on the physics thread. The game thread only sends inputs: active water volume, winch anchor and length.

**Rationale.** Forces get the same timestep as the tire and suspension model, and stay stable with substepping.

**Alternatives.** `AddForce` from the component Tick (fastest to write, frame-rate dependent). It's the fallback if the spike shows that subclassing the simulation is too brittle in 5.8.

**Amended 2026-10-25 (plan critique round 1).**
- **Physics stepping.** The project currently has `bSubstepping=False` and no async physics tick, so physics steps once per frame with a variable dt. The T0.4 spike also picks the stepping mode: async physics with a fixed dt (recommended, Chaos vehicles are built for it) or substepping. It's tested with a stiff spring-to-anchor force at 20 and 120 fps.
- **Template change.** Swapping the movement component class means changing the `AMudDriverPawn` / `AMudDriverOffroadCar` constructors to take `FObjectInitializer`.

**Amended 2026-10-25 (spike S1-07..S1-09).** Subclassing works in 5.8: `FB1VehicleSimulation` ticks on the physics thread, and a 3 kN push and a stiff spring-to-anchor both apply from it (user drove against the spring: works as described). One deviation from the plan: custom input can't travel in `FChaosVehicleAsyncInput`, because `SetCurrentAsyncInputOutput` is non-virtual and always allocates the base structs. Game-thread data goes through a lock-guarded latest-value buffer on the simulation instead. The fallback (component-Tick `AddForce`) isn't needed. Stepping mode is still open (S1-12).

**Amended 2026-10-25 (S1-12, stepping).** Async physics with a fixed 1/60 s step: `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, plus `MaxPhysicsDeltaTime=0.1`.
- **Measured.** Rigs ran headless at 20 and 120 fps, using a 3 kN push rolling the jeep for 5 s and 40 % throttle against the spring. Full numbers are in HISTORY.md (2026-10-25, S1-12).
- **The real problem was the clamp.** `MaxPhysicsDeltaTime` defaults to 1/30 s and clamps the frame delta in every mode, async included. At 20 fps physics then runs at 2/3 real time: push distance was 53 % short. Raising it to 0.1 keeps real time down to 10 fps.
- **Async vs substepping.** With the clamp raised, async and substepping (1/60 max) both land within 1 % between frame rates. A single variable step (sync) lands within 3.6 %. Async wins:
  - the same dt at every frame rate, so rig results repeat exactly across runs and machines;
  - physics runs off the game thread (a side benefit for later optimisation, not a driver);
  - at high fps it steps at 60 Hz, not once per frame.
- **Cost.** The game thread reads interpolated physics results, about one frame behind. Camera, template UI and flip check are a user check.
- **Rejected.** Substepping (ties on numbers, dt still varies with fps, sim steps on the game thread's timeline). Sync single step (frame-rate dependent beyond 3 % at 20 fps, worse with stiffer forces).

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

**Status:** accepted (2026-10-27, S3-16 checkpoint)

**Problem.** The winch must pull the jeep toward an anchor (tree) or pull a log toward the jeep, with analog pull control.

**Decision.** `UB1WinchComponent` models the cable as a one-sided spring-damper. It produces force only when taut, is capped at `MaxPullForce`, and reels in at a rate set by player input. The force is applied in the vehicle simulation (ADR-003). If the anchor simulates physics, it gets the opposite force. Cable visuals use `UCableComponent`. It works Uncharted-style: the hook is carried on foot to an anchor, and pull is controlled from inside the jeep only.

**Rationale.** Physics constraints with changing length are stiff and tend to explode with heavy vehicles. A capped force is predictable and easy to tune.

**Alternatives.** `UPhysicsConstraintComponent` with a linear limit (hard to animate the length). A rope made of physics segments (expensive).

**Amended 2026-10-26 (mud + winch replan).** Stays proposed until the Sprint 3 stability rigs pass.
- **Own force path.** The winch is a separate force in `FB1VehicleSimulation`, next to the S1-09 test force, which stays for the stepping rigs.
- **Point of application.** `AddForceAtPosition` at the winch point on the jeep's front, so a winched jeep pitches nose-up on a slope.
- **Drag caps.** The winch pull counts as push in the rolling-resistance and belly-drag caps (ADR-001 standstill rule). Otherwise a belly-stuck jeep wouldn't move under full pull, because the caps only let the tires' push through.
- **Reel-in.** The rest length shrinks at `ReelInSpeed × pull` while taut, so tension ramps up to `MaxPullForce` instead of stepping. The body is kept awake while attached.
- **Pull input.** Native analog input on the jeep (like throttle), not an ability. Continuous controls stay native; abilities are for discrete actions (ADR-014).
- **Log.** A simulating anchor gets the opposite force in its own async physics tick (fixed step), reading the cable state the winch publishes. Fallback: game-thread `AddForce` on the log only.

**Amended 2026-10-27 (S3-08..S3-10, built).** Still proposed until the S3-15 stability rigs pass.
- **Reel-in and stall.** Pull reels in slack as well as a taut cable. The drum stalls once the spring force reaches `MaxPullForce`, so the cable never stores more than the cap (no lurch when the jeep breaks free). The rest length lives on the physics thread; the game thread only resets it per attach (serial).
- **Drag caps, corrected premise.** Before counting the winch, mud barely resisted it: the caps let only the tire push through, so the winch dragged a stuck jeep almost freely, the opposite of what the amendment above expected. Counting the winch as push (`FB1MudSimulation::SetExternalForce`, a generic host hook) lets mud resist it at full strength.
- **Input until on foot.** One toggle (attach nearest / release) plus pull, in the vehicle mapping context, until the on-foot hook carry (S4-07) replaces attaching from the seat. User's pad layout: D-pad Up pull, D-pad Down toggle.

**Accepted 2026-10-27 (S3-15/16).** Stability rigs pass at 20 and 120 fps with the same tension (21.1 kN peak, ~1 kN range in the last 3 s). After the playtest the user tuned `BP_B1Jeep` to `MaxPullForce` 60 kN and `MaxCableLength` 8000 cm (80 m, S4-01); winch, slope and flat suites all pass with these values.
- **Snatch overrun = drum slip (user, 2026-10-27).** Tension stays capped while a cable is being stretched, so a jeep that drives into a taut cable at speed pulls cable off the drum (`B1Winch_CableSnatch`: 4.9 m past a 77.8 m rest at 60 kN). Accepted as a slipping drum brake; no hold cap above `MaxPullForce`.

**Amended 2026-10-27 (S4-13, reel-out).**
- **Signed pull.** Pull is −1..1. Positive reels in as before. Negative grows the rest length at `ReelOutSpeed × |pull|` up to `MaxCableLength`, and ignores the stall, so a jeep hanging on the cable descends at reel speed. `Vehicle.State.Winching` stays reel-in only.
- **Reel-out damping.** While paying out, damping acts on the stretch rate (body speed minus payout rate). Damping on body speed alone read a steady 3 m/s descent as 60 kN and snatched the jeep in a loop. Reel-in keeps body-speed damping so tension still ramps up (applying stretch-rate damping there stepped every pull to the cap).
- **Start-up overshoot accepted (user).** A full reel-out step lets the hanging jeep lag, then catch the cable: on the medium slope it peaks at 4.3 m/s in the first seconds, then holds 3.0 m/s with steady tension. No payout ramp.
- **Cable visual collision (S4-12).** The `UCableComponent` rests on the world through its own particle sweeps: object type WorldDynamic, blocking WorldStatic, WorldDynamic and the mud channel, ignoring vehicles and pawns (the cable can't ignore its owner, so filtering is by type). One segment per metre of `MaxCableLength`. Visual only; the sim cable stays a straight spring. A reeling winch wakes a sleeping jeep, since the drum lives in the physics step.
- **Hook carry (S4-07).** `UB1WinchComponent` hook states: stowed (no cable), carried (visual cable to the character's `HookHand`, no sim input, no tension), attached. The carried cable pays out as you walk away and leaves slack when you walk back. Leash in the winch tick: past `MaxCableLength` the carrying `ACharacter` is swept back onto that radius and loses its outward velocity, so a sliding jeep drags it. Each state change is its own instant ability fired by an interaction option (jeep front: take / return; anchor: attach / detach, which puts the hook back in hand). Possessing a jeep stows a carried hook.
- **Input.** Detach has its own action (`IA_B1WinchDetach`, Q / pad Y). Attaching from the seat ends; `B1WinchAttachNearest` and the Winch window's Attach button stay as debug until the on-foot hook carry (S4-07). Pull: arrow Up/Down, D-pad Up/Down.

**Amended 2026-10-28 (S4-09, log).** Replaces the 2026-10-26 "Log" line (async tick on the log).
- **Opposite force from the vehicle sim, same step.** For an anchor whose attach parent simulates, the winch sends its physics proxy and the anchor point in the body frame. `UpdateWinch` reads the anchor body's pose and velocity on the physics thread, damps on relative closing speed, and `ApplyDeferredForces` (the single-threaded pass after Chaos's parallel vehicle update) applies exactly −force plus its torque at the anchor point. No cross-thread read, no frame lag. Rejected: the log's own async tick reading the published tension (one step late, needs its own tick class), and game-thread `AddForce` (a frame late, not equal and opposite).
- The anchor offset is unscaled (the body frame has no scale). An anchor unhooks its winches in `EndPlay`, so the sim never reads a destroyed body.

**Amended 2026-10-28 (fallen tree replan; S4-14, built).** Supersedes the S4-13 detach key.
- **Handbrake hold.** The handbrake (`IA_Handbrake`: Space and pad B; the template's pad LB removed, and pad A no longer brakes, user) is a normal Chaos handbrake. While the hook is attached, it also adds a standstill hold in `FB1VehicleSimulation`: −(this step's winch force + gravity along the ground) on the ground plane, plus a drift-killing velocity term. The hold is capped at `HandbrakeHoldForce` (`Winch|Config`, 80 kN > `MaxPullForce`) and only acts below `HandbrakeHoldSpeed` with ≥ 3 wheels down. It never pushes. It's computed right after `UpdateWinch` and is zero in the step the anchor clears, so a detach or a sudden drop in tension leaves no phantom shove. Winch + hold reach the mud sim as one external force. The goal: the winch pulls a heavy object toward the jeep instead of pulling the jeep in (user). Not scaled on mud (user: see how it plays).
- **No in-car detach.** `IA_B1WinchDetach` goes away; the hook is taken off at the anchor on foot (user). The cheat and the Winch window's Detach stay as debug.
- Rejected: a plain wheel lock (a heavy pull still drags the jeep); the hold on every handbrake (would change the driving handbrake for no gain, since the hold only matters with a hook).
- Built: the handbrake is read from the step's `InputData.PhysicsInputs.NetworkInputs.VehicleInputs`. `UChaosVehicleSimulation::VehicleInputs` is only filled with network prediction on, so it reads 0 here. The drift term stops a drift over 0.1 s (no overshoot at 1/60 s). Brake at a standstill is reverse throttle in Chaos, and the hold doesn't fight the engine, so brake + handbrake creeps back (~0.2 m/s at the cap). The hold rigs use the handbrake alone, like the user's "hold B". `B1Winch_FirmPull` dropped its handbrake to stay a "braked jeep gets dragged" rig.
- Order-independent (user check, 2026-10-28). Above `HandbrakeHoldSpeed` the hold cancels only the cable pull, so the handbrake brings a reeling jeep to a stop. Below it, the full hold (gravity + drift) takes over. Before this, pressing B mid-reel did nothing: the jeep followed the drum above the gate. Gravity and drift stay gated, so a handbrake slide on a slack cable doesn't stop dead.

---

## ADR-008 — Mud trails are visual only

**Status:** accepted, revisit after the mud gym (2026-10-25)

**Problem.** Tires should leave trails in mud.

**Decision.** Trails are drawn into a render target or runtime virtual texture and used by the mud material (darkening, fake depth). They have no effect on physics.

**Rationale.** Cheap on a low-end PC. Physics ruts are a separate system and can be added later.

**Alternatives.** Ruts that store depth and affect sinking and steering (memory per mud zone, much more work).

**Amendment (2026-10-26, user decision after S2-14).** Darkening alone reads flat, worst on Heavy/Deep. Ruts stay visual only, but must look dug in: the per-patch trail target stores rut depth (the wheel's sink depth, max-blended), a dense visual surface mesh is displaced down by it in the vertex shader with a raised lip, and normals come from the depth gradient (no shadow maps, ADR-011). Rejected: parallax-only (breaks at the low chase-camera angle and at patch edges), Nanite tessellation (experimental, too heavy for the low-end target), physics ruts (not wanted).

---

## ADR-009 — Mechanics are built in test gyms before the scenario level

**Status:** accepted (2026-10-25)

**Problem.** The Huta → Bagnisko scenario depends on five mechanics that each need tuning.

**Decision.** One small greybox map per mechanic (`Lvl_Gym_Mud`, `Lvl_Gym_Obstacles`, `Lvl_Gym_River`, `Lvl_Gym_Winch`), tuned through NetImGui. The scenario level is built only after the mechanics feel right.

**Rationale.** Fast iteration, so each mechanic is tuned on its own.

**Alternatives.** Greybox scenario first. A mud + winch vertical slice first.

**Amendment (2026-10-26).** Mechanic gyms can have a playground companion for hands-on playtests: `Lvl_Gym_Swamp` mixes every mud depth, slopes and low ground over a large area. It has no rigs; rigs stay in the per-mechanic gyms.

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

**Amendment (2026-10-26): mud strain term.** The rationale above didn't hold: Chaos engine torque is throttle² × curve(RPM) and ignores load, and the jeep rarely spins in medium mud, so a slow jeep in mud burned less per hour than a fast one on firm ground (user playtest). Consumption is now `IdleRate + WorkRate * NormalizedEnginePower + MudWorkRate * MudStrain`, with `MudStrain = throttle * clamp(average wheel sink depth / MudFullStrainDepth, 0, 1)`. Both new values sit on `UB1VehicleTuningData` and are editable in the Vehicle Status window. Alternative considered: resistive power (rolling resistance + belly drag × speed), rejected because it's ~0 when stuck at full throttle, which is exactly when the engine strains most.

**Amended 2026-10-27 (S2-22, portability).** Consumption lives in `UB1FuelConsumptionComponent` (`Fuel/`), not the jeep. It knows nothing of this project's GAS classes: `FuelAttribute`, `DrainEffect`, `DrainSetByCallerTag` and `OutOfFuelTag` are properties, and it works on whatever ASC `UAbilitySystemGlobals` finds on the owner. Engine power comes from `IB1EngineLoadSource` (the movement component); extra loads from `IB1FuelLoadSource` components, each a name + load in [0, 1]. `UB1FuelConsumptionData` (was `UB1VehicleTuningData`, CoreRedirect) holds `IdleRate`, `WorkRate`, `DrainInterval` and `LoadRates` (name → L/h at load 1; `Mud` = 300), so mud's rate sits with the other fuel rates and the mud component only reports strain. Out of fuel, the component broadcasts `OnOutOfFuelChanged`; the vehicle cuts its own throttle. `MudFullStrainDepth` moved to `UB1MudVehicleComponent` (S2-21). Rejected: the source returning L/h (rates would be split between fuel data and mud config).

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

---

## ADR-012 — Mud collision layering, sinking via suspension trace offset

**Status:** accepted (2026-10-25)

**Problem.** If the jeep "sinks" by lowering the wheel contact, the chassis body still collides with the same mud surface. It can never get below ground clearance, and that real collision fights the analytic belly drag. Lowering the wheel radius instead makes the jeep pull *harder* as it sinks (drive force = torque / radius).

**Decision.**
- A mud patch has two layers:
  - **Mud surface**: a separate mesh with the `B1MudSurface` collision profile and a `UB1PhysicalMaterial_Mud`. Only the wheel suspension traces hit it (`WheelTraceCollisionResponses`). The chassis and the character ignore it.
  - **Hard bottom**: the landscape or a mesh `MaxSinkDepth` below the surface. Everything collides with it.
- Sinking is applied in `FB1VehicleSimulation` by overriding `PerformSuspensionTraces`: on mud, the hit point is offset down by the wheel's `SinkDepth`. Wheel radius stays unchanged.
- Belly drag is analytic: when the average sink depth exceeds ground clearance, a drag force grows with `(SinkDepth - Clearance)`. The hard bottom is the backstop.

**Rationale.** It keeps the chassis free to sit "in" the mud and keeps drive force physically sane. It also keeps the effect local to the vehicle simulation.

**Alternatives.** Mud as a landscape paint layer with radius reduction (the radius side effect above, and no per-layer collision). A deformable heightfield (expensive; out of scope per ADR-008).

**Consequences.** Mud patches are authored as meshes on top of the landscape: a plane, a spline mesh, or a modelling-tools patch. The character walks through mud on the hard bottom; slowing the character in mud is a later polish item.

**Implementation (2026-10-25, S1-13).** `B1Mud` is `ECC_GameTraceChannel1` with default Ignore. `B1MudSurface` is QueryOnly and blocks only `WorldDynamic`, the channel the Chaos suspension trace queries. `UB1VehicleMovementComponent` (not the jeep) makes its wheel traces block `B1Mud`.

**Implementation (2026-10-25, S1-19).** The offset is done by raising each trace start by the sink offset, not by moving the hit afterwards. A sunk chassis would otherwise start the sweep inside or below the 10 cm mud slab. Chaos measures `Distance` from the real start, so the hit lands the offset deeper. A rise-only rate limit (30 cm/s) replaces the planned entry ramp: depth starts at 0 on entry and only changes at the mud rates.

**Implementation (2026-10-26, S1-21).** "Clearance" for belly drag is the axle height (`BellyClearance` 45 cm on the movement component), not the physics body's underside (60–64 cm measured). Axles and diffs plough mud first, and the body height would leave only the last 8 cm of deep for drag. The drag is applied at the centre of mass.

**Implementation (2026-10-26, S1-28).** Sink depth is capped by the hard ground under each wheel, found by an extra line trace that ignores `B1Mud`. The hard bottom no longer has to be flat: patches get optional edge ramps (puddle-like exits), and a landscape bottom shallower than `MaxSinkDepth` works the same way. Before this, a wheel kept its offset over a raised bottom and only the chassis collided with it.

---

## ADR-013 — Test rigs live in a separate `MudDriverTests` module

**Status:** accepted (2026-10-25)

**Problem.** `AB1GymTestRig` derives from `AFunctionalTest`, which lives in the `FunctionalTesting` Developer module. Developer modules aren't available in Shipping, and a `UCLASS` can't be compiled out with `#if`, so the rig can't sit in the `MudDriver` runtime module.

**Decision.** A second project module, `MudDriverTests` (type `DeveloperTool`), holds the gym test rigs and depends on `MudDriver` and `FunctionalTesting`. Symbols the tests need from the game module get `MUDDRIVER_API`.

**Rationale.** Shipping builds stay clean without per-config hacks. Test code stays out of the game module.

**Alternatives.** `FunctionalTesting` as a non-Shipping dependency of `MudDriver` (Shipping wouldn't compile). Blueprint-only rigs (logic and metrics harder to maintain).

---

## ADR-014 — Abilities are granted by ability sets and activated by input tags

**Status:** accepted (2026-10-27, S4-02)

**Problem.** Phase 2 adds the first real abilities (enter/exit, carry the winch hook; the machete follows in Phase 4). Nothing grants abilities or binds input to them yet, and the player swaps between two pawns while keeping one ASC on the PlayerState (ADR-004).

**Decision.**
- `UB1AbilitySet` (data asset): abilities with an input tag each, plus effects to grant. Sets are granted on the PlayerState ASC once, not per pawn; activation is gated by `State.Avatar.OnFoot` / `State.Avatar.InVehicle` tags.
- `UB1InputConfig` (data asset): input action → ability input tag. `AB1PlayerController` forwards pressed/released for those actions to the ASC, which activates the abilities whose spec carries the tag.
- Continuous controls (throttle, steering, look, walk, winch pull) stay native input on the pawn.
- `AB1PlayerController` swaps the vehicle and on-foot mapping contexts on possession.

**Rationale.** Lyra's pattern, and the user wants Lyra practices. Granting once on the PlayerState fits the single ASC; tags keep on-foot abilities from firing in the jeep and the other way round. Data assets keep bindings editable without code.

**Alternatives.** Grant and remove abilities per pawn on possession (churn on every swap, and specs are lost). Activate by class from pawn input handlers (couples input code to ability classes). Lyra's full `ULyraInputComponent` / pawn data stack (more than a two-pawn prototype needs).

**Accepted 2026-10-27 (S4-02, built).**
- **Where sets come from.** `UB1GASSettings::AbilitySets` (Project Settings → B1 GAS), granted in `AB1PlayerState::BeginPlay`. There is no pawn data or experience, so the settings stand in for Lyra's pawn data.
- **Where input binds.** `UB1InputConfig` sits on the controller (`BP_B1PlayerController`), not per pawn, so ability keys work on both pawns. Its contexts (`IMC_B1Abilities`) stay in `DefaultMappingContexts`; only the vehicle and on-foot contexts swap (S4-04).
- **Immediate activation.** Pressed/released go straight to the ASC. Lyra's per-frame `ProcessAbilityInput` queue and activation policies (on spawn, while held) are left out until an ability needs them.

**Amended 2026-10-27 (S4-06).** One press activates only the specs whose `CanActivateAbility` passes before any of them activates. Exit and interact share `InputTag.Interact`: without this, the exit press swapped the avatar to on foot, and the same press then let interact pick the jeep door's "Enter". Lyra's frame queue would avoid it the same way.

---

## ADR-015 — Mud surface contract

**Status:** accepted (2026-10-27, S2-18)

**Problem.** Mud is only placed as `AB1MudPatch` rectangles today. Later it has to come from spline meshes, modelling-tools blobs and PCG, and the mud code must be copyable into another project. Nothing says what makes a primitive "mud", so every producer would have to copy the patch.

**Decision.** A mud surface is any primitive component that:
1. has the `B1MudSurface` collision profile (object type `B1Mud`, QueryOnly, blocks only `WorldDynamic`), so only the wheel suspension traces hit it (ADR-012);
2. reports a `UB1PhysicalMaterial_Mud` on the hit (phys material override or the mesh's material), which carries all tuning (ADR-002);
3. has something that blocks `WorldDynamic` below it within the material's `MaxSinkDepth`: the hard bottom.

Depth rule: mud sits over a depression. The sink depth is capped by the hard-ground trace (ADR-012, S1-28), so a surface lying flush on the landscape gives ~0 cm sink. To get depth, lower the ground under the surface (a hard-bottom mesh, a landscape dip, PCG lowering the terrain under its output) or raise the surface over it.

Producers: `AB1MudPatch` (surface slab + optional hard bottom and edge ramps) is one. Spline meshes, modelling-tools meshes and, later, PCG output (mud surface at terrain height plus the terrain lowered under it) are others. They only need the three points above; the vehicle side never asks who made the surface.

Visual ruts are optional and separate: a surface that wants them owns a trail receiver (S2-23); one without it is still mud for physics.

**Rationale.** The wheel side already works this way: it reads the trace hit's material and traces for the hard ground. Writing it down lets new producers skip `AB1MudPatch` and keeps the vehicle code producer-agnostic.

**Alternatives.** A mud volume or actor registry the vehicle queries (extra overlaps, ADR-002 already rejected volumes). Requiring every producer to derive from `AB1MudPatch` (rectangles only, no PCG).

---

## ADR-016 — Chaos suspension anti-slide off

**Status:** accepted (2026-10-27, S3-04)

**Problem.** The Chaos suspension constraint has a built-in anti-slide (`PBDSuspensionConstraints.cpp`). On slopes up to 45° (`p.Chaos.Suspension.SlopeThreshold` 0.707) it pushes the suspension straight up instead of along the ground normal below 1 mph, and blends back up to 10 mph. That cancels gravity along the slope, so:
- a stopped jeep never slides, whatever the mud does (ADR-001 slopes need "barely sunk slides");
- a slow climb gets free help against gravity. In the S3-02 baseline, medium mud climbed 20° at 50 % throttle.

The CVar is global; there's no per-vehicle or per-surface switch.

**Decision.** `p.Chaos.Suspension.SlopeThreshold=1.0` in `DefaultEngine.ini` `[ConsoleVariables]`, which turns the anti-slide off everywhere. On firm ground a stopped jeep is still held by engine braking (Chaos's brake model holds a near-stopped wheel) and tire grip. On muddy slopes the mud sim releases that brake when there's no input (ADR-001 amendment), and the slope hold decides.

**Rationale.** The anti-slide is an arcade crutch that hides gravity on slopes; the project wants physics. With it off, the slope rules do what the ADR-001 amendment describes: firm 50 % climbs 20°, medium fails at 30/50/100 %, medium idle slides at ~0.5 m/s, deep and dug-in jeeps hold.

**Consequences.**
- Firm hills are harder at low throttle: firm 50 % no longer climbs 25° or 30°, so the winch gym slope is 20°.
- `Lvl_Offroad` rigs still pass. `B1Gym_FullThrottle5s` 85.3 → 89.0 m. `B1Step_Push20/120` drift drops 7.6 → 0.7 m, and 20 vs 120 fps still agree (0.74 / 0.71).
- Flat mud rigs are unchanged.

**Alternatives.**
- Keep it on and fake the slide with a mud force (fights the constraint every step; still no slide below 1 mph).
- Per-material slope grip factor (ADR-001 fallback): can't overcome the anti-slide either.
- Toggle the CVar at runtime when the player is on mud (global, so it would flip every vehicle).

---

## ADR-017 — Interaction options + one interact ability

**Status:** accepted (2026-10-27, S4-11)

**Problem.** On foot, one key has to do different things: enter the jeep at the door, take or return the hook at the jeep front, attach or detach at an anchor. S4-06/07/08 would each need the same "what's in range, which one wins" logic, and the prompt (S4-08) must never disagree with what the key does.

**Decision.**
- `IB1Interactable::GatherInteractionOptions(Character, Options)`, on an actor or any of its components. `FB1InteractionOption`: text, trigger event tag, priority, interaction point (Lyra's `IInteractableTarget` / `FInteractionOption`, minus the per-option granted ability).
- `UB1InteractionScanComponent` on `AB1Character`: sphere overlap (`Range` 250 cm, all object types) every `ScanInterval` (0.1 s) while player-controlled and not seated. Options whose point is past the range are dropped; the rest sort by priority, then distance to the point.
- `GA_B1Interact` (`UB1GameplayAbility_Interact`, `InputTag.Interact`, requires `State.Avatar.OnFoot`): `CanActivateAbility` runs a fresh scan and fails with nothing in range; activation sends the best option's event to the player ASC (target = the option's actor, instigator = the character). The option's ability triggers from it (`AbilityTriggers`, GameplayEvent).
- Input `IA_B1Interact` in `IMC_B1Abilities`: F and pad X (user, S4-06 check: pad X was removed from vehicle throttle, RT throttles). The same key exits the jeep (`GA_B1ExitVehicle`, S4-06).

**Rationale.** Lyra's pattern; one ability owns "which option", and each action stays a small ability triggered by its event. Abilities are granted once on the PlayerState ASC (ADR-014), so Lyra's per-option ability grant isn't needed. The prompt reads the same scan.

**Alternatives.** Each interactable grants its own ability on overlap (Lyra's full version; grant churn on a single ASC). Per-feature input checks in each ability (duplicated range logic, prompt can drift). Line trace from the camera (fiddly for a big jeep and a thin tree; can be added as a tie-breaker later).

**Verified.** `B1OnFoot_Interact` (`Lvl_Gym_Core`): walk up to `AB1TestInteractable`, press: interact + `GA_B1TestPing` (abilities = 2); walk 8 m away, press: nothing activates.

---

## ADR-018 — Fallen log: frozen until the cable breaks it free

**Status:** accepted (2026-10-28, S4-18 rigs pass; proposed in the fallen tree replan)

**Problem.** The S4-09 log was a free-simulating cylinder. The jeep rolled it by driving over it, and the winch dragged it easily. The fallen tree (VISION.md) must not budge when rammed, hooked or not. It should move only when the cable pulls it hard enough, and then by physics, not along a path. Where it's hooked must matter: near an end it swings open like a door, at the middle it barely moves.

**Decision.**
- Two states on `AB1WinchLog`. **Frozen**: no simulation, a kinematic body the jeep can't move. **Free**: simulating.
- Frozen → free when the hooked winch's published tension ≥ `BreakawayTension`. Free → frozen when tension is below it and the log has been at rest (< 5 cm/s, < 2 °/s) for `RefreezeDelay`. The switch runs on the game thread from the log's tick; the cable force on a free log stays in the vehicle sim's same-step pass (ADR-007).
- A frozen log is a static anchor to the winch, so without the handbrake hold it pulls the jeep in, like a tree. `BreakawayTension` ≥ 1.2 × the measured tension of a braked jeep being dragged, so freeing the log takes the handbrake.
- Body: an unscaled simulating box as the root (the flat-sided no-roll collision, with a clean body frame). The round trunk visual is a child with no collision. The root ball is a welded child primitive, and the compound mass puts the COM toward the root. High-friction physical material. The anchor attaches to the box root.
- **Physics first (user).** Once free, only Chaos moves the log. No hook-position rules, no scripted motion, no authored forces. Where to hook comes out of friction and torque: turning about the root end takes about half the force at the tip that it takes at the middle. So near the tip the cap can swing it, and at the middle it probably can't, which is the intended puzzle. Tuning touches only physical knobs (mass, friction, root ball). An authored root-end drag or standstill term needs the user's OK.
- The log draws its own state (`ShowDebug B1Log`) and logs each switch with the tension on `LogB1Winch`.

**Built (S4-17).**
- `SetSimulatePhysics` only flips the kinematic flag on the same physics actor (`FBodyInstance::UpdateInstanceSimulatePhysics` → `SetIsKinematic_AssumesLocked`). So the proxy, pose and winch handle survive each switch.
- Kinematic → simulated runs `ApplyWeldOnChildren`, so the root ball welds in on the first freeing (free mass 3300 kg = trunk + root ball) and stays welded when refrozen.
- Switches log on `LogB1Physics` with the winch's lines; there is no `LogB1Winch`.
- `B1Winch_Log` was retired.

**Rationale.** Ramming immunity costs nothing: a kinematic body is a wall to Chaos. The motion stays physical once it's free; a player who struggles beats a fake that always succeeds (user). No new physics-thread state, and a one-frame lag on a threshold doesn't matter. The unscaled root avoids the S4-09 scale bug class.

**Alternatives.**
- A stump hinge (the root end pinned to its stump; the most predictable "door", but the root can never slide).
- Always simulated and heavy (ramming still nudges it, and its mass fights the winch and the jeep's grip).
- Our own planar solver (full control, but we'd own its collision with the world).
- Flipping the state on the physics thread inside `ApplyDeferredForces` (no lag, but it changes a body the game thread owns).

---

## ADR-019 — Winch anchors are areas; the hook is placed by aiming

**Status:** accepted (2026-10-28, S4-16: `B1OnFoot_HookAim` hooks at 1.200 m aiming at 1.2 m, clamps to 1.600 m aiming at 3 m)

**Built (S4-16).** The alpha always comes from the closest point between the view ray and the axis, so the hook lands where the view points at the axis. Projecting a surface hit read a few cm high when looking down at a trunk. A trunk the ray actually hits only decides which anchor wins, and otherwise the anchor with the smallest ray-to-point distance does. The ray is the pawn's control rotation from the camera location (the camera transform only follows the control rotation when the view updates). The aim range is the interaction scan's `Range`, so the ghost and the prompt agree.

**Problem.** Anchors were single points: an 80 cm point on a tree, an end cap on the log. The cable hung off the side of the trunk, and the player couldn't choose where to hook, which the fallen tree needs.

**Decision.**
- `UB1WinchAnchorComponent` describes an area: a segment on the trunk axis (`AreaStart` / `AreaEnd`) plus `TrunkRadius`. A hook position is an alpha along it.
- The sim pulls at the axis point, so there's no roll torque from hooking on the side. The visual cable ends on a strap ring around the trunk, on the side facing the winch. The cable's particle sweeps also block PhysicsBody, so a cable across a log rests on it.
- `UB1HookAimComponent` on the character, ticking only while carrying a hook. The camera ray picks the point: a trunk hit is projected onto the segment, otherwise the closest point between the ray and the segment, clamped to the area. It stores the result on the carried winch (`SetAimTarget(Anchor, Alpha)`), so the anchor's "Attach" option, `GA_B1AttachHook` and the debug read it through `FindCarriedBy`, keeping one owner of hook state (ADR-007). A yellow translucent ghost ring shows it. The prompt and the attach agree (ADR-017). On attach the ring becomes the solid black strap.

**Rationale.** Gives the player free placement (The Alters-style ghost) with the same interaction scan and abilities. The axis point keeps the force clean. One component serves trees and logs.

**Alternatives.**
- Several fixed hook points per anchor (the user explicitly didn't want this).
- A GAS targeting actor (`WaitTargetData` + world reticle; heavier, and the carry isn't a long-running ability today).
- Hooking at the trunk surface point (adds torque that rolls a log).

---

## ADR-020 — Vehicle looks: pawn BP variants on the shared Offroad skeleton

**Status:** accepted (2026-11-07)

**Problem.** The player jeep should look like a Jeep Wrangler (Sketchfab FBX), keep the template buggy available, and keep working with the existing Chaos setup, AnimBP wheel controller, mud, fuel and winch. The Sketchfab model is a static part hierarchy with no bones, and the Unreal MCP can't set an AnimBP's target skeleton.

**Decision.**
- Each look is a child BP of `BP_B1Jeep` (`BP_B1Jeep_Wrangler`) that overrides only visuals and geometry-bound values: mesh, AnimBP, chassis/tire meshes, wheel classes (radius/width), COM, winch point, cameras. The game modes and controller pick the look (`DefaultPawnClass`, `VehiclePawnClass`).
- Vehicle meshes are generated offline (`Scripts/VehicleImport`): SM body + one centred SM tire + a small SKM whose bones are a subset of `SKM_Offroad_Skeleton` (`OffroadCar`, `PhysWheel_*`) with `VisWheel_*` mesh sockets on the wheel bones. The SKM shares that skeleton, and the AnimBP is a copy of `Offroad_AnimBP` without the suspension ControlRig.

**Rationale.** Gameplay stays in `AB1Jeep` / `BP_B1Jeep`, so a look swap changes no code and no tuning beyond what the geometry dictates. Sharing a skeleton is standard UE and is the only MCP-reachable way to get a working AnimBP. Sockets let the C++ tire components (attached to `VisWheel_*`) follow the wheel bones without touching code.

**Alternatives.**
- Rigid-hierarchy skeletal import of the raw FBX (no control over bone names or pivots; wheels would pivot at the origin).
- Overlaying the Wrangler on the buggy's skeleton without moving the wheels (buggy track 2.3 m doesn't fit a jeep body).
- A native wheel-visual component instead of an AnimBP (new C++ for something the template already does).

**Consequences.** `SKM_Offroad_Skeleton` is now shared; removing the template car means moving the skeleton first. The Wrangler mesh carries the skeleton's unused suspension bones at their Offroad ref pose. Gym rigs still spawn `BP_B1Jeep`, so tests keep the buggy's geometry.

---

## ADR-021 — Mud on the vehicle is a material layer, not decals

**Status:** accepted (2026-11-07, user choice; plan: Phase V Visuals)

**Problem.** The jeep should start clean and collect mud while it drives in mud: tires first, then the arches, then splatter up the body, capped at the concept look (`Concept/jeep_wrangler_mud_reference.jpg`).

**Decision.**
- **The dirt look** is a material function, `MF_B1MudDirt` (`/Game/B1/Mud/Dirt/`), called by the vehicle's base material (`M_Wrangler_Base`). It's a two-scale triplanar mask in the mesh's local space. Its reveal threshold falls as dirt grows, weighted by region: tires, then arches and sills, then doors and bumpers, then hood, roof and glass.
- **Values** are 0..1. Custom Primitive Data carries them, written by the portable `UB1MudDirtComponent` (`Mud/`):

  | Slot | Body | Tire |
  |---|---|---|
  | 0 | `BodyDirt` | its own wheel's dirt (crust) |
  | 1 | `WheelDirt[0]` | tire coat (amendment 2026-11-08) |
  | 2 | `WheelDirt[1]` | tire wetness (amendment 2026-11-08) |
  | 3–4 | `WheelDirt[2..3]` | unused |

  A static switch on the MI tells tire from body.
- **Targets** are components tagged `B1MudDirt`, matched to wheels by position at BeginPlay. A tagged component near no wheel is the body.
- **Dirt only grows** and caps at 1. Both dirt and the spray rate come from one helper (`B1MudThrow`), which computes thrown-mud intensity from sink depth, tread speed and slip. Dirt doesn't read the spray component.

**Rationale.** Deferred decals on a moving car also project onto the ground and through the body, smear at panel edges, and can't follow a rotating tire. A material layer has none of those problems. CPD needs no dynamic material instances and keeps the meshes batching. The material function and the tag keep the look and the logic portable: a host only calls the function and tags its parts.

**Alternatives.**
- Decal components attached around the arches and doors (projection artifacts, nothing on the tires).
- A UV-space render target painted by particle hits (needs unique UVs, much more work).
- Dynamic MIs per component (more draw-state churn than CPD for the same result).
- Dirt driven by the spray's particle rate (couples two visuals: spray off would mean no dirt).

**Consequences.** Every material on a dirt-capable vehicle has to call `MF_B1MudDirt`. CPD slots 0–4 are reserved on tagged components (slot 5 is held for a future `Wetness`). Nothing washes off yet; Phase 3 water can add a decrease path.

**Amendment (2026-11-08, user feedback at checkpoint 1).**
- **Tire mud layer:** tires get a second, fast layer on top of the slow crust. CPD on tires: slot 1 = coat (0.4 = tread covered after one wheel rotation in mud, 1 = thick, reaching the rim), slot 2 = wetness (darker when wet). Unlike the crust, the coat sheds on firm ground and the wetness dries, so tire values can go down. Body slots are unchanged. Runtime in S5-20.
- **Cap:** `DirtScale` (0.8) in the function scales the body and crust values, so the first look's 0.8 is the new 1.
- **Clean:** a `B1DirtClean` cheat resets everything (S5-11). Gameplay still has no wash path.
- Rims on the tire mesh need a tire material (`MI_Wrangler_Rim`, `bIsTire` on), or they run the body branch and read the tire slots as wheel dirt.

**As built (S5-18 check, 2026-11-08).**
- Targets resolve in the first tick that has wheels, not at BeginPlay (wheels come with the physics state). The tire match is "within the wheel radius of that wheel's centre".
- Accumulation never lowers dirt; `SetDirt` / `Clean` (debug) can. The tire coat and wetness go down on their own (amendment above).
- Glass calls the function too (`M_Wrangler_Glass`, film from slot 0), so "every material calls `MF_B1MudDirt`" holds for body, tires, rims and glass.
- Slot 5 is still free; body wetness stays a stretch.
- The CPD writes go to the runtime copy (`SetCustomPrimitiveDataFloat`), so `PreviewDirt` never dirties a level or BP.

---

## ADR-022 — Mud on the lens is a camera post-process driven by the spray's throw

**Status:** accepted (2026-11-08, user request; plan: [plans/2026-11-08-mud-lens.md](plans/2026-11-08-mud-lens.md))

**Problem.** Hard wheelspin should splash the chase camera's lens. The mud builds up, fades, and caps at `Concept/mud-on-the-screen.jpg`: small dark specks plus muddy drops that refract the scene, with the HUD clean on top. The user asked for a cheap trigger tied to the big spray, not a simulation of drops hitting the camera, and for a generous aim (splashed more often than not).

**Decision.**
- **Look:** one post-process material (`M_B1LensMud`, after tonemapping) added as a MID to the `WeightedBlendables` of camera components tagged `B1MudLens`. Only the active tagged camera shows and gathers mud. The host tags `BackCamera`; the in-cabin camera isn't tagged (the windshield has glass dirt, ADR-021).
- **State:** 4 splash slots (strength, screen offset, 1 of 8 rotation/mirror variants). A splash fills an empty slot or tops up the weakest. Each slot holds, then fades linearly. `LensDirt` = mean strength, 1 = the concept. Generated textures carry per-drop and per-speck reveal ids, so a slot's strength reveals that fraction of its drops and specks, and the drops shrink as it fades.
- **Trigger:** the portable `UB1MudLensComponent` (`Mud/`) reads each wheel's launch origin and velocity from `UB1MudSprayComponent`. Exposure = throw intensity × a launch-speed band × a wide aim window × distance falloff, max over the wheels. Accumulation is set in seconds (`LensSecondsToFull`). Random placement uses a seeded stream.

**Rationale.** A post-process pass is the only cheap way to get refraction, and it costs one full-screen pass (fits ADR-011). After tonemapping keeps the drops out of TSR and bloom and leaves the UMG HUD clean. Slots give variety (no same-drops-every-time) without a render target. The speed band follows the visible spray: a rolling throw (~250 cm/s) doesn't reach the lens, medium wheelspin (~510 cm/s) does, and retuning the spray moves the trigger the right way. Tagging cameras keeps the component portable and free of controller or camera-manager code.

**Alternatives.**
- UMG overlay with drop images (no refraction).
- Niagara sprites on the camera's near plane (more cost, harder fade/accumulation control).
- A render target painted by splash stamps or real particle hits (true placement, more work, needs a decay pass).
- A ballistic reach test (safety parabola) as the gate: physically honest, but at today's spray speeds medium wheelspin can't reach a camera 5.5 m back, so it would never fire.
- A `UCameraModifier` on the player camera manager (needs a host-side camera manager hook; the tag is simpler).

**As built (S5-28, 2026-11-08).**
- **Look:** drops only. The concept's dark specks were an image artifact (user, lens checkpoint round 1), so `T_B1LensSpecks` and the speck path are gone. Drops are translucent, blurred and irregular, with brown towards their lower edge. They're more and larger at the bottom of the screen (`LensBottomScale` 1.2, `LensTopDensity` 0.45). The reveal is front-loaded (`LensRevealGamma` 0.5), so the first splashes show medium drops. Final sheet: `Saved/MudTextures/lens/stills4/lens_sheet.png`.
- **Trigger:** measured throw peaks (S5-27, `s527_measure.log`): rolling 150–291 cm/s, firm 0; wheelspin 547–555 (buggy, medium), 675 (Wrangler, medium), 900 (deep). The plan's band `LensMinThrowSpeed` 350 / `LensFullThrowSpeed` 500 sits in the 291–547 gap and stays. The rationale's ~510 cm/s guess for medium wheelspin was low.
- **Rates:** with the defaults (threshold 0.1, interval 0.25 s, 4 s to full, hold 1.5 s, fade 6 s), 4 s of Wrangler wheelspin in medium gives exposure 0.68, 14 splashes and `LensDirt` 0.52. It clears in ~4.5 s once the spin stops (`B1Lens_Fade` ends at 0 after 10 s idle). The cap holds at 1.
- **Camera:** an unpossessed jeep's `BackCamera` is active, so rigs measure the lens without a controller. The camera PP blendable applies from the next `GetCameraView`; no refresh needed.
- **Debug:** the Mud window "Lens" section, cheats `B1Lens <v>` / `B1LensSplash`, telemetry `lens=d/exp`, ShowDebug `lens d exp slots`, CVar `b1.Mud.Lens`. Rigs: `B1Lens_Firm`, `_Roll`, `_Spin`, `_Cap`, `_Fade` (`Lvl_Gym_Mud`, `B1Tests/Lens`).

**Consequences.** The lens reads the spray on purpose: `b1.Mud.Spray 0` means a clean lens. That's unlike dirt (ADR-021), which stays independent of the spray. A front wheel's throw counts through the body (no occlusion). Spray retuning shifts the trigger, so the lens rigs are re-run after spray changes. Phase 3 water can reuse the slots for lens water.
