# Portability rules

Applies to `Source/MudDriver/Mud/**` and `Source/MudDriver/Fuel/**`. Mud and fuel must stay copyable into another project with a different vehicle, movement component and fuel attribute ([MIGRATION.md](../docs/MIGRATION.md)).

## Includes

- No project-specific classes from these folders: not `AB1Jeep`, `UB1VehicleMovementComponent`, `FB1VehicleSimulation`, `UB1AbilitySystemComponent`, `UB1VehicleAttributeSet`, `B1GameplayTags`, `UB1GASSettings`, `B1Log.h`, the player or game mode classes.
- Allowed cross-folder includes are the ones MIGRATION.md lists (today: `Mud/` → `Fuel/B1FuelLoadSource.h`; `Mud/Debug/` → `Debug/B1ImGuiWidgets.h`, `Debug/B1ImGuiAssetSave.h`). Fuel never includes Mud.
- Logs: `LogB1Mud` (`Mud/B1MudLog.h`) and `LogB1Fuel` (`Fuel/B1FuelLog.h`).

## Reaching the host

- Something the vehicle has to provide goes through an interface (`IB1MudSimHost`, `IB1EngineLoadSource`, `IB1FuelLoadSource`) or a property set by the host (tags, attribute, effect class). Never cast to a project class.
- Project tags and attributes are defaults set by the host (`AB1Jeep`'s constructor) or its BP, not hard-coded in these folders.
- Channel, profile and default assets come from `UB1MudSettings`, not macros or `ConstructorHelpers` paths.

## Keep MIGRATION.md current

Any change that adds a host requirement (interface method, hook call, property the host must set), a setting, an asset, a module or plugin dependency, or a cross-folder include updates [MIGRATION.md](../docs/MIGRATION.md) in the same task.
