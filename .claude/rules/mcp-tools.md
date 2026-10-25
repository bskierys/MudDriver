# Unreal MCP rules

Applies whenever you use the `unreal-mcp` server (Epic's ModelContextProtocol plugin, `127.0.0.1:8001/mcp`). It runs in tool-search mode: only `list_toolsets`, `describe_toolset` and `call_tool` are exposed. Everything else is reached through `call_tool`.

## Calling tools

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
- **New actors in World Partition maps** (`Lvl_Offroad`) live in their own `__ExternalActors__` package. `save_actor` and `save_assets([map])` both skip a package that isn't on disk yet. `save_assets([])` (save all dirty) writes it; check `git status` after.
- **WP landscape isn't loaded in the editor**, so `SceneTools.trace_world` hits nothing there. Place actors relative to known actors (e.g. PlayerStart) and let PIE settle them.
- **`ActorTools.set_actor_transform` resets omitted fields.** Despite the schema ("unset = don't change"), passing only `rotation` moved the actor to the origin. Always pass location, rotation and scale.
- **Collision profiles on static mesh actors:** set `bUseDefaultCollision=false` on the component first, then `BodyInstance` `collisionProfileName`, `objectType` and `collisionEnabled`. With default collision on (BasicShapes do this), the mesh's BlockAll wins and the profile is silently ignored, even after a reload.
- **ObjectTools arg names:** `list_properties(instance)`, `get_properties(instance, properties[])`, `set_properties(instance, values)` where `values` is a JSON string.

## Not available

- Landscape sculpting, layer painting and foliage — do these manually or ask before building a custom toolset.
- Arbitrary editor Python.
- C++ header/layout changes in a running editor. Live Coding (`LiveCodingToolset`) only handles function-body edits; anything else needs a full build with the editor closed.

If a toolset you expect is missing from `list_toolsets`, its plugin isn't enabled in `MudDriver.uproject`. Toolset plugins live in `Engine/Plugins/Experimental/Toolsets/`.
