Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$pinvoke = @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public class Native {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetClassNameW")]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetWindowTextW")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

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
                    return false; // stop enum
                }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
Add-Type -TypeDefinition $pinvoke

$artifactDir = "C:\Users\kamah\.gemini\antigravity\brain\3a88f21f-7118-44fe-9ec6-e2dae6026bd8"

function Capture-Window($hwnd, $filename) {
    $rc = New-Object Native+RECT
    [Native]::GetWindowRect($hwnd, [ref]$rc)
    $w = $rc.Right - $rc.Left
    $h = $rc.Bottom - $rc.Top
    if ($w -le 0 -or $h -le 0) {
        $w = 880
        $h = 620
    }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $res = [Native]::PrintWindow($hwnd, $hdc, 2) # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $outPath = Join-Path $artifactDir $filename
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Captured $filename via PrintWindow: success=$res ($w x $h)"
}

Write-Output "=== Starting AzkarReminder Verification ==="
$exePath = "E:\project\Dhikr Reminder\Azkar Reminder\build_rel\bin\Release\AzkarReminder.exe"

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exePath
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
$targetPid = [uint32]$proc.Id
Write-Output "Spawned process PID: $targetPid"

$mainHwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 400
    $mainHwnd = [Native]::FindWindowByProcessAndClass($targetPid, "DhikrMain")
    if ($mainHwnd -ne [IntPtr]::Zero) {
        break
    }
}
Write-Output "MainWindow HWND (DhikrMain): $mainHwnd"

if ($mainHwnd -ne [IntPtr]::Zero) {
    [Native]::ShowWindow($mainHwnd, 9) # SW_RESTORE
    [Native]::SetForegroundWindow($mainHwnd)
    Start-Sleep -Milliseconds 800

    Capture-Window $mainHwnd "screen_main_running.png"

    # Navigate to Settings Page (ID_TRAY_SETTINGS = 2005)
    Write-Output "Navigating to Settings Page..."
    [Native]::PostMessage($mainHwnd, 0x0111, [IntPtr]2005, [IntPtr]0)
    Start-Sleep -Milliseconds 1200

    Capture-Window $mainHwnd "screen_settings_page.png"

    # Trigger Reminder Popup (ID_TRAY_NEXT = 2004)
    Write-Output "Triggering Reminder Popup..."
    [Native]::PostMessage($mainHwnd, 0x0111, [IntPtr]2004, [IntPtr]0)
    Start-Sleep -Milliseconds 1500

    # Locate DhikrPopup window belonging to this process
    $popupHwnd = [Native]::FindWindowByProcessAndClass($targetPid, "DhikrPopup")
    Write-Output "Popup HWND (DhikrPopup): $popupHwnd"

    if ($popupHwnd -ne [IntPtr]::Zero) {
        Capture-Window $popupHwnd "screen_popup_custom.png"

        # Simulate click on Counter Pill (approx top-right inside popup: x=340, y=24)
        $rc = New-Object Native+RECT
        [Native]::GetWindowRect($popupHwnd, [ref]$rc)
        $clickX = 96
        $clickY = 17
        $lparam = [IntPtr](($clickY -shl 16) -bor ($clickX -band 0xFFFF))
        Write-Output "Clicking counter pill at x=$clickX, y=$clickY..."
        [Native]::PostMessage($popupHwnd, 0x0201, [IntPtr]1, $lparam) # WM_LBUTTONDOWN
        Start-Sleep -Milliseconds 100
        [Native]::PostMessage($popupHwnd, 0x0202, [IntPtr]0, $lparam) # WM_LBUTTONUP
        Start-Sleep -Milliseconds 800

        Capture-Window $popupHwnd "screen_popup_counter_clicked.png"
    } else {
        Write-Output "PopupWindow not found!"
    }

    # Clean shutdown via ID_TRAY_EXIT (2008)
    Write-Output "Shutting down test process..."
    [Native]::PostMessage($mainHwnd, 0x0111, [IntPtr]2008, [IntPtr]0)
    Start-Sleep -Seconds 1
}

if (-not $proc.HasExited) {
    $proc.Kill()
}
Write-Output "Verification complete."
