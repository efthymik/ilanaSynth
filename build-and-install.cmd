@echo off
setlocal EnableExtensions EnableDelayedExpansion
title ilanaSynth - build and install

rem ==========================================================================
rem  Builds the ilanaSynth VST3 + standalone (Release) and ilanaSynth FX (the
rem  effect that plays audio input) and installs them:
rem    VST3       -> C:\Program Files\Common Files\VST3\ilanaSynth.vst3
rem    VST3 (FX)  -> C:\Program Files\Common Files\VST3\ilanaSynth FX.vst3
rem    CLAP       -> C:\Program Files\Common Files\CLAP\ilanaSynth.clap (and FX)
rem    Standalone -> C:\Program Files\ilanaSynth\ilanaSynth.exe
rem
rem  Needs: Visual Studio 2022 (or Build Tools) with "Desktop development
rem  with C++", Git, and internet the first time (JUCE is downloaded).
rem  CMake is used from PATH, or the copy bundled with Visual Studio.
rem ==========================================================================

rem --- Run as administrator (installing into Program Files needs it) ---------
net session >nul 2>&1
if errorlevel 1 (
    echo Asking for administrator rights to install the plugin...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

rem --- Find Visual Studio ----------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (
    echo.
    echo ERROR: Visual Studio 2022 with the C++ tools was not found.
    echo Install "Visual Studio 2022 Community" or "Build Tools for Visual Studio 2022"
    echo and tick "Desktop development with C++":
    echo     https://visualstudio.microsoft.com/downloads/
    goto :fail
)
echo Visual Studio: %VSPATH%

rem --- Find CMake ------------------------------------------------------------
set "CMAKE=cmake"
where cmake >nul 2>&1
if errorlevel 1 (
    set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if not exist "!CMAKE!" (
        echo.
        echo ERROR: CMake was not found. Install it from https://cmake.org/download/
        echo or add the "C++ CMake tools for Windows" component in Visual Studio.
        goto :fail
    )
)
echo CMake: %CMAKE%

rem --- Git (JUCE is fetched with it) -----------------------------------------
where git >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: Git was not found. Install it from https://git-scm.com/download/win
    goto :fail
)

rem --- Configure and build ---------------------------------------------------
echo.
echo === Configuring (the first run downloads JUCE, this can take a while) ===
"%CMAKE%" -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo Configure failed. If you changed Visual Studio versions, delete the
    echo "build" folder next to this script and run it again.
    goto :fail
)

echo.
echo === Building the VST3s and standalone (Release) ===
"%CMAKE%" --build build --config Release --target ilanaSynth_VST3 ilanaSynth_Standalone ilanaSynthFX_VST3 ilanaSynth_CLAP ilanaSynthFX_CLAP --parallel
if errorlevel 1 goto :fail

set "VST3_SRC=%~dp0build\ilanaSynth_artefacts\Release\VST3\ilanaSynth.vst3"
set "APP_SRC=%~dp0build\ilanaSynth_artefacts\Release\Standalone\ilanaSynth.exe"
set "FX_SRC=%~dp0build\ilanaSynthFX_artefacts\Release\VST3\ilanaSynth FX.vst3"
if not exist "%VST3_SRC%" (
    echo ERROR: build finished but "%VST3_SRC%" is missing.
    goto :fail
)

rem --- Install ---------------------------------------------------------------
set "COMMON=%CommonProgramW6432%"
if not defined COMMON set "COMMON=%CommonProgramFiles%"
set "PROGRAMS=%ProgramW6432%"
if not defined PROGRAMS set "PROGRAMS=%ProgramFiles%"
set "VST3_DST=%COMMON%\VST3\ilanaSynth.vst3"
set "FX_DST=%COMMON%\VST3\ilanaSynth FX.vst3"
set "APP_DST=%PROGRAMS%\ilanaSynth"

