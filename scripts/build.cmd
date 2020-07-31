@echo off
set here=%~dp0
pushd %here%\..
call scripts\env.cmd
if errorlevel 1 goto silent_error

if not exist build mkdir build
if errorlevel 1 goto error

git submodule init
if errorlevel 1 goto error
git submodule update
if errorlevel 1 goto error

pushd build
cmake -DCMAKE_BUILD_TYPE=Debug "-GMinGW Makefiles" -DCMAKE_PREFIX_PATH=%QT_DIR% ..
if errorlevel 1 goto error_popd
cmake --build . -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto error_popd
popd

:done
popd
exit /b 0

:error_popd
popd
:error
echo.
echo Error occurred, stopping
echo.
:silent_error
popd
exit /b 1

