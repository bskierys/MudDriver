#include "B1InputKeyText.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

namespace B1InputKeyText
{
	static FString GetShortName(const FKey& Key)
	{
		// The engine names pad keys by position and at length.
		static const TMap<FKey, FString> ShortNames = {
			{ EKeys::Gamepad_FaceButton_Bottom, TEXT("A") },
			{ EKeys::Gamepad_FaceButton_Right, TEXT("B") },
			{ EKeys::Gamepad_FaceButton_Left, TEXT("X") },
			{ EKeys::Gamepad_FaceButton_Top, TEXT("Y") },
			{ EKeys::Gamepad_RightTriggerAxis, TEXT("RT") },
			{ EKeys::Gamepad_RightTrigger, TEXT("RT") },
			{ EKeys::Gamepad_LeftTriggerAxis, TEXT("LT") },
			{ EKeys::Gamepad_LeftTrigger, TEXT("LT") },
			{ EKeys::Gamepad_RightShoulder, TEXT("RB") },
			{ EKeys::Gamepad_LeftShoulder, TEXT("LB") },
			{ EKeys::Gamepad_Left2D, TEXT("LS") },
			{ EKeys::Gamepad_LeftX, TEXT("LS") },
			{ EKeys::Gamepad_LeftY, TEXT("LS") },
			{ EKeys::Gamepad_Right2D, TEXT("RS") },
			{ EKeys::Gamepad_RightX, TEXT("RS") },
			{ EKeys::Gamepad_RightY, TEXT("RS") },
			{ EKeys::Gamepad_DPad_Up, TEXT("D-pad Up") },
			{ EKeys::Gamepad_DPad_Down, TEXT("D-pad Down") },
			{ EKeys::Gamepad_DPad_Left, TEXT("D-pad Left") },
			{ EKeys::Gamepad_DPad_Right, TEXT("D-pad Right") },
			{ EKeys::Mouse2D, TEXT("Mouse") },
			{ EKeys::SpaceBar, TEXT("Space") },
		};
		const FString* Short = ShortNames.Find(Key);
		return Short ? *Short : Key.GetDisplayName(false).ToString();
	}

	FText GetKeyText(const UEnhancedInputLocalPlayerSubsystem& Subsystem, TConstArrayView<const UInputAction*> Actions, int32 KeysPerDevice)
	{
		TArray<FString> KeyboardNames;
		TArray<FString> PadNames;
		for (const UInputAction* Action : Actions)
		{
			if (!Action)
			{
				continue;
			}
			for (const FKey& Key : Subsystem.QueryKeysMappedToAction(Action))
			{
				// The template also maps VR controllers and racing wheels; only keyboard, mouse and pad are shown.
				const FName Category = Key.GetMenuCategory();
				const bool bPad = Category == EKeys::NAME_GamepadCategory;
				if (!bPad && Category != EKeys::NAME_KeyboardCategory && Category != EKeys::NAME_MouseCategory)
				{
					continue;
				}
				TArray<FString>& Names = bPad ? PadNames : KeyboardNames;
				const FString Name = GetShortName(Key);
				if (Names.Num() < KeysPerDevice && !Names.Contains(Name))
				{
					Names.Add(Name);
				}
			}
		}

		TArray<FString> Parts;
		for (const TArray<FString>* Names : { &KeyboardNames, &PadNames })
		{
			if (!Names->IsEmpty())
			{
				Parts.Add(FString::Join(*Names, TEXT(" ")));
			}
		}
		return Parts.IsEmpty() ? NSLOCTEXT("B1Input", "Unbound", "unbound") : FText::FromString(FString::Join(Parts, TEXT(" / ")));
	}
}
