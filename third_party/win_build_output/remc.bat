@REM Copyright 2017 The Chromium Authors
@REM Use of this source code is governed by a BSD-style license that can be
@REM found in the LICENSE file.

ninja -C out\gn ^
    gen/base/trace_event/etw_manifest/chrome_events_win.h ^
    gen/chrome/common/win/eventlog_messages.h ^
    gen/remoting/host/win/remoting_host_messages.h
