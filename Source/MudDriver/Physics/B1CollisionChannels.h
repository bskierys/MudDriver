#pragma once

// Must match the [/Script/Engine.CollisionProfile] channels in DefaultEngine.ini.

/** Mud surface layer, hit only by wheel suspension traces (ADR-012) */
#define B1_ObjectChannel_Mud ECC_GameTraceChannel1

#define B1_CollisionProfile_MudSurface TEXT("B1MudSurface")
