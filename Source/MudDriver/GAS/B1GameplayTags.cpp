#include "B1GameplayTags.h"

namespace B1GameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Avatar_InVehicle, "State.Avatar.InVehicle", "Player is driving a vehicle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Avatar_OnFoot, "State.Avatar.OnFoot", "Player is on foot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_InMud, "Vehicle.State.InMud", "A wheel touches mud");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_OutOfFuel, "Vehicle.State.OutOfFuel", "No engine torque");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_Winching, "Vehicle.State.Winching", "Winch is reeling in");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Vehicle_State_EngineOff, "Vehicle.State.EngineOff", "Parked, engine off");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Fuel, "Data.Fuel", "SetByCaller fuel delta (L)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller condition damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Debug_TestPing, "InputTag.Debug.TestPing", "Activates GA_B1TestPing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "InputTag.Interact", "Activates GA_B1Interact");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Interact_TestPing, "Event.Interact.TestPing", "Test interactable's option; triggers GA_B1TestPing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Interact_EnterVehicle, "Event.Interact.EnterVehicle", "Jeep door's option; triggers GA_B1EnterVehicle");
}
