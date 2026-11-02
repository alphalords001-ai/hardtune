@echo off
setlocal enabledelayedexpansion
title HardTune - compilation Windows

echo ============================================================
echo   HardTune  -  compilation du plugin VST3 sous Windows
echo ============================================================
echo.

rem ---- verification des outils -------------------------------------------
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERREUR] CMake introuvable.
    echo          Installe-le depuis https://cmake.org/download/
    echo          et coche "Add CMake to the system PATH".
    echo.
    pause
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo [ERREUR] Git introuvable.
    echo          Installe-le depuis https://git-scm.com/download/win
    echo.
    pause
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo [INFO] Compilateur MSVC absent du PATH courant.
    echo        Lance ce script depuis le "Developer Command Prompt for VS 2022"
    echo        OU installe Visual Studio 2022 avec la charge de travail
    echo        "Developpement Desktop en C++".
    echo.
)

rem ---- configuration ------------------------------------------------------
echo [1/3] Configuration du projet...
if not exist build mkdir build

cmake -S "%~dp0." -B "%~dp0build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo [ERREUR] La configuration a echoue. Copie le message ci-dessus.
    pause
    exit /b 1
)

rem ---- compilation --------------------------------------------------------
echo.
echo [2/3] Compilation (le premier build telecharge JUCE : 5 a 15 min)...
cmake --build "%~dp0build" --config Release --parallel
if errorlevel 1 (
    echo.
    echo [ERREUR] La compilation a echoue. Copie le message ci-dessus.
    pause
    exit /b 1
)

rem ---- resultat -----------------------------------------------------------
echo.
echo [3/3] Termine.
echo.
echo Fichiers produits :
for /r "%~dp0build" %%f in (*.vst3) do echo    %%f
echo.
echo Pour utiliser le plugin : copie le dossier .vst3 dans
echo    C:\Program Files\Common Files\VST3
echo (clic droit sur build.bat - "Executer en tant qu'administrateur",
echo  ou copie manuelle avec les droits admin).
echo.
pause
