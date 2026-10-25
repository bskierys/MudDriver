#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "B1ImGuiSubsystem.generated.h"

/** Owns the NetImgui main menu bar and the debug windows launchable from it */
UCLASS()
class UB1ImGuiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	struct FWindowEntry
	{
		const char* Menu;
		const char* Name;
		TFunction<void(UWorld*, bool*)> Draw;
		bool bOpen = false;
	};

	void RegisterWindow(const char* Menu, const char* Name, TFunction<void(UWorld*, bool*)> Draw);
	void DrawImGui();
	void DrawMainMenuBar();

	TArray<FWindowEntry> Windows;
	FDelegateHandle DrawHandle;
};
