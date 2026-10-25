# ARCHITECTURE

The source of truth for what exists *today*. Planned systems live in [PLAN.md](PLAN.md) until they are built. Update via `/save`.

## Module

Single runtime module `MudDriver` (UE 5.8, engine at `C:\Program Files\UE_5.8`).

Deps: Core, CoreUObject, Engine, InputCore, EnhancedInput, ChaosVehicles, PhysicsCore, UMG, Slate, NetImgui, Projects.

Plugins enabled: ChaosVehiclesPlugin, GameplayAbilities (no code uses it yet), NetImgui (project plugin), ModelContextProtocol, GameInputWindows, ModelingToolsEditorMode.

## Current code (from the UE vehicle template)

| Area | Classes | Notes |
|---|---|---|
| Vehicle pawn | `AMudDriverPawn` (abstract, `AWheeledVehiclePawn`) | Input, front/back cameras, flip check + auto-reset |
| Offroad jeep | `AMudDriverOffroadCar`, `UMudDriverOffroadWheelFront/Rear` | AWD, 600 Nm, wheel radius 50, shapecast suspension. BP: `BP_OffroadCar_Pawn` |
| Sports car | `AMudDriverSportsCar` + wheels | Template leftover, unused for the game |
| Controller | `AMudDriverPlayerController` | Mapping contexts, respawn, vehicle UI, sets cheat manager |
| Game modes | `AMudDriverGameMode`, `AOffroadGameMode`, `ATimeTrialGameMode` | Template variants |
| Debug | `UMudImGuiSubsystem`, `MudImGui::DrawVehicleBasics`, `UMudCheatManager` | NetImGui menu bar with registered windows; `NetImGuiLaunch` cheat |

Maps: `VehicleTemplate/Maps/VehicleBasic`, `Variant_OffRoad/Maps/Lvl_Offroad`, `Variant_TimeTrial/Maps/*`.
Physical materials: `Vehicles/PhysicsMaterials/Slippery`, `NonSlippery`.

## Debug tooling

- `UMudImGuiSubsystem` (GameInstance subsystem) owns the NetImGui main menu bar. New windows are registered with `RegisterWindow(Menu, Name, DrawFn)`.
- Cheats live on `UMudCheatManager` (`Exec` UFUNCTIONs).

## Physics settings

`bSubstepping=False`, `bSubsteppingAsync=True`. Chaos vehicle sim runs through the vehicle manager async callback (physics thread).

## Rendering settings

Low-end budget (ADR-011): no dynamic GI, SSR, shadow maps (no VSM), no hardware RT, no mesh distance fields, no Substrate. Virtual textures on. DX12 SM6.
