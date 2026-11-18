@echo off
setlocal
pushd "%~dp0" || exit /b 1
cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release || goto fail
cmake --build out/play --config Release --target raycast-2d || goto fail
if /i "%~1"=="--build-only" goto done
set "game=out\play\raycast-2d.exe"
if exist "out\play\Release\raycast-2d.exe" set "game=out\play\Release\raycast-2d.exe"
"%game%"
if errorlevel 1 goto fail
:done
popd
exit /b 0
:fail
popd
if "%~1"=="" pause
exit /b 1
