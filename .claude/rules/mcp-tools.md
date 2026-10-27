# Unreal MCP rules

Applies whenever you use the `unreal-mcp` server (Epic's ModelContextProtocol plugin, `127.0.0.1:8001/mcp`). It runs in tool-search mode: only `list_toolsets`, `describe_toolset` and `call_tool` are exposed. Everything else is reached through `call_tool`.

## Calling tools

- **If the MCP tools aren't registered** (the server failed at session start, ECONNREFUSED), start the editor and POST JSON-RPC to `http://127.0.0.1:8001/mcp` from PowerShell: `initialize`, keep the `Mcp-Session-Id` response header, then `tools/call` with name `call_tool` and the same `toolset_name` / `tool_name` / `arguments` (verified 2026-10-26).

- **`call_tool` takes the short tool name plus `toolset_name`.** `describe_toolset` prints fully qualified names, but passing those to `call_tool` fails with `Unknown tool`.

  ```json
  { "toolset_name": "editor_toolset.toolsets.scene.SceneTools", "tool_name": "get_current_level" }
  ```

- **Object arguments are `{"refPath": "..."}`.** Actors, components, classes and assets are passed and returned this way. Pass returned refs back as-is.
- **Classes are refs too**, e.g. `{"refPath": "/Script/Engine.StaticMeshActor"}` or `/Game/.../BP_Foo.BP_Foo_C`.

## Context cost

- `describe_toolset` on `BlueprintTools` returns ~72k chars. Describe it once per session, never in a loop.
- For graph work, prefer `get_graph_dsl_docs` + `read_graph_dsl` / `write_graph_dsl` over many `create_node` / `connect_pins` calls.
- For multi-step edits, use `ProgrammaticToolset.execute_tool_script` to batch calls. Call `get_execution_environment` first and read its `instructions`. The sandbox only orchestrates tools; it isn't general editor Python.

## Editing

- **Nothing is saved automatically.** Finish with `AssetTools.save_assets` (assets) or `SceneTools.save_actor` (level actors).
- **Property names can't be guessed.** Run `ObjectTools.list_properties` before `get_properties` / `set_properties`. Setting an unknown name can fail silently.
- **Blueprint defaults** live on the CDO: `BlueprintTools.get_default_object`, then `ObjectTools.set_properties`. Compile with `compile_blueprint` after structural changes.
- **Verify visually** with `EditorAppToolset.CaptureViewport` / `CaptureAssetImage` after level or asset edits.
- **New actors in World Partition maps** (all `B1/Gym` maps) live in their own `__ExternalActors__` package. `save_actor` and `save_assets([map])` both skip a package that isn't on disk yet. `save_assets([])` (save all dirty) writes it; check `git status` after.
- **WP landscape isn't loaded in the editor**, so `SceneTools.trace_world` hits nothing there. Place actors relative to known actors (e.g. PlayerStart) and let PIE settle them.
- **`ActorTools.set_actor_transform` resets omitted fields.** Despite the schema ("unset = don't change"), passing only `rotation` moved the actor to the origin. Always pass location, rotation and scale.
- **Collision profiles on static mesh actors:** set `bUseDefaultCollision=false` on the component first, then `BodyInstance` `collisionProfileName`, `objectType` and `collisionEnabled`. With default collision on (BasicShapes do this), the mesh's BlockAll wins and the profile is silently ignored, even after a reload.
- **New BP from a C++ class:** `BlueprintTools.create(folder_path, asset_name, asset_type={"refPath": "/Script/MudDriver.<Class>"})`.
- **Viewport/asset captures** return base64 PNG that overflows the tool output. Decode the saved result file to a PNG in the scratchpad and Read it. `CaptureViewport` needs an `annotations` object; pass `gridSpacing: 0, maxLabelDistance: 0` to disable the overlays.
- **Capturing PIE/Simulate:** `CaptureViewport` with a `captureTransform` renders the editor world, not the play world. Set the camera with `SetCameraTransform` before `StartPIE` (Simulate keeps the editor camera), then use `CaptureEditorImage`.
- **Array properties:** `set_properties` can't resize an array and change its existing elements in one call ("insertion points are ambiguous"). Set the elements at the current size first, then the full array.
- **ObjectTools arg names:** `list_properties(instance)`, `get_properties(instance, properties[])`, `set_properties(instance, values)` where `values` is a JSON string.

## Not available

- Landscape sculpting, layer painting and foliage — do these manually or ask before building a custom toolset.
- Arbitrary editor Python.
- Creating asset types without a toolset `create` (e.g. physical materials). `DataAssetTools.create` only takes `UDataAsset` subclasses. Ask the user to make one by hand, then `AssetTools.duplicate` it for variants.
- `StartPIE`'s `startTransform` doesn't move the player when the game mode picks a PlayerStart. To test a spot, place a rig there (it spawns its own vehicle).
- C++ header/layout changes in a running editor. Live Coding (`LiveCodingToolset`) only handles function-body edits; anything else needs a full build with the editor closed. Run `Scripts/BuildEditor.ps1` ([editor-lifecycle.md](editor-lifecycle.md)).

If a toolset you expect is missing from `list_toolsets`, its plugin isn't enabled in `MudDriver.uproject`. Toolset plugins live in `Engine/Plugins/Experimental/Toolsets/`.
