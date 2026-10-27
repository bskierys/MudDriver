# MIGRATION

How to copy mud and fuel into another UE 5.8 project with its own vehicle, movement component and fuel attribute. Keep this current: any change that adds a host requirement, setting, asset or dependency to `Mud/` or `Fuel/` updates this file in the same task ([portability rule](../rules/portability.md)).

ADRs behind it: ADR-001 (sinking), ADR-002 (mud material), ADR-003 (forces in the Chaos sim), ADR-008 (visual ruts), ADR-010 (fuel), ADR-012 (collision layering), ADR-015 (mud surface contract).

## 1. Files

Copy into the target module's source folder (paths are relative to `Source/MudDriver/`):

| Folder | Files | Notes |
|---|---|---|
| `Mud/` | `B1PhysicalMaterial_Mud`, `B1MudSettings.h`, `B1MudLog`, `B1MudSimHost.h`, `B1MudVehicleComponent`, `B1MudSprayComponent`, `B1MudTrailComponent`, `B1MudTrailSurfaceComponent`, `B1MudTrailSubsystem`, `B1MudPatch` | Runtime. `B1MudPatch` is optional (one surface producer, ADR-015) |
| `Mud/Sim/` | `B1MudWheelModel` (plain C++, no Chaos), `B1MudSimTypes.h`, `B1MudSimulation` | Physics-thread mud |
| `Mud/Debug/` | `B1MudShowDebug`, `B1ImGuiMud` | Optional. `B1ImGuiMud` needs NetImGui and `Debug/B1ImGuiWidgets` + `Debug/B1ImGuiAssetSave` |
| `Fuel/` | `B1FuelConsumptionComponent`, `B1FuelConsumptionData.h`, `B1FuelLoadSource.h`, `B1EngineLoadSource.h`, `B1FuelLog` | |

Cross-folder includes (the whole list):
- `Mud/B1MudVehicleComponent.h` → `Fuel/B1FuelLoadSource.h` (interface only; mud strain as a fuel load). Copying mud without fuel: copy that one header too, or drop `IB1FuelLoadSource` and `AddFuelLoads` from the component.
- `Mud/Debug/B1ImGuiMud.cpp` → `Debug/B1ImGuiWidgets.h`, `Debug/B1ImGuiAssetSave.h`.
- Nothing in `Mud/` or `Fuel/` includes the jeep, the movement component, the project ASC/attribute set or `B1GameplayTags`.

Rename the API macro (`MUDDRIVER_API`) to the target module's. Keep the `B1` prefix or rename it everywhere at once (classes, `Mud|...` categories, `LogB1Mud`, `LogB1Fuel`, CVars `b1.Mud.*`, ImGui labels).

## 2. Build.cs and plugins

- Public: `Core`, `CoreUObject`, `Engine`, `ChaosVehicles`, `PhysicsCore`, `GameplayAbilities`, `GameplayTags`, `DeveloperSettings`; `NetImgui` only with `Mud/Debug/B1ImGuiMud`.
- Private: `ChaosVehiclesCore`, `Niagara`, `ProceduralMeshComponent`.
- Include paths: add `Mud`, `Mud/Sim`, `Mud/Debug`, `Fuel` (this project lists every folder in `PublicIncludePaths`).
- Plugins: ChaosVehiclesPlugin, GameplayAbilities, Niagara, ProceduralMeshComponent; NetImgui for the window.

## 3. Config

`DefaultEngine.ini`, `[/Script/Engine.CollisionProfile]` (pick a free channel if `GameTraceChannel1` is taken):

```ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=False,bStaticObject=False,Name="B1Mud")
+Profiles=(Name="B1MudSurface",CollisionEnabled=QueryOnly,bCanModify=True,ObjectTypeName="B1Mud",CustomResponses=((Channel="WorldStatic",Response=ECR_Ignore),(Channel="WorldDynamic",Response=ECR_Block),(Channel="Pawn",Response=ECR_Ignore),(Channel="Visibility",Response=ECR_Ignore),(Channel="Camera",Response=ECR_Ignore),(Channel="PhysicsBody",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore),(Channel="Destructible",Response=ECR_Ignore)),HelpMessage="Mud surface: only vehicle wheel traces hit it (ADR-012).")
```