rem --- Back up what is installed now (so a bad build can be rolled back) ---
for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmm"`) do set "STAMP=%%T"
if not defined STAMP set "STAMP=unknown-time"
set "BACKUP=%USERPROFILE%\ilanaSynth-backups\%STAMP%-before-install"
if exist "%VST3_DST%" call :backupdir "%VST3_DST%" "%BACKUP%\ilanaSynth.vst3"
if errorlevel 1 goto :fail
if exist "%FX_DST%" call :backupdir "%FX_DST%" "%BACKUP%\ilanaSynth FX.vst3"
if errorlevel 1 goto :fail
set "CLAP_DST=%COMMON%\CLAP"
for %%N in ("ilanaSynth.clap" "ilanaSynth FX.clap") do (
    if exist "%CLAP_DST%\%%~N" (
        if not exist "%BACKUP%\CLAP" mkdir "%BACKUP%\CLAP"
        copy /y "%CLAP_DST%\%%~N" "%BACKUP%\CLAP\" >nul
    )
)
if exist "%BACKUP%" echo Previous install saved to "%BACKUP%"

echo.
echo === Installing the VST3 to "%VST3_DST%" ===
robocopy "%VST3_SRC%" "%VST3_DST%" /MIR /R:2 /W:2 /NFL /NDL /NJH /NJS /NP
if errorlevel 8 goto :copyfail

if exist "%FX_SRC%" (
    echo === Installing ilanaSynth FX to "%FX_DST%" ===
    robocopy "%FX_SRC%" "%FX_DST%" /MIR /R:2 /W:2 /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 goto :copyfail
)

for %%P in ("%~dp0build\ilanaSynth_artefacts\Release\CLAP\ilanaSynth.clap" "%~dp0build\ilanaSynthFX_artefacts\Release\CLAP\ilanaSynth FX.clap") do (
    if exist %%P (
        if not exist "%CLAP_DST%" mkdir "%CLAP_DST%"
        echo === Installing %%~nxP to "%CLAP_DST%" ===
        copy /y %%P "%CLAP_DST%\" >nul
        if errorlevel 1 goto :copyfail
    )
)

if exist "%APP_SRC%" (
    echo === Installing the standalone app to "%APP_DST%" ===
    if not exist "%APP_DST%" mkdir "%APP_DST%"
    copy /y "%APP_SRC%" "%APP_DST%\ilanaSynth.exe" >nul
    if errorlevel 1 (
        echo WARNING: could not copy the standalone app ^(is it running?^).
    ) else (
        powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('CommonDesktopDirectory')+'\ilanaSynth.lnk'); $s.TargetPath='%APP_DST%\ilanaSynth.exe'; $s.Save()" >nul 2>&1
    )
)

echo.
echo ==========================================================================
echo  Done. ilanaSynth is installed.
echo    VST3:       %VST3_DST%
echo    VST3 (FX):  %FX_DST%
echo    CLAP:       %CLAP_DST%
echo    Standalone: %APP_DST%\ilanaSynth.exe  (shortcut on the desktop)
echo  Rescan plugins in your DAW to pick it up.
echo ==========================================================================
if exist "%BACKUP%" echo  Previous install: %BACKUP%
if exist "%~dp0build\build-label.txt" (
    for /f "usebackq delims=" %%L in ("%~dp0build\build-label.txt") do echo Build: %%L
)
echo.
pause
exit /b 0

:backupdir
rem  %1 = installed folder, %2 = backup folder. Fails (errorlevel 1) if the copy fails.
robocopy %1 %2 /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP
if errorlevel 8 (
    echo.
    echo ERROR: could not back up %1 to %2.
    exit /b 1
)
exit /b 0

:copyfail
echo.
echo ERROR: could not copy the plugin files. Close Ableton Live ^(and any other
echo DAW or the standalone app^) and run this script again. The previous install
echo if any, was saved to "%BACKUP%".
pause
exit /b 1

:fail
echo.
echo Build/install did not finish. Scroll up for the error.
pause
exit /b 1
