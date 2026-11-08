# MIGRATION

How to copy mud and fuel into another UE 5.8 project with its own vehicle, movement component and fuel attribute. Keep this current: any change that adds a host requirement, setting, asset or dependency to `Mud/` or `Fuel/` updates this file in the same task ([portability rule](../rules/portability.md)).

ADRs behind it: ADR-001 (sinking), ADR-002 (mud material), ADR-003 (forces in the Chaos sim), ADR-008 (visual ruts), ADR-010 (fuel), ADR-012 (collision layering), ADR-015 (mud surface contract), ADR-021 (dirt layer).

## 1. Files

Copy into the target module's source folder (paths are relative to `Source/MudDriver/`):

| Folder | Files | Notes |
|---|---|---|
| `Mud/` | `B1PhysicalMaterial_Mud`, `B1MudSettings.h`, `B1MudLog`, `B1MudSimHost.h`, `B1MudVehicleComponent`, `B1MudSprayComponent`, `B1MudTrailComponent`, `B1MudDirtComponent`, `B1MudLensComponent`, `B1MudThrow`, `B1MudTrailSurfaceComponent`, `B1MudTrailSubsystem`, `B1MudPatch` | Runtime. `B1MudPatch` is optional (one surface producer, ADR-015) |
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
LensMaterial=/Game/B1/FX/MI_B1LensMud.MI_B1LensMud
```

`LensMaterial` is optional: unset, the lens still gathers and fades mud (debug, rigs) but shows nothing.

`DefaultEngine.ini`, for mud on slopes (ADR-016). Without it, Chaos's suspension anti-slide holds every stopped vehicle on slopes up to 45°, and muddy slopes never slide:

```ini
[ConsoleVariables]
p.Chaos.Suspension.SlopeThreshold=1.0
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
| `/Game/B1/FX/NS_B1MudSpray`, `M_B1MudSpray` | Wheel spray: emitters `Spray` (GPU clump meshes), `Droplets` (GPU sprites, `M_B1MudSpray`), `Haze` (CPU sprites). User params `SpawnRate`, `SprayVelocity`, `Color`, `Intensity` 0..1, `SpraySpread` (cone deg), `SpeedSpread` (± speed fraction); a replacement system needs the same six (the last two can be ignored) |
| `/Game/B1/Mud/Dirt/MF_B1MudDirt`, `T_B1MudBlotch`, `T_B1MudSplatter`, `T_B1MudNoise` | Dirt layer (ADR-021); the host's vehicle materials call the function. Textures regenerate with `Scripts/MudTextures` |
| `/Game/B1/FX/SM_B1MudClump`, `M_B1MudClump` | Spray clump mesh and its material (opaque, lit, particle colour; used with Niagara meshes) |
| `/Game/B1/FX/M_B1LensMud`, `MI_B1LensMud`, `MI_B1LensMud_Preview`, `/Game/B1/FX/Lens/T_B1LensDrops` | Lens mud post-process (after tonemapping, one Custom node = `Scripts/MudTextures/M_B1LensMud.hlsl`). Vector params `LensSlot0..3` = (strength, offset U, offset V, variant 0–7), written by `UB1MudLensComponent`'s MID; look params `LensRefraction`, `LensDropFill`, `LensDropSoftness`, `LensDropBlur`, `LensDropOpacity`, `LensDropTint(Amount)`, `LensHighlight`, `LensBottomScale`, `LensTopDensity`, `LensRevealGamma`. The texture regenerates with `Scripts/MudTextures/gen_lens_textures.py` |
| `/Game/B1/FX/M_B1MudHaze` | Spray haze sprite (translucent soft disc, particle colour and alpha, depth fade) |
| `/Game/B1/GAS/GE_B1FuelDrain` | Instant GE, SetByCaller modifier on the fuel attribute (or make your own) |
| `/Game/B1/Vehicles/DA_B1JeepTuning` | `UB1FuelConsumptionData` (optional, defaults match it) |

## 5. Host vehicle

