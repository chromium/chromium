ECHO OFF

REM Copyright 2012 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Copies an installer and symbols from a build directory on a network share
REM into the directory \[out|build]\[Debug|Release] on the current drive.
REM
REM Usage:
REM   \\build.share\<path_to_checkout>\src\tools\win\copy-installer.bat
REM
REM By default, the script will copy the Debug build in the tree, falling back
REM to the Release build if one is not found.  Similarly, the ninja output
REM directory is preferred over the devenv output directory.  The x86 build is
REM preferred over the x64 build.  Specify any of "out|build", "Debug|Release"
REM (case matters), or "x64" on the command line in any order to influence
REM selection.  The defaults for location and build type can also be overridden
REM in a given build tree by creating a "copy-installer.cfg" file alongside the
REM .gclient file that sets any of OUTPUT, BUILDTYPE, or ARCH variables.
REM
REM Install Robocopy for superior performance on Windows XP if desired (it is
REM present by default on Vista+).

SETLOCAL

REM Get the path to the build tree's src directory.
CALL :_canonicalize "%~dp0..\.."
SET FROM=%RET%

REM Read local configuration (set OUTPUT and BUILDTYPE there).
IF EXIST "%FROM%\..\copy-installer.cfg" CALL "%FROM%\..\copy-installer.cfg"

REM Read any of OUTPUT, BUILDTYPE, or ARCH from command line.
FOR %%a IN (%1 %2) do (
IF "%%a"=="out" SET OUTPUT=out
IF "%%a"=="build" SET OUTPUT=build
IF "%%a"=="Debug" SET BUILDTYPE=Debug
IF "%%a"=="Release" SET BUILDTYPE=Release
IF "%%a"=="x64" SET ARCH=_x64
)

CALL :_find_build
IF "%OUTPUT%%BUILDTYPE%%ARCH%"=="" (
ECHO No build found to copy.
EXIT 1
)

SET FROM=%FROM%\%OUTPUT%\%BUILDTYPE%%ARCH%
SET TO=\%OUTPUT%\%BUILDTYPE%%ARCH%

SET TOCOPY=mini_installer.exe *.dll.pdb chrome.exe.pdb mini_installer.exe.pdb^
           setup.exe.pdb

CALL :_copyfiles

REM incremental_chrome_dll=1 puts chrome_dll.pdb into the "initial" dir.
IF EXIST "%FROM%\initial" (
SET FROM=%FROM%\initial
SET TOCOPY=*.pdb
CALL :_copyfiles
)

ECHO Ready to run/debug %TO%\mini_installer.exe.
GOTO :EOF

REM All labels henceforth are subroutines intended to be invoked by CALL.

REM Canonicalize the first argument, returning it in RET.
:_canonicalize
SET RET=%~f1
GOTO :EOF

REM Search for a mini_installer.exe in the candidate build outputs.
:_find_build
IF "%OUTPUT%"=="" (
SET OUTPUTS=out build
) ELSE (
SET OUTPUTS=%OUTPUT%
SET OUTPUT=
)

IF "%BUILDTYPE%"=="" (
SET BUILDTYPES=Debug Release
) ELSE (
SET BUILDTYPES=%BUILDTYPE%
SET BUILDTYPE=
)

FOR %%o IN (%OUTPUTS%) DO (
FOR %%f IN (%BUILDTYPES%) DO (
IF EXIST "%FROM%\%%o\%%f\mini_installer.exe" (
SET OUTPUT=%%o
SET BUILDTYPE=%%f
GOTO :EOF
)
IF EXIST "%FROM%\%%o\%%f_x64\mini_installer.exe" (
SET OUTPUT=%%o
SET BUILDTYPE=%%f
SET ARCH=_x64
GOTO :EOF
)
)
)
GOTO :EOF

REM Branch to handle copying via robocopy (fast) or xcopy (slow).
:_copyfiles
robocopy /? 1> nul 2> nul
IF NOT "%ERRORLEVEL%"=="9009" (
robocopy "%FROM%" "%TO%" %TOCOPY% /MT /XX
) ELSE (
IF NOT EXIST "%TO%" mkdir "%TO%"
call :_xcopy_hack %TOCOPY%
)
GOTO :EOF

REM We can't use a for..in..do loop since we have wildcards, so we make a call
REM to this with the files to copy.
:_xcopy_hack
SHIFT
IF "%0"=="" GOTO :EOF
xcopy "%FROM%\%0" "%TO%" /d /y
GOTO _xcopy_hack
