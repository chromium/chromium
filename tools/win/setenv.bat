@ECHO off

REM Copyright 2017 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Run this script to add the current toolchain, as determined by
REM DEPOT_TOOLS_WIN_TOOLCHAIN and the hash in vs_toolchain.py,
REM to the path. Be aware of running this multiple times as too-long paths will
REM break things.

REM Execute whatever is printed by setenv.py. Use "CALL" to ensure that the
REM command title is reset when this script finishes executing.
FOR /f "usebackq tokens=*" %%a in (`python3 %~dp0setenv.py`) do CALL %%a
