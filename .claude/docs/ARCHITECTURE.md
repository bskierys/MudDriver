# ARCHITECTURE

The source of truth for what exists *today*. Planned systems live in [PLAN.md](PLAN.md) until they are built. Update via `/save`.

## Modules

UE 5.8, engine at `C:\Program Files\UE_5.8`.

| Module | Type | Deps | Holds |
|---|---|---|---|
| `MudDriver` | Runtime | Core, CoreUObject, Engine, InputCore, EnhancedInput, ChaosVehicles, PhysicsCore, UMG, Slate, NetImgui; private Projects, ChaosVehiclesCore | Game code |
| `MudDriverTests` | DeveloperTool (not in Shipping, ADR-013) | Core, CoreUObject, Engine, FunctionalTesting; private ChaosVehicles, MudDriver | Gym test rigs |

Game-module symbols used by the tests need `MUDDRIVER_API` (the log categories already have it).

Plugins enabled: ChaosVehiclesPlugin, GameplayAbilities (no code uses it yet), NetImgui (project plugin), ModelContextProtocol + editor toolsets, GameInputWindows, ModelingToolsEditorMode.

Log categories (`B1Log.h`): `LogB1Vehicle`, `LogB1Physics`, `LogB1Test`.

## Code

| Area | Classes | Notes |
|---|---|---|
| Vehicle pawn | `AMudDriverPawn` (abstract, `AWheeledVehiclePawn`) | Template: input, front/back cameras, flip check + auto-reset. Constructors take `FObjectInitializer` down the chain. Owns the telemetry timer and the ShowDebug binding |
| Offroad jeep (template) | `AMudDriverOffroadCar`, `UMudDriverOffroadWheelFront/Rear` | AWD, 600 Nm, wheel radius 50, shapecast suspension. BP: `BP_OffroadCar_Pawn` |
| B1 jeep | `AB1Jeep : AMudDriverOffroadCar` (abstract) | Swaps in `UB1VehicleMovementComponent` via `SetDefaultSubobjectClass`. BP: `/Game/B1/Vehicles/BP_B1Jeep` (duplicate of the template BP, 0 CDO diffs). Default pawn of `BP_OffRoadGameMode` |
| Sports car | `AMudDriverSportsCar` + wheels | Template leftover, unused for the game |
| Controller | `AMudDriverPlayerController` | Mapping contexts, respawn, vehicle UI, sets `UB1CheatManager` |
| Game modes | `AMudDriverGameMode`, `AOffroadGameMode`, `ATimeTrialGameMode` | Template variants |

Maps: `VehicleTemplate/Maps/VehicleBasic`, `Variant_OffRoad/Maps/Lvl_Offroad` (World Partition, one file per actor), `Variant_TimeTrial/Maps/*`.
Physical materials: `Vehicles/PhysicsMaterials/Slippery`, `NonSlippery`.

## Vehicle simulation (ADR-003)

- `UB1VehicleMovementComponent : UChaosWheeledVehicleMovementComponent` overrides `CreatePhysicsVehicle()` to install `FB1VehicleSimulation` before calling the base.
- `FB1VehicleSimulation : UChaosWheeledVehicleSimulation` runs on the physics thread (`UpdateSimulation`). It never touches UObjects.
- **Game → physics data:** `SetCurrentAsyncInputOutput` is non-virtual and always allocates the base async structs, so custom input can't ride the Chaos async input. The component's `Update(DeltaTime)` pushes a POD `FB1VehicleSimInput` through `SetGameThreadInput` into a `FCriticalSection`-guarded `PendingInput`; the physics step reads the latest copy.
- **Physics → game data:** the sim copies its per-step results (`FB1VehicleSimOutput`) under a second lock. The component reads them in `Update` (`GetWheelSinkDepth`).
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
  - The component keeps `KnownMudMaterials` (every mud material its wheels have touched) and sends copies of their params (`FB1MudSimParams`). The sim matches `TraceResult.PhysMaterial` by weak-pointer equality, so new mud reads as firm for about a frame.
- **Traction and resistance (ADR-001):** `ApplyWheelFrictionForces` scales each mud wheel's `FrictionMultiplier` by `FrictionVsDepth` around the base call. After the call, it adds a rolling resistance force along the steered wheel forward at the contact point: (`RollingResistanceVsDepth` + `SpeedDragCoefficient`·depth·v²) × the wheel's weight share. Below 1 cm/s it opposes the tire's push, like static friction, and it's capped at the tire push plus 0.4 × the stopping force. `SinkOffset` rises at max(30 cm/s, 0.5 × wheel speed). The `B1Mud` telemetry shows `mu=` and `rr=`. <!-- updated 2026-10-26 -->
- **Belly drag (ADR-012):** after rolling resistance, `ApplyBellyDrag` takes the average `SinkDepth` of all wheels minus `BellyClearance` (component, 45 cm = axle height; the body underside measures 60–64 cm, logged once per vehicle). Drag = mean `BellyDragCoefficient` of the mud wheels × excess (m) × weight, at the centre of mass against ground velocity, with the same standstill and no-reversal rules as rolling resistance. Only Heavy and Deep reach it. Telemetry `belly=<cm>/<N>`. <!-- updated 2026-10-26 -->
- Forces from the sim use `AddForce` in kg·cm/s² (N × 100).
- **Test forces** (`EB1TestForce`: Off / Push / Spring, settings in `TestForceSettings`, category `TestForce|Config`): push = constant world +Y force; spring = one-sided spring-damper to an anchor captured at toggle time. While active, the component zeroes `SleepThreshold`, because a sleeping vehicle skips the simulation, and restores it on Off.
- Steps at a fixed 1/60 s (see Physics settings).

