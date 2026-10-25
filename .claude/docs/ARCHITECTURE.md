# ARCHITECTURE

The source of truth for what exists *today*. Planned systems live in [PLAN.md](PLAN.md) until they are built. Update via `/save`.

## Modules

UE 5.8, engine at `C:\Program Files\UE_5.8`.

| Module | Type | Deps | Holds |
|---|---|---|---|
| `MudDriver` | Runtime | Core, CoreUObject, Engine, InputCore, EnhancedInput, ChaosVehicles, PhysicsCore, UMG, Slate, NetImgui, Projects | Game code |
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

- `AB1MudPatch` (`Mud/B1MudPatch.h`) has two components:
  - `MudSurface`: a 10 cm slab, top at the actor origin, `B1MudSurface` profile, `MudMaterial` as its phys material override.
  - `HardBottom`: a 1 m BlockAll slab, top `HardBottomDepth` below the surface. It's hidden and NoCollision when `bHardBottom` is off, for patches where the landscape is the bottom.
  - `OnConstruction` sizes both from `Size` (cm).
- `/Game/B1/Mud/BP_B1MudPatch` is the placeable child, with `M_B1Mud` (a `Color` vector parameter for per-depth tints). Tints: `MI_B1Mud_Shallow/Medium/Deep`, set as the `MudSurface` material override per instance. <!-- updated 2026-10-25 -->

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
- Telemetry: CVar `b1.Vehicle.Telemetry` logs one `B1Wheels ...` line per vehicle every 0.5 s to `LogB1Vehicle` (`B1VehicleTelemetry`).
- ShowDebug: `ShowDebug B1Vehicle` draws vehicle text above the actor (`B1VehicleShowDebug`, bound via `AHUD::OnShowDebugInfo`).

## Testing

- `AB1GymTestRig : AFunctionalTest` (`MudDriverTests`): resolves a vehicle (explicit target, player's vehicle, or spawns `VehicleClass` with `SetRequiresControllerForInputs(false)`), waits `SettleTime`, plays an `InputScript` through the movement component setters, samples every tick and logs one line: `B1Test <name> PASS|FAIL dist= disp= vmax= stuck= settle= peakOsc= endH= avgDt= frames= [failed=...]`. `endH` is the vehicle's height above the rig actor at the end (optional `MinEndHeight` check). Optional `MaxFPS` forces `t.MaxFPS` for the run. `bAutoRun` runs it in a normal PIE session without the automation framework.
- Rigs in `Lvl_Offroad`: `B1Gym_FullThrottle5s` (baseline ~85–88 m at 60 fps, MinDistance 50); `B1Step_Spring20/120` and `B1Step_Push20/120` (stepping checks, S1-12). Optional `TestForce` on the rig switches the jeep's test force on at script start. Spring runs log `taut`, `settleTaut`, stretch amplitude and growth. `settle=-1` means still moving at the end. `B1Mud_WheelsOnPlane` and `B1Mud_ChassisThroughRail` (folder `B1Tests/MudChannel`, on their own floors at Z≈5000) check the mud channel: wheels rest on a mud plane, and the chassis drives through a mud rail. `B1Mud_PatchSurface` drops the jeep on a `BP_B1MudPatch` (`B1MudTest_Patch`) and checks that it rests on the surface. `Lvl_Gym_Mud` has `B1Gym_FirmSmoke` (6 s at 50 % throttle on the firm lane, ≥ 20 m; ~43 m) and `B1Gym_WallStop` (full throttle into the north wall; the jeep must stay on the floor). <!-- updated 2026-10-25 -->
- Headless: `Scripts/RunGymTests.ps1 [-Map] [-Filter]` runs `UnrealEditor-Cmd -nullrhi` with `Automation RunTests Project.Functional Tests.<map dotted>[.<label>]; Quit`. The editor exits 0 regardless, so the script parses `Test Completed. Result={...}` lines for its exit code. Needs the editor closed.
- Agent-side, in-editor: place/configure rigs through MCP, `StartPIE`, read `LogB1Test` with `LogsToolset.GetLogEntries`.

<!-- updated 2026-10-25 -->

## Physics settings

Async physics, fixed step (ADR-003): `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, `MaxPhysicsDeltaTime=0.1` (real time down to 10 fps), `bSubstepping=False`. The Chaos vehicle sim and our forces run once per 1/60 s step on the physics thread. Game-thread reads of physics state are interpolated, about a frame behind. <!-- updated 2026-10-25 -->

## Rendering settings

Low-end budget (ADR-011): no dynamic GI, SSR, shadow maps (no VSM), no hardware RT, no mesh distance fields, no Substrate. Virtual textures on. DX12 SM6.
