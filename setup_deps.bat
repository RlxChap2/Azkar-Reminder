@echo off
:: DhikrReminder — Dependency Bootstrap Script
:: Run this once to download required third-party files.

setlocal
cd /d "%~dp0"

echo.
echo ====================================
echo  Azkar Reminder — Dependency Setup
echo ====================================
echo.

:: Create directories
if not exist third_party mkdir third_party
if not exist assets\icons   mkdir assets\icons

:: ── 1. nlohmann/json (header-only) ────────────────────────────────────────
echo [1/3] Downloading nlohmann/json.hpp...
curl -L -o third_party\json.hpp "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
if %ERRORLEVEL% == 0 (
    echo   OK: third_party\json.hpp
) else (
    echo   FAILED. Please download manually from:
    echo   https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
)

:: ── 2. SQLite3 (amalgamation) ─────────────────────────────────────────────
echo.
echo [2/3] Downloading SQLite3 amalgamation...
curl -L -o sqlite3_amal.zip "https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip"
if %ERRORLEVEL% == 0 (
    powershell -command "Expand-Archive -Path sqlite3_amal.zip -DestinationPath sqlite3_tmp -Force"
    copy sqlite3_tmp\sqlite-amalgamation-3450000\sqlite3.h  third_party\sqlite3.h  >nul
    copy sqlite3_tmp\sqlite-amalgamation-3450000\sqlite3.c  third_party\sqlite3.c  >nul
    rmdir /s /q sqlite3_tmp
    del sqlite3_amal.zip
    echo   OK: third_party\sqlite3.h + sqlite3.c
) else (
    echo   FAILED. Please download manually from https://www.sqlite.org/download.html
)

:: ── 3. Application icons (generate placeholder with PowerShell) ────────────
echo.
echo [3/3] Generating placeholder application icons...
powershell -ExecutionPolicy Bypass -Command ^
  "Add-Type -AssemblyName System.Drawing;" ^
  "$bmp = New-Object System.Drawing.Bitmap 256,256;" ^
  "$g = [System.Drawing.Graphics]::FromImage($bmp);" ^
  "$g.Clear([System.Drawing.Color]::FromArgb(8,10,9));" ^
  "$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(183,243,74));" ^
  "$g.FillEllipse($brush, 40, 40, 176, 176);" ^
  "$g.FillEllipse([System.Drawing.Brushes]::FromArgb(8,10,9), 90, 30, 140, 140);" ^
  "$bmp.Save('assets\\icons\\app.ico');" ^
  "$bmp.Save('assets\\icons\\app_small.ico');" ^
  "Write-Host '  OK: Placeholder icons created'"

echo.
echo ====================================
echo  Dependencies ready.
echo  Next step:
echo    cmake -B build -DCMAKE_BUILD_TYPE=Release
echo    cmake --build build --config Release
echo ====================================
echo.
endlocal
