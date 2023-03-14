@ECHO off

REM Copyright 2017 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM This batch file executes the commands passed to it and prints out the
REM elapsed run time.

SETLOCAL
SET starttime=%time%
CALL %*
CALL python3 %~dp0subtract_time.py %time% %starttime%