**Movement component** implements two interfaces (see `UB1VehicleMovementComponent`):
- `IB1MudSimHost`: `GetMudSimulation()` (null until the physics vehicle exists), wheel count / `FWheelStatus` / world location (wheel bone) / radius / width, effective throttle (after input rates and any cut-off), raw drive input (throttle − brake, ignores cut-offs; used by the unstuck assist), `SetMudKeepAwake(bool)` (called every physics step; while true the vehicle must not sleep and must wake if asleep. Chaos sleeps on slopes up to 30°, which would hold a jeep on a muddy slope. This project sets `SleepSlopeLimit` to 1 and calls `SetSleeping(false)`).
- `IB1EngineLoadSource` (only with fuel): `GetNormalizedEnginePower()` in [0, 1]. Chaos doesn't expose engine torque on the game thread; this project rebuilds it as throttle² × torque(RPM) × RPM over the curve's peak power. Optional `IsEngineRunning()` (default true): false stops all consumption, idle included (this project: engine off while parked).

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
		// optional: a force you apply this step (winch, tow), world, N, so the mud drag caps count it as push
		Mud.SetExternalForce(MyWinchForce);
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
- `UB1MudSprayComponent`, `UB1MudTrailComponent`: optional visuals. They find the mud component in BeginPlay. The spray rate is `B1MudThrow` intensity × `MaxSpawnRate` (× `RollingRateFactor` below `MinSlipSpeed`), so rolling wheels spray too; dirt doesn't read the spray. Spawn point: `SpawnLift` above the visible mud and wheel radius + `SpawnBackOffset` behind the contact; randomness: `SpraySpread`, `SpraySpeedSpread` (all `Mud|Spray`).
- `UB1MudDirtComponent`: optional dirt on the vehicle (ADR-021). It reads only the mud component. The host:
  - tags the body and tire primitives with `B1MudDirt` (`TargetTag`; this project sets it on `BP_B1Jeep_Wrangler`'s inherited `Chassis` and four `Tire *` components). Once the wheels exist, a tagged part within a wheel's radius of that wheel's centre is its tire, any other is body; the mapping is logged once (`dirt targets: body=… w0=…`).
  - calls `MF_B1MudDirt` from the body/tire material: BaseColor, Roughness, Normal in; IsTire (1 on tire materials, this project uses a `bIsTire` static switch); the four arch centres (= wheel centres), `SillHeight` and the body's local bounds, all in the body mesh's local space; use the `Mask` output to fade anything else (metallic). Glass can add its own film from CPD slot 0 (see `M_Wrangler_Glass`).
  - CPD layout: body slot 0 = body dirt, slots 1–4 = wheel 0–3 dirt; tire slot 0 = its wheel's dirt (crust), 1 = tire coat, 2 = tire wetness. Keep slots 0–4 free on tagged parts.
  - `PreviewDirt` (0..1, −1 off) shows a stage in the editor without PIE; it drives the tire coat and wetness too.
  - Growth comes from `Mud/B1MudThrow` (intensity from sink, tread speed, slip), shared with the spray. Tread speed is a mud sim output (`UB1MudVehicleComponent::GetWheelTreadSpeed`), so no new host call. Tuning: `Mud|Dirt` (seconds to full, belly term) and `Mud|Dirt|Tire` (coat, shed distance, dry time) on the component.
  - Debug: `SetDirt(v)` / `Clean()` reset everything (this project's `B1Dirt` / `B1DirtClean` cheats call them).
- `UB1MudLensComponent`: optional mud on the chase camera's lens (ADR-022). Needs `UB1MudSprayComponent` on the same actor; it reads each wheel's launch origin and velocity (`GetWheelSprayOrigin` / `GetWheelSprayVelocity`), so spray off means a clean lens. The host:
  - tags the chase camera component(s) with `B1MudLens` (`TargetTag`; this project tags `BackCamera` in `AB1Jeep`'s constructor). Only an active tagged camera gathers mud. The MID of `UB1MudSettings::LensMaterial` is appended to each tagged camera's `PostProcessSettings.WeightedBlendables` in BeginPlay and removed in EndPlay; host blendables stay.
  - Exposure per wheel = spray intensity × launch-speed band × aim window × distance falloff, max over the wheels. Splashes fill 4 slots that hold, then fade; tuning in `Mud|Lens`. CVar `b1.Mud.Lens`.
  - Debug: `PreviewLensDirt`, `SetLensDirt(v)` / `CleanLens()` / `Splash()`.
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

- `ShowDebug B1Mud`: call `B1MudShowDebug::DrawVehicle(Canvas, DisplayInfo, MudComponent)` from your pawn's `AHUD::OnShowDebugInfo` handler. Patches bind their own. Adds a `dirt w …  body …` line when the actor has a `UB1MudDirtComponent` and a `lens d …  exp …  slots a/b/c/d` line when it has a `UB1MudLensComponent`.
- NetImGui: register `B1ImGui::DrawMud` in your ImGui window registry (this project: `UB1ImGuiSubsystem::RegisterWindow("Debug", "Mud", &B1ImGui::DrawMud)`). Its "Dirt", "Spray" and "Lens" sections read the player pawn's components; their "Save to asset" writes the pawn's own BP class component archetype. Lens: dirt bar, slot table (strength, hold/fade, offset, variant), per-wheel exposure (intensity, throw, aim, distance, launch speed, aim angle), splash count, Splash / Clean / Max, preview, every `Mud|Lens` float except `PreviewLensDirt` saved. The fuel rates are drawn by this project's Vehicle Status window, not part of `Fuel/`.
- Telemetry: this project's `B1VehicleTelemetry::LogMud` (2 Hz `B1Mud ...` line) reads only the mud, spray, dirt and lens components (`spray=rate/intensity` per wheel, `dirt=w0/w1/w2/w3/body`, `lens=dirt/exposure`); copy it if you want the line.
- Cheats (this project's `UB1CheatManager`, copy if wanted): `B1Dirt <v>`, `B1DirtClean`, `B1Lens <v>` (negative cleans), `B1LensSplash`.
- CVars: `b1.Mud.Spray`, `b1.Mud.Trails`, `b1.Mud.Lens`.

## 8. Mud surfaces

Any primitive that meets ADR-015 is mud: `B1MudSurface` profile, a `UB1PhysicalMaterial_Mud`, and something blocking `WorldDynamic` below it within `MaxSinkDepth`. Ruts need a `UB1MudTrailSurfaceComponent` initialised with a primitive whose UV0 spans its local XY rectangle and whose material is `M_B1Mud` (or has the same parameters).

## 9. Verify

- Drop the vehicle on a `BP_B1MudPatch` with `PM_B1Mud_Medium`: the `B1Mud` telemetry line (or `ShowDebug B1Mud`) shows sink climbing to ~16 cm at rest, and full throttle digs to 35 cm and sprays.
- Rigs: copy `MudDriverTests` (`AB1GymTestRig`) and the mud rigs from `Lvl_Gym_Mud`; `B1Mud_SinkMedium`, `B1Mud_DigInMedium`, `B1Mud_DeepFullThrottle` and `B1Fuel_Medium` are a good smoke set. Expect different numbers on a different vehicle.
- Dirt: the `B1Dirt_*` rigs (`B1Tests/Dirt`, `VehicleClass` = a vehicle with tagged parts) log `dirtTargets=tires/body` (expect 4/1) and the dirt and tire-coat values. `B1Dirt_Firm` must stay at 0.
- Spray look: `B1Spray_Medium` (`B1Tests/Spray`) is a capture rig with no thresholds; `Scripts/MudTextures/README.md` ("Spray sheet") has the camera and the comparison sheet.
- Lens: the `B1Lens_*` rigs (`B1Tests/Lens`) need a lens camera that is active without a controller (`lensCams=1`). They log `lensPeak lensEnd lensSplashes lensExpPeak throwPeak`. Firm and Roll must stay clean, Spin must splash, Cap stays ≤ 1, Fade clears. On a different vehicle, compare its rolling vs spinning `throwPeak` with `LensMin/FullThrowSpeed` first.
- Fuel: Vehicle Status (or your UI) shows L/h rising with throttle, more in mud at the same throttle.
