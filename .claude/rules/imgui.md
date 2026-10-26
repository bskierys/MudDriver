# NetImGui rules

Applies to every NetImGui window in `Source/MudDriver/Debug/B1ImGui*`.

## Tunable values

- **Every editable float gets a typed-value field next to its drag.** Use `B1ImGui::DragFloatTyped` from `Debug/B1ImGuiWidgets.h`. Don't call `ImGui::DragFloat` / `SliderFloat` directly for tunables. A drag alone can't hit an exact value, and the user asked for this twice (Mud window, then Vehicle Status).
- Curve keys follow the same pattern (drag + `InputFloat2`), see `DrawCurve` in `B1ImGuiMud.cpp`.
- If a new kind of tunable control is needed (int, vector, angle), add a typed variant to `B1ImGuiWidgets` rather than inlining one.

## Asset-backed values

- Edits apply to the loaded asset object, call `MarkPackageDirty()`, and get a "Save to asset" button (`WITH_EDITOR` only) that calls `B1ImGui::SaveAssetPackage` from `Debug/B1ImGuiAssetSave.h`.
