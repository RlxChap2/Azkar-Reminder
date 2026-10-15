[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
# Native startup uses the Windows known-folder API and HKCU. Use a throwaway
# runner profile rather than redirecting or replacing a developer's data.
if ($env:CI -ne 'true') { throw 'Requires a disposable CI profile.' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$data = Join-Path $env:LOCALAPPDATA 'Azkar Reminder'
if (Test-Path -LiteralPath $data) { throw 'Existing application data found.' }
if (!('AzkarSmokeWindow' -as [type])) { Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AzkarSmokeWindow {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
}
'@
}
New-Item -ItemType Directory -Path $data | Out-Null
@{ firstLaunch = $false; startWithWindows = $false; remindersEnabled = $false; language = 'en' } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $data 'settings.json') -Encoding utf8
$process = $null
$clients = [Collections.Generic.List[Diagnostics.Process]]::new()
function Start-Reminder {
    $script:process = Start-Process -FilePath $Executable -ArgumentList '--background' -PassThru
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        if ($script:process.HasExited) { throw "Application exited: $($script:process.ExitCode)" }
        $window = [AzkarSmokeWindow]::FindWindow('DhikrMain', $null)
        [uint32]$ownerId = 0
        [void][AzkarSmokeWindow]::GetWindowThreadProcessId($window, [ref]$ownerId)
        if ($window -ne [IntPtr]::Zero -and $ownerId -eq $script:process.Id) { return $window }
        Start-Sleep -Seconds 1
    }
    throw 'Native main window was not created.'
}
function Stop-Reminder {
    $quit = Start-Process -FilePath $Executable -ArgumentList '--quit' -PassThru
    $clients.Add($quit)
    if (!$quit.WaitForExit(15000) -or $quit.ExitCode -ne 0) { throw 'Quit client failed.' }
    if (!$script:process.WaitForExit(15000) -or $script:process.ExitCode -ne 0) { throw 'Native shutdown failed.' }
    $script:process = $null
}
try {
    $window = Start-Reminder
    if ([AzkarSmokeWindow]::IsWindowVisible($window)) { throw 'Background startup displayed the main window.' }
    $second = Start-Process -FilePath $Executable -PassThru
    $clients.Add($second)
    if (!$second.WaitForExit(15000) -or $second.ExitCode -ne 0) { throw 'Single-instance launch failed.' }
    Start-Sleep -Seconds 1
    if (![AzkarSmokeWindow]::IsWindowVisible($window)) { throw 'Second launch did not show the existing window.' }
    Stop-Reminder
    Set-Content -LiteralPath (Join-Path $data 'content.json') -Value '[]' -NoNewline
    $null = Start-Reminder
    Stop-Reminder
    if ((Get-Content -Raw -LiteralPath (Join-Path $data 'content.json')) -ne '[]') { throw 'Empty library was replaced.' }
    Write-Host 'PASS: native window, background startup, single instance, IPC show/quit and empty library.'
} finally {
    foreach ($client in $clients) { if (!$client.HasExited) { Stop-Process -Id $client.Id -Force } }
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force }
    # The directory was absent before this script and created above.
    Remove-Item -LiteralPath $data -Recurse -Force
}
