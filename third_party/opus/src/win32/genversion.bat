@echo off

setlocal enableextensions enabledelayedexpansion

for /f %%v in ('cd "%~dp0.." ^&^& git status ^>NUL 2^>NUL ^&^& git describe --tags --match "v*" --dirty 2^>NUL') do set version=%%v

if not "%version%"=="" set version=!version:~1! && goto :gotversion

if exist "%~dp0..\package_version" goto :getversion

echo Git cannot be found, nor can package_version. Generating unknown version.

set version=unknown

goto :gotversion

:getversion

for /f "delims== tokens=2" %%v in (%~dps0..\package_version) do set version=%%v
set version=!version:"=!

:gotversion

set version=!version: =!
set version_out=#define %~2 "%version%"

echo %version_out%> "%~1_temp"

echo n | comp "%~1_temp" "%~1" > NUL 2> NUL

if not errorlevel 1 goto exit

copy /y "%~1_temp" "%~1"

:exit

del "%~1_temp"
