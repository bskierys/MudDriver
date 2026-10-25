#include "B1PhysicalMaterial_Mud.h"

UB1PhysicalMaterial_Mud::UB1PhysicalMaterial_Mud()
{
	Friction = 0.5f;

	FRichCurve* FrictionCurve = FrictionVsDepth.GetRichCurve();
	FrictionCurve->AddKey(0.0f, 1.0f);
	FrictionCurve->AddKey(1.0f, 0.6f);

	FRichCurve* ResistanceCurve = RollingResistanceVsDepth.GetRichCurve();
	ResistanceCurve->AddKey(0.0f, 0.05f);
	ResistanceCurve->AddKey(1.0f, 0.3f);
}
