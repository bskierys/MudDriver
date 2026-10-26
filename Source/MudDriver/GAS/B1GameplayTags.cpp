#include "B1GameplayTags.h"

namespace B1GameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Avatar_InVehicle, "State.Avatar.InVehicle", "Player is driving a vehicle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Avatar_OnFoot, "State.Avatar.OnFoot", "Player is on foot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_InMud, "Vehicle.State.InMud", "A wheel touches mud");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_OutOfFuel, "Vehicle.State.OutOfFuel", "No engine torque");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Fuel, "Data.Fuel", "SetByCaller fuel delta (L)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller condition damage");
}
