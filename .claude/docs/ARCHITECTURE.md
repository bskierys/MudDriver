# ARCHITECTURE

The source of truth for what exists *today*. Planned systems live in [PLAN.md](PLAN.md) until they are built. Update via `/save`.

## Modules

UE 5.8, engine at `C:\Program Files\UE_5.8`.

| Module | Type | Deps | Holds |
|---|---|---|---|
| `MudDriver` | Runtime | Core, CoreUObject, Engine, InputCore, EnhancedInput, ChaosVehicles, PhysicsCore, UMG, Slate, NetImgui, GameplayAbilities, GameplayTags, GameplayTasks, DeveloperSettings; private Projects, ChaosVehiclesCore, Niagara, ProceduralMeshComponent | Game code |
| `MudDriverTests` | DeveloperTool (not in Shipping, ADR-013) | Core, CoreUObject, Engine, FunctionalTesting; private ChaosVehicles, MudDriver | Gym test rigs |

Game-module symbols used by the tests need `MUDDRIVER_API` (the log categories already have it).

Plugins enabled: ChaosVehiclesPlugin, GameplayAbilities, NetImgui (project plugin), ModelContextProtocol + editor toolsets, GameInputWindows, ModelingToolsEditorMode.

Log categories: `LogB1Vehicle`, `LogB1Physics`, `LogB1Test` (`B1Log.h`, project code); `LogB1Mud` (`Mud/B1MudLog.h`, all mud code and the `B1Mud` telemetry line); `LogB1Fuel` (`Fuel/B1FuelLog.h`). <!-- updated 2026-10-27 -->

Source layout: `Mud/` (runtime), `Mud/Sim/` (physics-thread mud), `Mud/Debug/` (`B1MudShowDebug`, `B1ImGuiMud`), `Fuel/`, `Vehicle/`, `GAS/`, `Player/`, `Debug/`. `Mud/` and `Fuel/` are portable: rules in [portability.md](../rules/portability.md), copy steps in [MIGRATION.md](MIGRATION.md). <!-- updated 2026-10-27 -->

## Code

