@echo off

rem SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
rem
rem SPDX-License-Identifier: MPL-2.0

rem Call this script to prepare the runtime environment on Windows for building the project.

set here=%~dp0
pushd %here%\..\..

set PATH=%ChocolateyInstall%\lib\mingw\tools\install\mingw64\bin;%ChocolateyInstall%\bin;%PATH%

echo Checking dependencies...

where /q cmake > NUL 2> NUL
if errorlevel 1 goto find_cmake
goto have_cmake
:find_cmake
set PATH=%ProgramFiles%\CMake\bin;%PATH%
where /q cmake > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
:have_cmake
where cmake 2> NUL | cmd /e /v /q /c"set/p.=&&echo CMake: ^!.^!"

rem Do not add git to the path, as sh.exe will be there also, which conflicts with make.
rem set PATH=%ProgramFiles%\Git\bin;%PATH%
where /q git > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
where git 2> NUL | cmd /e /v /q /c"set/p.=&&echo git: ^!.^!"

where /q gcc > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
where gcc 2> NUL | cmd /e /v /q /c"set/p.=&&echo gcc: ^!.^!"

where /q make > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
where make 2> NUL | cmd /e /v /q /c"set/p.=&&echo make: ^!.^!"

where /q python > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
where python 2> NUL | cmd /e /v /q /c"set/p.=&&echo python: ^!.^!"

if exist dist\venv goto done
echo Setting up Python virtual environment...
python -m venv dist\venv
if errorlevel 1 goto error
call dist\venv\Scripts\activate.bat
if errorlevel 1 goto error
python -m pip install --upgrade pip setuptools wheel
if errorlevel 1 goto error
python -m pip install -r dist\common\requirements.txt
if errorlevel 1 goto error

:done
popd
exit /b 0

:need_bootstrap
echo Missing dependencies. Run bootstrap first.

:error
echo.
echo Error occurred, stopping
echo.
popd
exit /b 1

