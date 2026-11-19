@echo off
setlocal
pushd "%~dp0" || exit /b 1
cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release || goto fail
cmake --build out/play --config Release --target raycast-2d || goto fail
if /i "%~1"=="--build-only" goto done
set "GAME_EXE=%cd%\out\play\raycast-2d.exe"
if exist "%cd%\out\play\Release\raycast-2d.exe" set "GAME_EXE=%cd%\out\play\Release\raycast-2d.exe"
if "%~1"=="" (
    start "" "%GAME_EXE%" --config "%cd%\assets\game.cfg"
) else (
    start "" "%GAME_EXE%" %*
)
:done
popd
exit /b 0
:fail
popd
if "%~1"=="" pause
exit /b 1
