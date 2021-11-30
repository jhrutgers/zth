@echo off
rem Call this script to prepare the runtime environment on Windows for building the project.

set here=%~dp0
pushd %here%\..

set PATH=C:\Program Files\CMake\bin;%ChocolateyInstall%\lib\mingw\tools\install\mingw64\bin;%ChocolateyInstall%\bin;%PATH%

where /q cmake > NUL 2> NUL
if errorlevel 1 goto find_cmake
goto have_cmake
:find_cmake
where /q cmake > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
:have_cmake
echo Found CMake

rem Do not add git to the path, as sh.exe will be there also, which conflicts with make.
rem set PATH=C:\Program Files\Git\bin;%PATH%
where /q git > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
echo Found git

where /q gcc > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
echo Found gcc

where /q make > NUL 2> NUL
if errorlevel 1 goto need_bootstrap
echo Found make

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