## Collision (ADR-012)

| Channel | Index | Default | Used by |
|---|---|---|---|
| `B1Mud` (object) | `ECC_GameTraceChannel1` (`B1_ObjectChannel_Mud`, `Physics/B1CollisionChannels.h`) | Ignore | Mud surface meshes |

- Profile `B1MudSurface`: object `B1Mud`, QueryOnly, blocks only `WorldDynamic`. The Chaos suspension trace queries `WorldDynamic` with `WheelTraceCollisionResponses`, so only wheel traces that block `B1Mud` hit the mud.
- `UB1VehicleMovementComponent`'s constructor sets `WheelTraceCollisionResponses[B1Mud] = Block`. The chassis (`Vehicle`), pawns, physics bodies and the camera pass through.
- Static mesh components must have `bUseDefaultCollision=false` (or use `SetCollisionProfileName`), or the mesh's own collision overrides the profile. <!-- updated 2026-10-25 -->

## Mud

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

## Debug tooling

- `UB1ImGuiSubsystem` (GameInstance subsystem) owns the NetImGui main menu bar. New windows are registered with `RegisterWindow(Menu, Name, DrawFn)`. Vehicle Basics window (`B1ImGui::DrawVehicleBasics`) includes a per-wheel table; slipping/skidding cells are orange.
- Cheats live on `UB1CheatManager` (`Exec` UFUNCTIONs): `NetImGuiLaunch`, `B1TestForce off|push|spring`.
- Mud window (`B1ImGui::DrawMud`, Debug → Mud): belly readout, live `BellyClearance`, a per-wheel mud table, and live editing of every loaded `UB1PhysicalMaterial_Mud` (scalars and curve keys) on the asset objects themselves. "Save to asset" (`WITH_EDITOR`) calls `UPackage::SavePackage`.
- Telemetry: CVar `b1.Vehicle.Telemetry` logs one `B1Wheels ...` line per vehicle every 0.5 s to `LogB1Vehicle` (`B1VehicleTelemetry`). CVar `b1.Mud.Telemetry` logs `B1Mud ... belly= | w<i> mat= sink= slip= mu= rr=` to `LogB1Physics` at the same rate, only while touching mud. <!-- updated 2026-10-26 -->
- ShowDebug: `ShowDebug B1Vehicle` draws vehicle text above the actor (`B1VehicleShowDebug`, bound via `AHUD::OnShowDebugInfo`).

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

It also has `B1Gym_FirmSmoke` (6 s at 50 % throttle on the firm lane, ≥ 20 m; ~43 m) and `B1Gym_WallStop` (full throttle into the north wall; the jeep must stay on the floor). <!-- updated 2026-10-25 -->
- Headless: `Scripts/RunGymTests.ps1 [-Map] [-Filter]` runs `UnrealEditor-Cmd -nullrhi` with `Automation RunTests Project.Functional Tests.<map dotted>[.<label>]; Quit`. The editor exits 0 regardless, so the script parses `Test Completed. Result={...}` lines for its exit code. Needs the editor closed (`Scripts/StopEditor.ps1`).
- Editor lifecycle: `Scripts/StopEditor.ps1`, `StartEditor.ps1` and `BuildEditor.ps1` close, open and rebuild the editor without the user. Rules are in [.claude/rules/editor-lifecycle.md](../rules/editor-lifecycle.md).
- Agent-side, in-editor: place/configure rigs through MCP, `StartPIE`, read `LogB1Test` with `LogsToolset.GetLogEntries`. If the MCP tools don't register in a session, the server still takes plain JSON-RPC POSTs on `127.0.0.1:8001/mcp` (initialize, reuse the `Mcp-Session-Id` header, then `tools/call` `call_tool`). <!-- updated 2026-10-26 -->

<!-- updated 2026-10-25 -->

## Physics settings

Async physics, fixed step (ADR-003): `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, `MaxPhysicsDeltaTime=0.1` (real time down to 10 fps), `bSubstepping=False`. The Chaos vehicle sim and our forces run once per 1/60 s step on the physics thread. Game-thread reads of physics state are interpolated, about a frame behind. <!-- updated 2026-10-25 -->

## Rendering settings

Low-end budget (ADR-011): no dynamic GI, SSR, shadow maps (no VSM), no hardware RT, no mesh distance fields, no Substrate. Virtual textures on. DX12 SM6.
