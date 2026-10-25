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

## Debug tooling

- `UB1ImGuiSubsystem` (GameInstance subsystem) owns the NetImGui main menu bar. New windows are registered with `RegisterWindow(Menu, Name, DrawFn)`. Vehicle Basics window (`B1ImGui::DrawVehicleBasics`) includes a per-wheel table; slipping/skidding cells are orange.
- Cheats live on `UB1CheatManager` (`Exec` UFUNCTIONs): `NetImGuiLaunch`, `B1TestForce off|push|spring`.
- Telemetry: CVar `b1.Vehicle.Telemetry` logs one `B1Wheels ...` line per vehicle every 0.5 s to `LogB1Vehicle` (`B1VehicleTelemetry`).
- ShowDebug: `ShowDebug B1Vehicle` draws vehicle text above the actor (`B1VehicleShowDebug`, bound via `AHUD::OnShowDebugInfo`).

## Testing

- `AB1GymTestRig : AFunctionalTest` (`MudDriverTests`): resolves a vehicle (explicit target, player's vehicle, or spawns `VehicleClass` with `SetRequiresControllerForInputs(false)`), waits `SettleTime`, plays an `InputScript` through the movement component setters, samples every tick and logs one line: `B1Test <name> PASS|FAIL dist= disp= vmax= stuck= settle= peakOsc= avgDt= frames= [failed=...]`. Optional `MaxFPS` forces `t.MaxFPS` for the run. `bAutoRun` runs it in a normal PIE session without the automation framework.
- Rigs in `Lvl_Offroad`: `B1Gym_FullThrottle5s` (baseline ~85–88 m at 60 fps, MinDistance 50); `B1Step_Spring20/120` and `B1Step_Push20/120` (stepping checks, S1-12). Optional `TestForce` on the rig switches the jeep's test force on at script start. Spring runs log `taut`, `settleTaut`, stretch amplitude and growth. `settle=-1` means still moving at the end.
- Headless: `Scripts/RunGymTests.ps1 [-Map] [-Filter]` runs `UnrealEditor-Cmd -nullrhi` with `Automation RunTests Project.Functional Tests.<map dotted>[.<label>]; Quit`. The editor exits 0 regardless, so the script parses `Test Completed. Result={...}` lines for its exit code. Needs the editor closed.
- Agent-side, in-editor: place/configure rigs through MCP, `StartPIE`, read `LogB1Test` with `LogsToolset.GetLogEntries`.

<!-- updated 2026-10-25 -->

## Physics settings

Async physics, fixed step (ADR-003): `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, `MaxPhysicsDeltaTime=0.1` (real time down to 10 fps), `bSubstepping=False`. The Chaos vehicle sim and our forces run once per 1/60 s step on the physics thread. Game-thread reads of physics state are interpolated, about a frame behind. <!-- updated 2026-10-25 -->

## Rendering settings

Low-end budget (ADR-011): no dynamic GI, SSR, shadow maps (no VSM), no hardware RT, no mesh distance fields, no Substrate. Virtual textures on. DX12 SM6.
