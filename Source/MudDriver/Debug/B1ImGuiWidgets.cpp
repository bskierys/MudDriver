#include "B1ImGuiWidgets.h"

#if NETIMGUI_ENABLED
namespace B1ImGui
{
	bool DragFloatTyped(const char* Label, float* Value, float Speed, float Min, float Max, const char* Format)
	{
		ImGui::PushID(Label);
		const float DragWidth = FMath::Max(ImGui::CalcItemWidth() - TypeFieldWidth - ImGui::GetStyle().ItemInnerSpacing.x, 50.0f);
		ImGui::SetNextItemWidth(DragWidth);
		bool bChanged = ImGui::DragFloat("##drag", Value, Speed, Min, Max, Format, ImGuiSliderFlags_AlwaysClamp);
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::SetNextItemWidth(TypeFieldWidth);
		if (ImGui::InputFloat("##type", Value, 0.0f, 0.0f, Format))
		{
			*Value = FMath::Clamp(*Value, Min, Max);
			bChanged = true;
		}
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(Label);
		ImGui::PopID();
		return bChanged;
	}
}
#endif
