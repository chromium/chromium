@REM Copyright 2017 The Chromium Authors
@REM Use of this source code is governed by a BSD-style license that can be
@REM found in the LICENSE file.

:: This file should be run from the chromium source directory without arguments,
:: e.g. `third_party\win_build_output\remc.bat`
@ECHO Off

CALL gn gen out\gn

:: Add new targets and remove outdated ones as needed.
:: To determine the current set of targets, your best bet is to look for files
:: referencing "messages.gni"
CALL autoninja -C out\gn ^
    gen/chrome/common/win/eventlog_messages.h ^
    gen/chrome/credential_provider/eventlog/gcp_eventlog_messages.h ^
    gen/remoting/host/win/remoting_host_messages.h

:: Make sure we get rid of any obsolete files
rmdir /s /q third_party\win_build_output\mc

:: Copy all files with a .h, .rc, or .bin extension
:: Can't just copy the folder because there might be additional output
for %%e in (h rc bin) do (
  xcopy out\gn\gen\chrome\common\win\*.%%e ^
     third_party\win_build_output\mc\chrome\common\win\
  xcopy out\gn\gen\chrome\credential_provider\eventlog\*.%%e ^
     third_party\win_build_output\mc\chrome\credential_provider\eventlog\
  xcopy out\gn\gen\remoting\host\win\*.%%e ^
     third_party\win_build_output\mc\remoting\host\win\
)
