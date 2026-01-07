@echo off

rem SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
rem
rem SPDX-License-Identifier: MPL-2.0

set here=%~dp0
pushd "%here%"
call env.cmd
if errorlevel 1 goto silent_error

if not exist build mkdir build
if errorlevel 1 goto error

set build_type=Release

if "%~1" == "" goto build
set build_type=%~1

:build
pushd build
set "repo=%here%\..\.."
set "repo=%repo:\=/%"
for /f "tokens=1*" %%x in ("%*") do cmake -DCMAKE_MODULE_PATH="%repo%/dist/common" -DCMAKE_BUILD_TYPE=%build_type% "-GMinGW Makefiles" -DZTH_DIST=win32 -DZTH_DIST_DIR="%here%\." %%y ..\..\..
if errorlevel 1 goto error_popd
cmake --build . -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto error_popd
cmake --build . --target install -- -j%NUMBER_OF_PROCESSORS%
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

