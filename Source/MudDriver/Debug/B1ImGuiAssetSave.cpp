#include "B1ImGuiAssetSave.h"
#if WITH_EDITOR
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace B1ImGui
{
	bool SaveAssetPackage(UObject& Asset)
	{
#if WITH_EDITOR
		UPackage* Package = Asset.GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *Filename, Args);
#else
		return false;
#endif
	}
}