`DefaultGame.ini` (Project Settings → Game → B1 Mud):

```ini
[/Script/MudDriver.B1MudSettings]
MudObjectChannel=ECC_GameTraceChannel1
SurfaceProfileName=B1MudSurface
SpraySystem=/Game/B1/FX/NS_B1MudSpray.NS_B1MudSpray
```

Replace `MudDriver` in `/Script/MudDriver.` with the target module name. If you copy an existing `DA_B1JeepTuning`, it was saved as `UB1VehicleTuningData`; this project's `[CoreRedirects]` entry maps it to `UB1FuelConsumptionData`. A fresh data asset doesn't need it.

## 4. Content

Migrate with the editor's Asset Actions → Migrate so references come along:

| Asset | Used by |
|---|---|
| `/Game/B1/Mud/M_B1Mud` | Mud surface material: `TrailMask`, `TrailDepthRange`, `PatchSizeX/Y`, `Color`; HLSL ruts (ADR-008) |
| `/Game/B1/Mud/MI_B1Mud_Shallow/Medium/Deep` | Tints |
| `/Game/B1/Mud/PM_B1Mud_Shallow/Medium/Heavy/Deep` | `UB1PhysicalMaterial_Mud` presets |
| `/Game/B1/Mud/BP_B1MudPatch` | Placeable patch |
| `/Game/B1/FX/NS_B1MudSpray`, `M_B1MudSpray` | Wheel spray (user params `SpawnRate`, `SprayVelocity`, `Color`) |
| `/Game/B1/GAS/GE_B1FuelDrain` | Instant GE, SetByCaller modifier on the fuel attribute (or make your own) |
| `/Game/B1/Vehicles/DA_B1JeepTuning` | `UB1FuelConsumptionData` (optional, defaults match it) |

## 5. Host vehicle

**Movement component** implements two interfaces (see `UB1VehicleMovementComponent`):
- `IB1MudSimHost`: `GetMudSimulation()` (null until the physics vehicle exists), wheel count / `FWheelStatus` / world location (wheel bone) / radius / width, effective throttle (after input rates and any cut-off), raw drive input (throttle − brake, ignores cut-offs; used by the unstuck assist).
- `IB1EngineLoadSource` (only with fuel): `GetNormalizedEnginePower()` in [0, 1]. Chaos doesn't expose engine torque on the game thread; this project rebuilds it as throttle² × torque(RPM) × RPM over the curve's peak power.

It also creates its own Chaos simulation subclass (`CreatePhysicsVehicle` sets `VehicleSimulationPT` before calling `UChaosVehicleMovementComponent::CreatePhysicsVehicle`) and makes its wheel traces block the mud channel in `OnRegister`:

```cpp
WheelTraceCollisionResponses.SetResponse(GetDefault<UB1MudSettings>()->MudObjectChannel, ECR_Block);
```

**Chaos sim subclass** owns an `FB1MudSimulation` and calls its hooks:

```cpp
class FMyVehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:
	FB1MudSimulation& GetMudSimulation() { return Mud; }

	virtual void UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle) override
	{
		Mud.BeginStep();
		UChaosWheeledVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle);
		// your own forces here
		Mud.EndStep(*this, DeltaTime);
	}

	virtual void PerformSuspensionTraces(const TArray<Chaos::FSuspensionTrace>& SuspensionTrace, FCollisionQueryParams& TraceParams, FCollisionResponseContainer& CollisionResponse, TArray<FWheelTraceParams>& WheelTraceParams) override
	{
		TArray<Chaos::FSuspensionTrace> Traces = SuspensionTrace;
		Mud.PreSuspensionTraces(*this, Traces);
		UChaosWheeledVehicleSimulation::PerformSuspensionTraces(Traces, TraceParams, CollisionResponse, WheelTraceParams);
		Mud.PostSuspensionTraces(*this, Traces, TraceParams, CollisionResponse);
	}

	virtual void ApplyWheelFrictionForces(float DeltaTime) override
	{
		Mud.PreFriction(*this);
		UChaosWheeledVehicleSimulation::ApplyWheelFrictionForces(DeltaTime);
		Mud.PostFriction(*this, DeltaTime);
	}

private:
	FB1MudSimulation Mud;
};
```