| Area | Classes | Notes |
|---|---|---|
| Vehicle pawn | `AMudDriverPawn` (abstract, `AWheeledVehiclePawn`) | Template: input, front/back cameras, flip check + auto-reset. Constructors take `FObjectInitializer` down the chain. Owns the telemetry timer and the ShowDebug binding |
| Offroad jeep (template) | `AMudDriverOffroadCar`, `UMudDriverOffroadWheelFront/Rear` | AWD, 600 Nm, wheel radius 50, shapecast suspension. BP: `BP_OffroadCar_Pawn` |
| B1 jeep | `AB1Jeep : AMudDriverOffroadCar` (abstract) | Swaps in `UB1VehicleMovementComponent` via `SetDefaultSubobjectClass`. BP: `/Game/B1/Vehicles/BP_B1Jeep` (duplicate of the template BP, 0 CDO diffs). Default pawn of `BP_OffRoadGameMode` |
| Sports car | `AMudDriverSportsCar` + wheels | Template leftover, unused for the game |
| Controller | `AMudDriverPlayerController` → `AB1PlayerController` (`Player/`, abstract) | Template: mapping contexts, respawn, vehicle UI, sets `UB1CheatManager`; accepts any pawn, binds respawn only for vehicles, hides the vehicle UI on foot. B1: `OnPossess` calls `InitAbilityActorInfo(PlayerState, pawn)` and sets the loose `State.Avatar.InVehicle` count (1 for `AWheeledVehiclePawn`, else 0). BP: `/Game/B1/Player/BP_B1PlayerController` (respawns `BP_B1Jeep`). HUD: `/Game/B1/UI/WBP_B1VehicleHUD` (copy of the template's `BP_VehicleAdvUI`, `UMudDriverUI`) adds a fuel bar + `12/20 L` text between gear and speed; `AB1PlayerController::Tick` calls `UpdateFuel` from the ASC; the fill goes green → red at 15 % <!-- updated 2026-10-27 --> |
| Game modes | `AMudDriverGameMode`, `AOffroadGameMode`, `ATimeTrialGameMode`, `AB1GameMode : AOffroadGameMode` | `AB1GameMode` sets `PlayerStateClass = AB1PlayerState`. BP `/Game/B1/Player/BP_B1GameMode` (B1 controller + `BP_B1Jeep`): `Lvl_Offroad`'s game mode and parent of `BP_B1GymGameMode` <!-- updated 2026-10-26 --> |
| Player state (ADR-004) | `AB1PlayerState` (`Player/`) | Owns the single `UB1AbilitySystemComponent` (Mixed) and `UB1VehicleAttributeSet` as default subobjects. The jeep (`AB1Jeep`) implements `IAbilitySystemInterface` and keeps a weak pointer to this ASC from `PossessedBy`; it isn't cleared on unpossess, so a parked jeep still reports its status <!-- updated 2026-10-26 --> |
| GAS (`GAS/`) | `UB1AbilitySystemComponent`, `UB1VehicleAttributeSet`, `B1GameplayTags`, `UB1GASSettings` | Attributes: Condition/MaxCondition (100), Fuel/MaxFuel (20 L, small so consumption shows; was 60), meta `Damage` → subtracted from Condition in `PostGameplayEffectExecute`; Condition/Fuel clamped to [0, Max]. No replication (single-player). Native tags in namespace `B1GameplayTags`: `State.Avatar.InVehicle/OnFoot`, `Vehicle.State.InMud/OutOfFuel`, `Data.Fuel`, `Data.Damage` (SetByCaller). `Vehicle.State.InMud` is a loose tag set by `UB1MudVehicleComponent` (its `InMudTag`, set in `AB1Jeep`'s constructor) while the pawn is controlled and has an ASC: any wheel on mud (`GetWheelMud`), released `MudTagReleaseDelay` (0.5 s) after the last contact, cleared when the controller changes and in EndPlay <!-- updated 2026-10-27 --> |
| Fuel (ADR-010) | `UB1FuelConsumptionComponent`, `UB1FuelConsumptionData`, `IB1EngineLoadSource`, `IB1FuelLoadSource` (`Fuel/`) | `FuelConsumption` on `AB1Jeep`. While the pawn is controlled and has an ASC, accumulates `IdleRate + WorkRate·EnginePower + Σ LoadRates[name]·load` (L/h) and applies one `DrainEffect` (SetByCaller `DrainSetByCallerTag`, negative litres) per `DrainInterval`; L/100 m over a 5 s window. Engine power from `IB1EngineLoadSource` (movement: throttle² × torque(RPM) × RPM over the curve's peak, rebuilt on the game thread since 5.8 exposes no engine torque there). Loads from `IB1FuelLoadSource` components: the mud component reports `Mud` = mud strain (effective throttle × clamp(average sink / `MudFullStrainDepth`)). `DA_B1JeepTuning` (`UB1FuelConsumptionData`, CoreRedirect from `UB1VehicleTuningData`): 6 / 240 L/h, `Mud` 300 L/h, 0.25 s. Fuel attribute + tags set in `AB1Jeep`'s constructor, `Data` + `DrainEffect` on `BP_B1Jeep`. Binds the fuel change delegate on the driver's ASC lazily (unbound on controller change); at 0 sets `OutOfFuelTag` and broadcasts `OnOutOfFuelChanged`, which `AB1Jeep` turns into the movement's fuel cut-off (`ThrottleInput` = 0 before `Super::Update`, so reverse is blocked too) <!-- updated 2026-10-27 --> |
| Gameplay effects | `/Game/B1/GAS/GE_B1Damage_Impact`, `GE_B1FuelDrain`, `GE_B1FuelAdd` | Instant BP GEs with SetByCaller modifiers (`Data.Damage` → `Damage`, `Data.Fuel` → `Fuel`, signed litres). Classes come from `UB1GASSettings` (Project Settings → B1 GAS, `DefaultGame.ini`). ASC helpers: `ApplySetByCallerEffectToSelf`, `ApplyVehicleDamage`, `SetVehicleCondition`, `AddVehicleFuel` (picks add/drain by sign) <!-- updated 2026-10-26 --> |

Maps: `VehicleTemplate/Maps/VehicleBasic`, `Variant_OffRoad/Maps/Lvl_Offroad` (World Partition, one file per actor), `Variant_TimeTrial/Maps/*`.
Physical materials: `Vehicles/PhysicsMaterials/Slippery`, `NonSlippery`.

## Vehicle simulation (ADR-003)

- `UB1VehicleMovementComponent : UChaosWheeledVehicleMovementComponent` overrides `CreatePhysicsVehicle()` to install `FB1VehicleSimulation` before calling the base.
- `FB1VehicleSimulation : UChaosWheeledVehicleSimulation` runs on the physics thread (`UpdateSimulation`). It never touches UObjects. It applies only the test force itself and owns an `FB1MudSimulation` (`Mud/Sim/`, composition), calling its hooks: `BeginStep` before and `EndStep` after the base `UpdateSimulation`, `Pre/PostSuspensionTraces` around the base traces (with a copy of the traces the mud raises), `Pre/PostFriction` around the base `ApplyWheelFrictionForces`. The mud sim reaches the host's public `PVehicle`, `WheelState`, `VehicleState`, `World` and `AddForce*` through the `UChaosWheeledVehicleSimulation&` it's given. <!-- updated 2026-10-27 -->
- `B1MudWheelModel` (`Mud/Sim/`, plain C++, no Chaos or UObjects): sink depth update, sink offset rise limit, relative load, friction scale, rolling resistance coefficient, belly drag limit, stop force and the no-reversal cap. `FB1MudSimulation` does the Chaos plumbing around it. <!-- updated 2026-10-27 -->
- **Game → physics data:** `SetCurrentAsyncInputOutput` is non-virtual and always allocates the base async structs, so custom input can't ride the Chaos async input. Each side has a `FCriticalSection`-guarded latest-value buffer: the movement component's `Update` pushes `FB1VehicleSimInput` (test force) into `FB1VehicleSimulation`; `UB1MudVehicleComponent` pushes `FB1MudSimInput` (mud params, belly clearance, sink reset serial, unstuck assist force) into `FB1MudSimulation`.
- **Physics → game data:** `FB1MudSimulation` copies its per-step results (`FB1MudSimOutput`) under a second lock; the mud component reads them.
- **Mud component timing:** `UB1MudVehicleComponent` has no tick for this. It binds `FPhysScene_Chaos::OnPhysSceneStep`, broadcast on the game thread right after `OnPhysScenePreTick`, where the Chaos vehicle manager refreshes `WheelStatus` and runs `Update`. A PrePhysics tick or a PreTick binding (multicast delegates broadcast in reverse order) runs before that refresh, learns new mud a frame late and changed the mud rigs by up to 5 %. It reaches the movement component through `IB1MudSimHost` (`Mud/B1MudSimHost.h`). <!-- updated 2026-10-27 -->
- **One-shot commands** ride the input as a counter, not a bool, because the input is latest-wins and a bool can be overwritten before a step reads it. `SinkResetSerial`: the sim zeroes `SinkDepth`/`SinkOffset` once per new value and echoes the applied serial in its output. <!-- updated 2026-10-26 -->
- **Unstuck (`B1Unstuck`, `UB1MudVehicleComponent::Unstuck`):** object trace on the mud channel ±3 m, teleport level (yaw kept) so the lowest wheel bottom is 50 cm above the surface, zero velocity, bump `SinkResetSerial`. For `UnstuckAssistDuration` (4 s) the sim adds `UnstuckAssistForce` (15 kN) × raw drive input (`IB1MudSimHost::GetMudDriveInput`, throttle − brake) along the flattened heading, so the player can drive out before the jeep sinks back. Raw inputs, so the fuel cut-off doesn't block it. Logs to `LogB1Mud`. <!-- updated 2026-10-27 -->
- **Mud sinking (ADR-001):** per wheel on mud, `SinkDepth` (cm):
  - settles at `SettleRate` to a floor of `MaxSinkDepth·StaticSinkRatio·load`, at any speed;
  - digs below the floor at `SinkRate·spin·load`;
  - climbs back toward the floor at `RiseRate·rolled m·(1−spin)`, never above it;
  - is clamped to `MaxSinkDepth`.

  Here `spin = clamp(|slip speed| / FullSinkSlipSpeed)` and `load` = wheel load / average load of the wheels in contact, capped at 1.5. Non-mud contact resets the depth; airborne keeps it.
- **Sinking the vehicle (ADR-012):**
  - `PerformSuspensionTraces` traces from a copy of the traces with each start raised by the wheel's `SinkOffset`. Chaos reads `Distance` from the real start, so the hit is `SinkOffset` deeper; `Location`/`ImpactPoint` are pushed down to match, clamped to full droop.
  - Raising the start keeps the sweep above the thin mud slab when the chassis is low.
  - `SinkOffset` follows `SinkDepth` instantly going down and at most 30 cm/s going up, so leaving mud doesn't pop.
  - **Hard ground cap:** for a wheel whose hit is on mud, a line trace (`WorldDynamic`, wheel responses with `B1Mud` ignored) from 2 cm above the hit down to `MaxSinkDepth` finds the hard surface below. `HardGroundDepth` caps both `SinkOffset` (same step) and `SinkDepth` (in `UpdateSinkDepth`), so edge ramps and shallow landscape under a patch push the wheel back up. One extra line trace per mud wheel per step. <!-- updated 2026-10-26 -->
  - `UB1MudVehicleComponent` keeps `KnownMudMaterials` (every mud material its wheels have touched) and sends copies of their params (`FB1MudSimParams`). The sim matches `TraceResult.PhysMaterial` by weak-pointer equality, so new mud reads as firm for about a frame.
- **Traction and resistance (ADR-001):** `ApplyWheelFrictionForces` scales each mud wheel's `FrictionMultiplier` by `FrictionVsDepth` around the base call. After the call, it adds a rolling resistance force along the steered wheel forward at the contact point: (`RollingResistanceVsDepth` + `SpeedDragCoefficient`·depth·v²) × the wheel's weight share. Below 1 cm/s it opposes the tire's push, like static friction, and it's capped at the tire push plus 0.4 × the stopping force. `SinkOffset` rises at max(30 cm/s, 0.5 × wheel speed). The `B1Mud` telemetry shows `mu=` and `rr=`. <!-- updated 2026-10-26 -->
- **Belly drag (ADR-012):** after rolling resistance, `ApplyBellyDrag` takes the average `SinkDepth` of all wheels minus `BellyClearance` (`UB1MudVehicleComponent`, 45 cm = axle height; the body underside measures 60–64 cm, logged once per vehicle). Drag = mean `BellyDragCoefficient` of the mud wheels × excess (m) × weight, at the centre of mass against ground velocity, with the same standstill and no-reversal rules as rolling resistance. Only Heavy and Deep reach it. Telemetry `belly=<cm>/<N>`. <!-- updated 2026-10-26 -->
- **Slopes (ADR-001 amendment, S3-03/04):**
  - Standstill rule: rolling resistance and belly drag judge push from the net force along the ground. That's tire force + gravity along the ground (per wheel: weight share × −wheel-forward Z; belly: mass·g on the averaged hit-normal plane).
  - Slope pull (user design after the S3-06 playtest): muddy slopes are slippery. Every mud wheel on mud steeper than `KeepAwakeSlope` (3°) gets an extra force down the slope at its contact: `SlopePull` (`UB1MudVehicleComponent`, C++ default 0.5; `BP_B1Jeep` 0.3 after the user's playtest) × gravity along the ground for its weight share (`B1MudWheelModel::GetSlopePull`). It's always on, climbing, idling or reversing, and counts in the rolling-resistance net push. With the lower mud grip it makes muddy hills hard or impossible to climb. There's no hold: a dug-in jeep can still reverse out.
  - On such mud the vehicle stays awake (`IB1MudSimHost::SetMudKeepAwake` → `SleepSlopeLimit` 1). With no throttle, brake or handbrake, `PreFriction` zeroes the mud wheels' brake torque, because Chaos's engine braking (900 rpm × 0.2 = 180 Nm) would lock them. So an idle jeep rolls down the muddy slope (20° medium: 42 m to the bottom, up to 7 m/s).
  - Chaos's suspension anti-slide (`p.Chaos.Suspension.SlopeThreshold`, pushes straight up below 10 mph on slopes ≤ 45°) is off project-wide in `DefaultEngine.ini` (ADR-016).
  - Mud window: `Pull N` column, live `Slope pull` and slope angle. Telemetry `pull=`; rig `pull=` (peak sum). <!-- updated 2026-10-27 -->
- Forces from the sim use `AddForce` in kg·cm/s² (N × 100).
- **Test forces** (`EB1TestForce`: Off / Push / Spring, settings in `TestForceSettings`, category `TestForce|Config`): push = constant world +Y force; spring = one-sided spring-damper to an anchor captured at toggle time. While active, the component zeroes `SleepThreshold`, because a sleeping vehicle skips the simulation, and restores it on Off.
- Steps at a fixed 1/60 s (see Physics settings).

## Collision (ADR-012)

| Channel | Index | Default | Used by |
|---|---|---|---|
| `B1Mud` (object) | `ECC_GameTraceChannel1` (`UB1MudSettings::MudObjectChannel`) | Ignore | Mud surface meshes |

- `UB1MudSettings` (`Mud/B1MudSettings.h`, Project Settings → Game → B1 Mud, `DefaultGame.ini`): `MudObjectChannel`, `SurfaceProfileName` (`B1MudSurface`), `SpraySystem` (soft ref, default for the spray component). Code reads these; `ECC_GameTraceChannel1` appears only as the settings default. `FB1MudSimulation` copies the channel in its constructor (game thread). <!-- updated 2026-10-27 -->
- Profile `B1MudSurface`: object `B1Mud`, QueryOnly, blocks only `WorldDynamic`. The Chaos suspension trace queries `WorldDynamic` with `WheelTraceCollisionResponses`, so only wheel traces that block `B1Mud` hit the mud.
- `UB1VehicleMovementComponent::OnRegister` sets `WheelTraceCollisionResponses[MudObjectChannel] = Block` (not the constructor: the CDO can be built before the settings are read from config). <!-- updated 2026-10-27 --> The chassis (`Vehicle`), pawns, physics bodies and the camera pass through.
- Static mesh components must have `bUseDefaultCollision=false` (or use `SetCollisionProfileName`), or the mesh's own collision overrides the profile. <!-- updated 2026-10-25 -->

## Mud

**Surface contract (ADR-015).** Any primitive is a mud surface when it (1) has the `B1MudSurface` profile, (2) reports a `UB1PhysicalMaterial_Mud`, and (3) has something blocking `WorldDynamic` below it within `MaxSinkDepth`. Mud sits over a depression: a surface flush on the landscape gives ~0 cm sink, because the hard-ground cap finds the landscape right under it. `AB1MudPatch` is one producer; spline meshes, modelling-tools meshes and PCG output are others. <!-- updated 2026-10-27 -->

- `AB1MudPatch` (`Mud/B1MudPatch.h`) has these components:
  - `MudSurface`: a 10 cm slab, top at the actor origin, `B1MudSurface` profile, `MudMaterial` as its phys material override.
  - `HardBottom`: a 1 m BlockAll slab, top `HardBottomDepth` below the surface. It's hidden and NoCollision when `bHardBottom` is off, for patches where the landscape is the bottom.
  - `EdgeRamps` (+X, −X, +Y, −Y): 1 m BlockAll boxes pitched so the top face climbs from the hard bottom to the surface over `EdgeRampLength` (default 300 cm) at the patch edge. On per edge with `bRampPlusX/MinusX/PlusY/MinusY` (default off, so patches butt together); only with `bHardBottom`. They take the hard bottom's material. Corners are the union of two ramps.
  - `OnConstruction` sizes all of them from `Size` (cm).
- Gym strips (Shallow/Medium/Heavy/Deep) have ramps on all four edges, 300 cm. <!-- updated 2026-10-26 -->
- `/Game/B1/Mud/BP_B1MudPatch` is the placeable child, with `M_B1Mud` (a `Color` vector parameter for per-depth tints). Tints: `MI_B1Mud_Shallow/Medium/Deep`, set as the `MudSurface` material override per instance. <!-- updated 2026-10-25 -->

- `UB1PhysicalMaterial_Mud` (`Mud/B1PhysicalMaterial_Mud.h`, ADR-002) holds the mud tuning:
  - `MaxSinkDepth` (cm), `StaticSinkRatio` (weight-only depth as a fraction of max), `SettleRate` (cm/s), `SinkRate` (extra cm/s at full wheelspin), `FullSinkSlipSpeed` (m/s slip that counts as full wheelspin), `RiseRate` (cm per m rolled with grip);
  - `FrictionVsDepth` and `RollingResistanceVsDepth` (fraction of wheel load), both `FRuntimeFloatCurve` over normalized depth;
  - `BellyDragCoefficient` (fraction of vehicle weight per m past clearance).
  - Its base `Friction` is the grip at zero depth. Chaos feeds it to the tire through `SetSurfaceFriction`, so the depth curve multiplies on top.
  - `SprayColor` (`Mud|Spray`) colours the wheel spray; every preset uses the same dark brown since S2-28. <!-- updated 2026-10-27 -->

## Mud spray

- `UB1MudSprayComponent` (`Mud/`, on `AB1Jeep` as `MudSpray`) creates one `UNiagaraComponent` per wheel (lazily, once the wheels exist) with `SpraySystem`, or `UB1MudSettings::SpraySystem` (`/Game/B1/FX/NS_B1MudSpray`) when unset. It finds `UB1MudVehicleComponent` in BeginPlay and reads wheels through it. <!-- updated 2026-10-27 -->
- Each tick, for a wheel on mud (`GetWheelMud`), sink > 0 and |`SlipMagnitude`| > `MinSlipSpeed` (0.5 m/s): `User.SpawnRate` = 60/s per m/s of excess slip (max 400), `User.SprayVelocity` = 250–900 cm/s backward (sign of the wheel's drive torque) and 35° up, in component space; `User.Color` = the material's `SprayColor`. Otherwise the rate is 0.
- The emitter sits 20 cm above the visible mud surface (hit + sink depth, ADR-012) and behind the tread, so depth-buffer collision doesn't catch particles at birth.
- `NS_B1MudSpray`: one GPU emitter (Fountain template), fixed bounds, cone velocity from `SprayVelocity`, 0.4–0.9 s life, 10–28 cm sprites, gravity, drag, GPU depth-buffer collision with Kill Occluded off. `M_B1MudSpray`: masked, lit, base colour = particle colour, round cutout, a flattened dome normal from the sprite UV (Custom node `SphereNormal`), roughness 0.2, specular 0.6, so clumps look wet, not like smoke. All presets spray the same dark brown (0.13, 0.075, 0.035). <!-- updated 2026-10-27 -->
- CVar `b1.Mud.Spray`; the `B1Mud` telemetry line has `spray=` per wheel. <!-- updated 2026-10-26 -->

## Mud ruts (ADR-008, visual only)

- `AB1MudPatch` has two surfaces: `MudSurface` (the 10 cm slab, collision; visible in the editor, hidden in game) and `MudVisual`, a `UProceduralMeshComponent` grid (`VisualGridSpacing` 10 cm, ≤ 400 cells per side, UV0 spans the patch, no collision, no shadow) with `MudSurface`'s material. The grid is built in `BeginPlay` and cleared in `OnConstruction`: built in the editor, its sections were saved into each actor package (13–18 MB per patch, minutes per save). <!-- updated 2026-10-26 -->
- Rut receivers are `UB1MudTrailSurfaceComponent`s (`TrailSurface` on the patch; any mud surface can own one, ADR-015). In game worlds `Initialize(MudVisual, Size)` creates a 512² R16F render target (rut depth / 100 cm), puts it on a dynamic instance of the primitive's material (`TrailMask`, plus `TrailDepthRange`, `PatchSizeX/Y`), computes a world box (the rect, ±100 cm in Z) and registers with `UB1MudTrailSubsystem` (`UTickableWorldSubsystem`, Game/PIE). The subsystem tests each stamp against the box before the local-UV test and doesn't know about patches. <!-- updated 2026-10-27 -->
- `UB1MudTrailComponent` (on `AB1Jeep` as `MudTrail`, reads `UB1MudVehicleComponent`) sends each mud wheel's contact path with its sink depth; wheels under `MinRutDepth` (2 cm) leave nothing. Width = tire width × `WidthScale` (1.2) + depth × `WallWidthPerDepth` (1.5).
- The subsystem stamps every 5 cm and draws one canvas batch per patch per frame: a procedural soft brush (flat floor, falloff 0.3–1), translucent at 0.5 toward the stamp depth. The canvas has no max blend, so a shallower wheel partly fills an older rut. CVar `b1.Mud.Trails`.
- `M_B1Mud` (HLSL Custom nodes): height = −depth + lip (saturate(4-tap blur − depth) × `LipFactor` 0.35 at `LipRadiusCm` 30) drives World Position Offset along the vertex normal; a world-space normal comes from the height gradient (`TrailTexel` step); base colour darkens to ×0.6 and roughness drops to 0.4 over the first 8 cm of depth.
- Ruts never fade and don't touch physics. <!-- updated 2026-10-26 -->
- Presets (S/M/L/XL): `/Game/B1/Mud/PM_B1Mud_Shallow` (25 cm), `_Medium` (35), `_Heavy` (50), `_Deep` (70), set as each gym patch's `MudMaterial`. Tints `MI_B1Mud_*`. Gym lanes: Shallow Y 16, Medium Y 29, Heavy Y 42, Deep Y 55. <!-- updated 2026-10-26 -->

<!-- updated 2026-10-25 -->

## Mud gym (ADR-009)

`/Game/B1/Gym/Lvl_Gym_Mud` (WP/OFPA, game mode `BP_B1GymGameMode`, a child of `BP_OffRoadGameMode`).
- **Ground:** grid-material cube tiles, top at Z=0, floor X −50..120 m and Y −60..80 m.
- **Walls:** 3 m perimeter walls (`Gym/Walls`), hidden in game, Custom collision with Camera ignored.
- **Lanes** along +X, with the patch zone at X 10–30 m:
  - firm (Y 0);
  - shallow 15 cm (Y 16), medium 35 cm (Y 29), deep 70 cm (Y 42), each in a gap in the tiling with its own hard bottom;
  - pit (Y −19): 2 m deep, ramps in and out, mud 60 cm over the floor.
- **Spawns:** `Spawn_1..5` at X −12 m. `Spawn_1_Firm` carries the `Player0` tag, which `AOffroadGameMode` always picks.

<!-- updated 2026-10-25 -->

## Swamp playground (ADR-009 amendment)

`/Game/B1/Gym/Lvl_Gym_Swamp`: a duplicate of `Lvl_Gym_Mud` (same game mode and lighting), no rigs. Floor X −60..140 m, Y −80..80 m; hidden walls Z −4..8 m.
- **Ground:** grid cube tiles 4 m thick, top at Z=0 (`Gym/Ground`); mud patches sit in gaps with their own hard bottoms (S 25, M 35, H 50, D 70 cm).
- **Hard-bottom transitions:** a patch's own edge ramp is on only when the whole edge meets firm ground (S 150, M 200, H 300, D 400 cm). Otherwise hidden BlockAll slabs (`Gym/MudBottom`, `Bottom_<patch>_<edge><n>`) run per edge segment inside the deeper side: up to the surface next to firm ground, or up to the shallower neighbour's bottom (8 cm of run per cm of depth difference, 200–400 cm). A full-height ramp on an edge shared with mud made a bump under the surface.
- **Spawn:** `Spawn_1_Main` (`Player0`) at X −50 m, Y 0, on a firm area X −60..−35.
- **Swamp** (X −35..55): 27 patches, mostly shallow/medium with some heavy and deep. A 6 m firm path runs from the spawn east (Y 0) and jogs to the lake shore (Y 14..20), with branches north (X 0, then Y 50..56) and south to hill 2.
- **Lake** (X 60..120, Y −30..30): concentric rings, shallow 5 m → medium 7.5 m → heavy 7.5 m → deep 20 × 20 m centre.
- **Hill 1** (plateau X 80..100, Y 55..70, +4 m): west ramp 11°, south and east ramps 18°; the east ramp ends in medium mud (`Mud_HillFoot_M`). Everything from the lake's north shore (Y 30) to the wall around the hill is shallow mud (`Mud_Around_*`).
- **Hill 2** (plateau X −6..6, Y −80..−70, +2.5 m) in shallow mud, with a firm landing at the end of the south path: north ramp 12°, east 14°, west 23°.
- **Mud on slopes** (`Gym/Mud/Slope`): rotated patches, top surface flush with the ramp. Half the width is mud on hill 1 W (shallow), E (shallow), S (medium) and hill 2 N (medium), next to the firm half (the firm ramp slab only covers its half). Hill 2 E (shallow) and W (medium) are mud across the full width. Edge ramps on at the top edge and on the side next to the firm half; at the foot, the floor tile under the ramp caps the sink. No slope rules yet (Sprint 3).
- **Basin** (X 75..105, Y −78..−40, floor −3 m, `Gym/Basin`): medium and heavy mud at −3 m; west and east ramps 11°, north ramp 21°.
- Layout is generated: rectangles in metres, firm tiles fill what's left. Edit `Gym/` actors by hand for small changes. <!-- updated 2026-10-26 -->

## Winch gym (Sprint 3)

`/Game/B1/Gym/Lvl_Gym_Winch`: a copy of `Lvl_Gym_Mud`'s base (walls, lighting, `BP_B1GymGameMode`). Floor `G_Floor`, one tile, top Z=0, X −50..120 m, Y −60..80 m.
- **Slope lanes** (`Gym/Slope`, S3-05): θ = 20°, 40 m along the slope, foot at X 20 m, top edge at X 57.59 m / Z 13.68 m. Lanes 10 m wide:
  - firm slab `Slope_Firm` (Y 0);
  - medium patch `Mud_Slope_Medium` (Y 20, hard bottom 35 cm);
  - deep patch `Mud_Slope_Deep` (Y 40, hard bottom 70 cm, `MI_B1Mud_Deep`).
  - Mud patches ramp only at the foot (300 / 400 cm). `Slope_Plateau` (10 m, Y −5..45) ends at a 1.5 m `Slope_Kerb`. Flat run-ups from the spawns.
- **Anchors** (`Gym/Anchors`): `Anchor_Medium`, `Anchor_Deep`, 1 × 2 m posts on the plateau 8 m past each mud top (X 65.59 m, Y 20 / 40). S3-14 swaps in trees.
- **Spawns:** `Spawn_1_FirmSlope` (`Player0`), `Spawn_2_MudSlope`, `Spawn_3_DeepSlope` at X −10 m, Y 0 / 20 / 40; labels at X 6 m.
- **Angle:** 20° is the steepest where firm 50 % still climbs with the Chaos anti-slide off (25° and 30° roll back). At 30 m the medium 100 % run carried over the top on run-up momentum (16.8 m/s at the foot), so the lanes are 40 m long.
- On a rotated patch the hard-ground trace follows the suspension direction, so the bottom stays `HardBottomDepth` away measured along the slope normal.
- **Slope rigs** (`B1Tests/Slope`, plateau +12.68 m above a run-up rig): `B1Slope_FirmClimb` (50 %, endH ≥ 12.18), `B1Slope_Medium30/50/100` (15 s, maxH ≤ 11.68), `B1Slope_MediumIdle` / `B1Slope_DeepIdle` (mid-slope, 10 s zero input, slide ≥ 1 m), `B1Slope_MediumDugIn` (3 s full, 10 s 50 %, maxH ≤ −0.39), `B1Slope_MediumDugInReverse` (3 s full, 6 s reverse, dist ≥ 5). <!-- updated 2026-10-27 -->

## Debug tooling

- `UB1ImGuiSubsystem` (GameInstance subsystem) owns the NetImGui main menu bar. New windows are registered with `RegisterWindow(Menu, Name, DrawFn)`. Vehicle Basics window (`B1ImGui::DrawVehicleBasics`) includes a per-wheel table; slipping/skidding cells are orange.
- Cheats live on `UB1CheatManager` (`Exec` UFUNCTIONs): `NetImGuiLaunch`, `B1TestForce off|push|spring`, `B1DamageVehicle <n>`, `B1SetCondition <n>`, `B1AddFuel <litres>` (GAS cheats go through the player's ASC and log before → after to `LogB1Vehicle`), `B1Unstuck`. <!-- updated 2026-10-26 -->
- Vehicle Status window (`B1ImGui::DrawVehicleStatus`, Debug → Vehicle Status): avatar, Condition/Fuel bars, owned tags, and Refill / ±10 L / Empty / Damage 10 / Repair buttons. A "Fuel use" section (avatar's `UB1FuelConsumptionComponent`) shows engine power, each fuel load (`Mud`), average sink, L/h and a rolling 5 s L/100 m, with the consumption-data rates (idle, work, one field per `LoadRates` entry, drain interval) editable plus "Save to asset", and a live-only `MudFullStrainDepth` field (component value). <!-- updated 2026-10-27 --> CVar `b1.Vehicle.StatusTelemetry` logs `B1Status <vehicle> cond= fuel= lph= l100= pow= strain= tags=` at 2 Hz for vehicles with an ASC. <!-- updated 2026-10-26 -->
- Shared ImGui helpers: `B1ImGui::DragFloatTyped` (`Debug/B1ImGuiWidgets`; drag plus typed field, required for every tunable float by `.claude/rules/imgui.md`) and `B1ImGui::SaveAssetPackage` (`Debug/B1ImGuiAssetSave`, editor only). <!-- updated 2026-10-26 -->
- Mud window (`B1ImGui::DrawMud`, `Mud/Debug/`, Debug → Mud): reads the player pawn's `UB1MudVehicleComponent`; belly readout, live `BellyClearance`, a per-wheel mud table, and live editing of every loaded `UB1PhysicalMaterial_Mud` (scalars and curve keys) on the asset objects themselves. "Save to asset" (`WITH_EDITOR`) calls `UPackage::SavePackage`.
- Telemetry: CVar `b1.Vehicle.Telemetry` logs one `B1Wheels ...` line per vehicle every 0.5 s to `LogB1Vehicle` (`B1VehicleTelemetry`). CVar `b1.Mud.Telemetry` logs `B1Mud ... belly= | w<i> mat= sink= slip= mu= rr= spray=` to `LogB1Mud` at the same rate, only while touching mud (reads the mud and spray components). <!-- updated 2026-10-27 -->
- ShowDebug: `ShowDebug B1Vehicle` draws vehicle text above the actor (`B1VehicleShowDebug`, bound via `AHUD::OnShowDebugInfo` in `AMudDriverPawn`, whose `DrawShowDebug` is virtual). `ShowDebug B1Mud` (`Mud/Debug/B1MudShowDebug`): `AB1Jeep` passes its `UB1MudVehicleComponent` and the text shows InMud (owner ASC has `InMudTag`), belly and per-wheel material / sink / max / mu / rr below the B1Vehicle anchor; each `AB1MudPatch` binds its own handler and draws its material, max sink, size, hard bottom and ramps when the viewer's camera is within 50 m. <!-- updated 2026-10-26 -->

## Testing

- `AB1GymTestRig : AFunctionalTest` (`MudDriverTests`): resolves a vehicle (explicit target, player's vehicle, or spawns `VehicleClass` with `SetRequiresControllerForInputs(false)`), waits `SettleTime`, plays an `InputScript` through the movement component setters, samples every tick and logs one line: `B1Test <name> PASS|FAIL dist= disp= vmax= stuck= settle= peakOsc= endH= avgDt= frames= [failed=...]`. `endH` is the vehicle's height above the rig actor at the end (optional `MinEndHeight` check). Optional `MaxFPS` forces `t.MaxFPS` for the run. `bAutoRun` runs it in a normal PIE session without the automation framework.
- Rigs in `Lvl_Offroad`: `B1Gym_FullThrottle5s` (baseline ~85–88 m at 60 fps, MinDistance 50); `B1Step_Spring20/120` and `B1Step_Push20/120` (stepping checks, S1-12). Optional `TestForce` on the rig switches the jeep's test force on at script start. Spring runs log `taut`, `settleTaut`, stretch amplitude and growth. `settle=-1` means still moving at the end. `B1Mud_WheelsOnPlane` and `B1Mud_ChassisThroughRail` (folder `B1Tests/MudChannel`, on their own floors at Z≈5000) check the mud channel: wheels rest on a mud plane, and the chassis drives through a mud rail. `B1Mud_PatchSurface` drops the jeep on a `BP_B1MudPatch` (`B1MudTest_Patch`) and checks that it rests on the surface. With a B1 movement component the rig line also has `sink=a/b/c/d` (max cm per wheel), and so does the `B1Mud` telemetry (current). `Lvl_Gym_Mud` has the mud rigs:
- `B1Mud_SinkMedium`: 2 s full throttle from the medium patch entry.
- `B1Mud_CrossMedium`: 50 % across the medium patch, peakOsc ≤ 0.5.
- `B1Mud_ExitMediumFull`: full throttle out of the medium patch, peakOsc ≤ 0.5.
- `B1Mud_SinkDeepStop`: 1.5 s full throttle into deep, then brake.
- `B1Mud_DigInMedium`: 4 s full throttle from rest in medium, ≤ 15 m (bogs down).
- `B1Mud_RestartMedium` / `B1Mud_RestartHeavy`: 6 s at 40 % from rest, ≥ 5 m.
- `B1Mud_RecoverMedium`: 2 s full, then 6 s at 35 %, ≥ 6 m.
- `B1Mud_CrawlIntoMedium`: 14 s at 30 % from firm ground, ≥ 25 m.
- `B1Mud_DeepRunUp`: full throttle with a 22 m run-up into deep, ≤ 42 m (stops inside the patch).
- `B1Mud_ExitDeep`: 50 % out of deep, ≥ 8 m, endH ≥ −1.05 (back at firm height).
- `B1Mud_DeepFullThrottle`: 7 s full throttle from rest in deep, stuck ≤ 5 s, `MaxEndSpeed` 0.5, ≤ 15 m.
- `B1Mud_CreepDeep` / `B1Mud_CreepMedium`: idle after digging in (deep) or from rest (medium), ≤ 5 cm horizontal in the last 10 s.
- Updated limits: DigInMedium 5–15 m, CrossMedium 10 s ≥ 32 m with peakOsc ≤ 0.65, CrawlIntoMedium peakOsc ≤ 0.5, DeepRunUp peakOsc ≤ 0.8.

The rig line also has `vend=` (last-sample speed, `MaxEndSpeed` threshold), `lastDisp=` (horizontal displacement during the last input step, `MaxLastStepDisplacement`) and, with a B1 component, `belly=` (max N). In PIE, rigs on the same lane collide, so run them in rounds with one rig per lane. <!-- updated 2026-10-26 -->

Fuel rigs use `bPossessWithPlayer`: player 0 possesses the rig's jeep so it gets the PlayerState ASC; `StartFuel` (negative = full) is set at script start, and `FinishTest` unbinds the template respawn and re-possesses the previous pawn. They add `fuelUsed= fuelPer100m= fuelPerHour= cond= inMud=` (s with `Vehicle.State.InMud`) with thresholds `Min/MaxFuelPer100m`, `MinFuelPerHour`, `MinInMudTime` (2 s on `B1Fuel_Medium`); the rig logs an error if the InMud tag survives the re-possess. Input steps can set `bUnstuck` (calls `Unstuck()` when the step starts); `B1Mud_UnstuckDeep` digs in deep, unstucks, then 3 s at 50 % (~14 m, smoke rig, no thresholds). Gym rigs: `B1Fuel_Firm` (50 %, 137 m, 0.085 L/100 m), `B1Fuel_Medium` (50 % in medium, ≥ 0.17 = 2× firm; ~0.37), `B1Fuel_FirmFull` (74 L/h), `B1Fuel_DeepFull` (stuck in deep, ≥ 150 L/h; ~344), `B1Fuel_Empty` (0 L, must not move). Firm fuel rigs run at Y 68 m, clear of the parked player jeep. <!-- updated 2026-10-26 -->

It also has `B1Gym_FirmSmoke` (6 s at 50 % throttle on the firm lane, ≥ 20 m; ~43 m) and `B1Gym_WallStop` (full throttle into the north wall; the jeep must stay on the floor). <!-- updated 2026-10-25 -->
- Headless: `Scripts/RunGymTests.ps1 [-Map] [-Filter]` runs `UnrealEditor-Cmd -nullrhi` with `Automation RunTests Project.Functional Tests.<map dotted>[.<label>]; Quit`. The editor exits 0 regardless, so the script parses `Test Completed. Result={...}` lines for its exit code. Needs the editor closed (`Scripts/StopEditor.ps1`).
- Editor lifecycle: `Scripts/StopEditor.ps1`, `StartEditor.ps1` and `BuildEditor.ps1` close, open and rebuild the editor without the user. Rules are in [.claude/rules/editor-lifecycle.md](../rules/editor-lifecycle.md).
- Agent-side, in-editor: place/configure rigs through MCP, `StartPIE`, read `LogB1Test` with `LogsToolset.GetLogEntries`. If the MCP tools don't register in a session, the server still takes plain JSON-RPC POSTs on `127.0.0.1:8001/mcp` (initialize, reuse the `Mcp-Session-Id` header, then `tools/call` `call_tool`). <!-- updated 2026-10-26 -->

<!-- updated 2026-10-25 -->

## Physics settings

Async physics, fixed step (ADR-003): `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, `MaxPhysicsDeltaTime=0.1` (real time down to 10 fps), `bSubstepping=False`. The Chaos vehicle sim and our forces run once per 1/60 s step on the physics thread. Game-thread reads of physics state are interpolated, about a frame behind. <!-- updated 2026-10-25 -->

`[ConsoleVariables] p.Chaos.Suspension.SlopeThreshold=1.0` turns off Chaos's suspension anti-slide (ADR-016). <!-- updated 2026-10-27 -->

## Rendering settings

Low-end budget (ADR-011): no dynamic GI, SSR, shadow maps (no VSM), no hardware RT, no mesh distance fields, no Substrate. Virtual textures on. DX12 SM6.
