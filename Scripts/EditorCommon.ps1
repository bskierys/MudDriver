# shared paths and lookups for the editor scripts; dot-source it

$EngineDir = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { "C:\Program Files\UE_5.8" }
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $ProjectDir "MudDriver.uproject"
$EditorExe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor.exe"
$EditorLog = Join-Path $ProjectDir "Saved\Logs\MudDriver.log"
$McpPort = 8001

# headless UnrealEditor-Cmd runs (RunGymTests) aren't matched, only the GUI editor
function Get-ProjectEditor {
    Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
        Where-Object { $_.CommandLine -like "*MudDriver.uproject*" }
}

function Test-McpPort {
    $Client = New-Object System.Net.Sockets.TcpClient
    try {
        $Connect = $Client.BeginConnect("127.0.0.1", $McpPort, $null, $null)
        return $Connect.AsyncWaitHandle.WaitOne(500) -and $Client.Connected
    } catch {
        return $false
    } finally {
        $Client.Close()
    }
}
