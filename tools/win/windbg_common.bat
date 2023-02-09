REM Copyright 2021 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Helper batch file for windbg32.bat and windbg64.bat

FOR /f "usebackq tokens=*" %%a in (`python3 %~dp0win_sdk_dir.py`) do set win_sdk_dir=%%a
echo When debugging processes pass -g -G to suppress process start/end breakpoints, and pass -o to debug child processes.
echo Pass -z to load crash dumps.
echo When launching processes you should use windbg32.bat for x86 processes and you must use windbg64.bat for x64 processes.
