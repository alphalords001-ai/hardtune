@echo off
setlocal enabledelayedexpansion
title HardTune - diagnostic de la machine
cd /d "%~dp0"

set "RAPPORT=%~dp0..\rapport-diagnostic.txt"

> "%RAPPORT%" echo ============================================================
>>"%RAPPORT%" echo   RAPPORT DE DIAGNOSTIC - HardTune
>>"%RAPPORT%" echo   Date : %DATE% %TIME%
>>"%RAPPORT%" echo ============================================================
>>"%RAPPORT%" echo.

rem ------------------------------------------------------------------ SYSTEME
>>"%RAPPORT%" echo --- SYSTEME ---
>>"%RAPPORT%" echo Windows    : %OS%
>>"%RAPPORT%" echo Architecture : %PROCESSOR_ARCHITECTURE%
ver >>"%RAPPORT%"

for /f "usebackq delims=" %%a in (`powershell -NoProfile -Command "(Get-CimInstance Win32_Processor).Name" 2^>nul`) do (
    >>"%RAPPORT%" echo CPU        : %%a
)
for /f "usebackq delims=" %%a in (`powershell -NoProfile -Command "[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)" 2^>nul`) do (
    >>"%RAPPORT%" echo RAM        : %%a Go
)
for /f "usebackq delims=" %%a in (`powershell -NoProfile -Command "[math]::Round((Get-PSDrive C).Free/1GB,1)" 2^>nul`) do (
    >>"%RAPPORT%" echo Disque C libre : %%a Go
)
>>"%RAPPORT%" echo.

rem ------------------------------------------------------------ OUTILS DE BUILD
>>"%RAPPORT%" echo --- OUTILS DE COMPILATION ---

rem Visual Studio 2022 + charge de travail C++
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%a in (`"%VSWHERE%" -latest -products * -property installationVersion 2^>nul`) do (
        >>"%RAPPORT%" echo Visual Studio : version %%a
    )
    for /f "usebackq delims=" %%a in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        >>"%RAPPORT%" echo [OK]     Compilateur C++ MSVC present : %%a
    )
    for /f "usebackq delims=" %%a in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if "%%a"=="" >>"%RAPPORT%" echo [ABSENT] Charge de travail "Desktop development with C++"
    )
) else (
    >>"%RAPPORT%" echo [ABSENT] Visual Studio 2022 (vswhere introuvable)
)

rem CMake
where cmake >nul 2>nul
if errorlevel 1 (
    >>"%RAPPORT%" echo [ABSENT] CMake
) else (
    for /f "usebackq delims=" %%a in (`cmake --version`) do (
        >>"%RAPPORT%" echo [OK]     %%a
        goto :cmake_done
    )
    :cmake_done
)

rem Git
where git >nul 2>nul
if errorlevel 1 (
    >>"%RAPPORT%" echo [ABSENT] Git
) else (
    for /f "usebackq delims=" %%a in (`git --version`) do (
        >>"%RAPPORT%" echo [OK]     %%a
        goto :git_done
    )
    :git_done
)
>>"%RAPPORT%" echo.

rem --------------------------------------------------------------------- DAW
>>"%RAPPORT%" echo --- DAW DETECTES ---
set "TRouv=0"
call :check "REAPER"                    "%ProgramFiles%\REAPER\reaper.exe"
call :check "REAPER (x86)"              "%ProgramFiles(x86)%\REAPER\reaper.exe"
call :check "FL Studio 21"              "%ProgramFiles%\Image-Line\FL Studio 21\FL64.exe"
call :check "FL Studio 20"              "%ProgramFiles%\Image-Line\FL Studio 20\FL64.exe"
call :check "FL Studio 2024"            "%ProgramFiles%\Image-Line\FL Studio 2024\FL64.exe"
call :check "Cubase 13"                 "%ProgramFiles%\Steinberg\Cubase 13\Cubase13.exe"
call :check "Cubase 12"                 "%ProgramFiles%\Steinberg\Cubase 12\Cubase12.exe"
call :check "Nuendo"                    "%ProgramFiles%\Steinberg\Nuendo 13\Nuendo13.exe"
call :check "Studio One 6"              "%ProgramFiles%\PreSonus\Studio One 6\Studio One.exe"
call :check "Studio One 7"              "%ProgramFiles%\PreSonus\Studio One 7\Studio One.exe"
call :check "Bitwig Studio"             "%ProgramFiles%\Bitwig Studio\Bitwig Studio.exe"
call :check "Ableton Live 12"           "%ProgramFiles%\Ableton\Live 12 Suite\Program\Ableton Live 12 Suite.exe"
call :check "Ableton Live 11"           "%ProgramFiles%\Ableton\Live 11 Suite\Program\Ableton Live 11 Suite.exe"
call :check "Cakewalk / Sonar"          "%ProgramFiles%\Cakewalk\Cakewalk Core\cakewalk.exe"
call :check "Waveform / Tracktion"      "%ProgramFiles%\Tracktion\Waveform\Waveform.exe"
if "%TRouv%"=="0" >>"%RAPPORT%" echo [ABSENT] Aucun DAW reconnu aux emplacements standards
>>"%RAPPORT%" echo.

rem ------------------------------------------------------------------- VST3
>>"%RAPPORT%" echo --- DOSSIER VST3 ---
if exist "%ProgramFiles%\Common Files\VST3" (
    >>"%RAPPORT%" echo [OK]     C:\Program Files\Common Files\VST3 existe
    >>"%RAPPORT%" echo Contenu (premiers elements) :
    dir /b "%ProgramFiles%\Common Files\VST3" >>"%RAPPORT%" 2>nul
) else (
    >>"%RAPPORT%" echo [ABSENT] C:\Program Files\Common Files\VST3
)
>>"%RAPPORT%" echo.

rem ------------------------------------------------------------------ VERDICT
>>"%RAPPORT%" echo --- VERDICT ---
>>"%RAPPORT%" echo Envoie ce fichier tel quel. Il ne contient que des noms de
>>"%RAPPORT%" echo logiciels et des versions : aucun fichier personnel.
>>"%RAPPORT%" echo Emplacement : %RAPPORT%

echo.
echo ============================================================
echo   Diagnostic termine.
echo   Le rapport est enregistre ici :
echo   %RAPPORT%
echo.
echo   Ouvre-le, copie tout son contenu et envoie-le moi.
echo ============================================================
echo.

start "" notepad "%RAPPORT%"
pause
exit /b 0

rem ---------------------------------------------------------------------------
:check
if exist "%~2" (
    >>"%RAPPORT%" echo [OK]     %~1
    set "TRouv=1"
    exit /b 0
)
>>"%RAPPORT%" echo [ABSENT] %~1
exit /b 0