Use async physics with a fixed step (ADR-003, `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=0.016667`, `MaxPhysicsDeltaTime=0.1`); the mud tuning assumes it.

**Components on the vehicle actor** (this project creates them in `AB1Jeep`'s constructor):
- `UB1MudVehicleComponent`: required for mud. It feeds the sim from `FPhysScene_Chaos::OnPhysSceneStep`, so the host doesn't override `Update` for mud. Config: `BellyClearance` (axle height), unstuck settings, `MudFullStrainDepth`, `FuelLoadName`.
- `UB1MudSprayComponent`, `UB1MudTrailComponent`: optional visuals. They find the mud component in BeginPlay.
- `UB1FuelConsumptionComponent`: optional fuel. Finds `IB1EngineLoadSource` and every `IB1FuelLoadSource` component in BeginPlay.

`B1Unstuck` lives on the mud component (`Unstuck()`); wire your own cheat to it. It assumes the actor's root is the simulating body.

## 6. GAS wiring

Both components find the ASC with `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner)`, so the vehicle (or its PlayerState, ADR-004) must expose one. They only act while the pawn is controlled and drop their state when the controller changes.

| Property | This project | Notes |
|---|---|---|
| `UB1MudVehicleComponent::InMudTag` | `Vehicle.State.InMud` | Loose tag; unset = no tag |
| `UB1FuelConsumptionComponent::FuelAttribute` | `UB1VehicleAttributeSet.Fuel` | Any float attribute, litres |
| `DrainEffect` | `GE_B1FuelDrain` | Gets negative litres |
| `DrainSetByCallerTag` | `Data.Fuel` | |
| `OutOfFuelTag` | `Vehicle.State.OutOfFuel` | Unset = no tag |
| `Data` | `DA_B1JeepTuning` | `IdleRate`, `WorkRate`, `LoadRates` (`Mud` = 300 L/h), `DrainInterval` |

Tags and attribute are set in `AB1Jeep`'s constructor; `Data` and `DrainEffect` on `BP_B1Jeep`. Bind `OnOutOfFuelChanged` and cut your engine there (this project zeroes `ThrottleInput` before `Super::Update`).

## 7. Debug

- `ShowDebug B1Mud`: call `B1MudShowDebug::DrawVehicle(Canvas, DisplayInfo, MudComponent)` from your pawn's `AHUD::OnShowDebugInfo` handler. Patches bind their own.
- NetImGui: register `B1ImGui::DrawMud` in your ImGui window registry (this project: `UB1ImGuiSubsystem::RegisterWindow("Debug", "Mud", &B1ImGui::DrawMud)`). The fuel rates are drawn by this project's Vehicle Status window, not part of `Fuel/`.
- Telemetry: this project's `B1VehicleTelemetry::LogMud` (2 Hz `B1Mud ...` line) reads only the mud and spray components; copy it if you want the line.
- CVars: `b1.Mud.Spray`, `b1.Mud.Trails`.

## 8. Mud surfaces

Any primitive that meets ADR-015 is mud: `B1MudSurface` profile, a `UB1PhysicalMaterial_Mud`, and something blocking `WorldDynamic` below it within `MaxSinkDepth`. Ruts need a `UB1MudTrailSurfaceComponent` initialised with a primitive whose UV0 spans its local XY rectangle and whose material is `M_B1Mud` (or has the same parameters).

## 9. Verify

- Drop the vehicle on a `BP_B1MudPatch` with `PM_B1Mud_Medium`: the `B1Mud` telemetry line (or `ShowDebug B1Mud`) shows sink climbing to ~16 cm at rest, and full throttle digs to 35 cm and sprays.
- Rigs: copy `MudDriverTests` (`AB1GymTestRig`) and the mud rigs from `Lvl_Gym_Mud`; `B1Mud_SinkMedium`, `B1Mud_DigInMedium`, `B1Mud_DeepFullThrottle` and `B1Fuel_Medium` are a good smoke set. Expect different numbers on a different vehicle.
- Fuel: Vehicle Status (or your UI) shows L/h rising with throttle, more in mud at the same throttle.
