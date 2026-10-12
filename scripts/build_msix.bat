@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo     Azkar Reminder - Microsoft Store MSIX Packager
echo ===================================================

cd /d "%~dp0\.."

set "BUILD_DIR=build_rel"
set "STAGE_DIR=package_staging"
set "OUTPUT_DIR=release_msix"

echo [1/5] Configuring CMake (Release)...
cmake -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [2/5] Building Release binary...
cmake --build %BUILD_DIR% --config Release
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo [3/5] Staging files...
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"
mkdir "%STAGE_DIR%\Assets"
mkdir "%STAGE_DIR%\audio"

copy "%BUILD_DIR%\bin\Release\AzkarReminder.exe" "%STAGE_DIR%\" 2>nul
if not exist "%STAGE_DIR%\AzkarReminder.exe" (
    copy "%BUILD_DIR%\bin\AzkarReminder.exe" "%STAGE_DIR%\" 2>nul
)

if not exist "%STAGE_DIR%\AzkarReminder.exe" (
    echo [ERROR] Could not find compiled AzkarReminder.exe
    exit /b 1
)

copy "package\AppxManifest.xml" "%STAGE_DIR%\"
xcopy "package\Assets\*" "%STAGE_DIR%\Assets\" /s /e /y >nul
if exist "%BUILD_DIR%\bin\audio" (
    xcopy "%BUILD_DIR%\bin\audio\*" "%STAGE_DIR%\audio\" /s /e /y >nul
) else (
    xcopy "assets\audio\*" "%STAGE_DIR%\audio\" /s /e /y >nul
)

echo [4/5] Locating Windows SDK MakeAppx.exe...
set "MAKEAPPX="
for /f "tokens=*" %%i in ('powershell -NoProfile -Command "(Get-Item 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\makeappx.exe' -ErrorAction SilentlyContinue | Select-Object -Last 1).FullName"') do (
    set "MAKEAPPX=%%i"
)

if "%MAKEAPPX%"=="" (
    echo [WARNING] MakeAppx.exe not found in standard Windows Kits path.
    echo Staged files are ready in "%STAGE_DIR%".
    echo To pack on your machine, run:
    echo makeappx.exe pack /d "%STAGE_DIR%" /p "AzkarReminder_v1.2.msix" /nv
    exit /b 0
)

echo Found MakeAppx: "%MAKEAPPX%"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
set "MSIX_PATH=%OUTPUT_DIR%\AzkarReminder_v1.2.0.msix"

echo [5/5] Creating MSIX Package: %MSIX_PATH%...
"%MAKEAPPX%" pack /d "%STAGE_DIR%" /p "%MSIX_PATH%" /nv /o
if errorlevel 1 (
    echo [ERROR] MakeAppx packing failed.
    exit /b 1
)

echo ===================================================
echo [SUCCESS] MSIX package successfully created at:
echo %MSIX_PATH%
echo Ready for Microsoft Partner Center Store submission!
echo ===================================================
endlocal
