@echo off

rem SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
rem
rem SPDX-License-Identifier: MPL-2.0

set here=%~dp0
pushd "%here%"

rem Usage: bootstrap.cmd [-f]
rem
rem Without -f, nothing is done when the dependencies are already fulfilled.
rem When -f is provided, choco is used to install the dependencies.

if "%1" == "-f" goto do_bootstrap

call env.cmd
if errorlevel 1 goto do_bootstrap
echo.
echo Your installation seems OK; skipping bootstrap.
echo To force installing the dependencies anyway, provide the -f flag to %0.
goto done

:do_bootstrap

net session > NUL 2> NUL
if errorlevel 1 goto no_admin
goto is_admin
:no_admin
echo Run this script as Administrator.
goto error
:is_admin

where /q choco > NUL 2> NUL
if errorlevel 1 goto no_choco
goto have_choco
:no_choco
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command " [System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
:choco_again
where /q choco > NUL 2> NUL
if errorlevel 1 goto still_no_choco
goto have_choco
:still_no_choco
echo Chocolatey not installed. Install from here: https://chocolatey.org/docs/installation
goto error
:have_choco

choco install -y --no-progress git cmake make mingw llvm
if errorlevel 1 goto error

:done
popd
exit /b 0

:error
echo.
echo Error occurred, stopping
echo.
:silent_error
popd
pause
exit /b 1

