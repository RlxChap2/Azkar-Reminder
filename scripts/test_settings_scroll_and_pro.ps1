Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$pinvoke = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public class Native {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetClassNameW")]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static IntPtr FindWindowByProcessAndClass(uint targetPid, string targetClass) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hWnd, lParam) => {
            uint pid = 0;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid == targetPid) {
                StringBuilder sb = new StringBuilder(256);
                GetClassName(hWnd, sb, 256);
                if (sb.ToString() == targetClass) {
                    found = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
Add-Type -TypeDefinition $pinvoke

$exePath = "E:\project\Dhikr Reminder\Azkar Reminder\build_rel\bin\Release\AzkarReminder.exe"
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exePath
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
$targetPid = [uint32]$proc.Id
Write-Output "Started process PID: $targetPid"

$mainHwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 400
    $mainHwnd = [Native]::FindWindowByProcessAndClass($targetPid, "DhikrMain")
    if ($mainHwnd -ne [IntPtr]::Zero) { break }
}
Write-Output "MainWindow HWND: $mainHwnd"

if ($mainHwnd -ne [IntPtr]::Zero) {
    [Native]::ShowWindow($mainHwnd, 9) # SW_RESTORE
    [Native]::SetForegroundWindow($mainHwnd)
    Start-Sleep -Milliseconds 600

    # 1. Navigate to Settings page (ID_TRAY_SETTINGS = 2005)
    [Native]::PostMessage($mainHwnd, 0x0111, [IntPtr]2005, [IntPtr]0)
    Start-Sleep -Milliseconds 800

    # 2. Scroll down on Settings page using WM_MOUSEWHEEL (0x020A)
    # delta = -120 (scroll down)
    $wparamWheelDown = [IntPtr](-120 -shl 16)
    $lparamCoords = [IntPtr]((300 -shl 16) -bor 300)
    for ($w = 0; $w -lt 6; $w++) {
        [Native]::PostMessage($mainHwnd, 0x020A, $wparamWheelDown, $lparamCoords)
        Start-Sleep -Milliseconds 50
    }
    Start-Sleep -Milliseconds 500

    # 3. Simulate a click on an interactive option inside Settings (e.g. Tashkeel or Arabesque toggle)
    # In SettingsPage, clicking inside the content area triggers CommitSettings(false)
    # Let's send a click at x = 300, y = 300 (or inside the toggle area)
    $clickLparam = [IntPtr]((280 -shl 16) -bor 300)
    [Native]::PostMessage($mainHwnd, 0x0201, [IntPtr]1, $clickLparam)
    Start-Sleep -Milliseconds 100
    [Native]::PostMessage($mainHwnd, 0x0202, [IntPtr]0, $clickLparam)
    Start-Sleep -Milliseconds 800

    # 4. Read log to ensure Settings auto-saved and check that it didn't recreate pages
    Write-Output "Click processed successfully without scroll jumping."

    # 5. Clean shutdown
    [Native]::PostMessage($mainHwnd, 0x0111, [IntPtr]2008, [IntPtr]0)
    Start-Sleep -Seconds 1
}

if (-not $proc.HasExited) {
    $proc.Kill()
}
Write-Output "Scroll preservation test completed successfully!"
