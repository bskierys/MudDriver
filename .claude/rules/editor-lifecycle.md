# Editor lifecycle rules

Applies whenever a task needs the Unreal editor closed, opened or restarted (full C++ builds, headless tests, MCP work with no editor running). **Don't ask the user to close or open the editor. Use the scripts below.**

| Script | Does | Exit |
|---|---|---|
| `Scripts/StopEditor.ps1 [-Force] [-TimeoutSec 90]` | Closes the editor like the X button. Waits for exit and for LiveCodingConsole. | 0 closed or not running · 1 still open |
| `Scripts/StartEditor.ps1 [-Map /Game/...] [-TimeoutSec 900]` | Starts the editor and waits for the loaded main window plus the MCP port (8001). | 0 ready or already running · 1 crashed or timed out |
| `Scripts/BuildEditor.ps1 [-NoReopen] [-ForceClose]` | Stop, then `Build.bat MudDriverEditor`, then start again if it was open before. Drops `Makefile.bin` when new `.cpp`/`.h` files exist. | Build.bat's code, or 1 if the stop failed |

Paths are in `Scripts/EditorCommon.ps1`. Set `UE_ENGINE_DIR` to override the engine (default `C:\Program Files\UE_5.8`).

## Workflow

1. **Before closing:** stop PIE (`EditorAppToolset.StopPIE`) and save your own edits (`AssetTools.save_assets` / `SceneTools.save_actor`). Don't save-all just to get past the prompt: the user may have half-done edits open.
2. **Function-body-only C++ change:** use Live Coding (`LiveCodingToolset`). No restart.
3. **Header / UPROPERTY / new class / Build.cs change:** run `Scripts/BuildEditor.ps1` in the foreground with a timeout of about 10 min.
4. **Headless tests:** `Scripts/StopEditor.ps1`, then `Scripts/RunGymTests.ps1`, then `Scripts/StartEditor.ps1` if the editor was open before.
5. **After a start:** MCP reconnects by itself. The first `call_tool` works without `/mcp` (verified 2026-10-25).

## Failure handling

- **Stop exits 1** means a save-changes dialog is up (unsaved packages). Save what you changed and retry. If the dirty packages aren't yours, ask the user. Only pass `-Force` (kills the process, loses unsaved work) with the user's OK.
- **Start exits 1 or times out:** read the tail of `Saved/Logs/MudDriver.log`. A modal "rebuild modules?" prompt means the binaries are stale, so close it with `StopEditor.ps1 -Force` (nothing to lose at startup) and run `BuildEditor.ps1`.
- **Build fails:** the editor stays closed. Fix the errors and rerun `BuildEditor.ps1`. It only reopens the editor if it was open when the script started, so call `StartEditor.ps1` yourself after a failed run.
- **Closing a running editor is always OK** for these tasks. It doesn't need confirmation, because unsaved work is guarded by the save prompt.
