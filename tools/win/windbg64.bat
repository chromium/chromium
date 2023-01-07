@echo off

REM Copyright 2021 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Helper batch file to launch windbg, either from a local install or from the
REM packaged Chromium toolchain.
REM There are windbg32.bat and windbg64.bat flavors of this batch file - keep
REM them synchronized when making changes.

@setlocal

REM Print helpful messages and set win_sdk_dir
call %~dp0windbg_common.bat

@echo on
"%win_sdk_dir%\Debuggers\x64\windbg.exe" %*
