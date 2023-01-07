#!/usr/bin/osascript

-- Copyright 2021 The Chromium Authors. All rights reserved.
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script makes sure that Safari is always in the same state to start
-- scenarios. This is because tab recovery is automatic and recovered tabs
-- need to be closed to get a clean slate.

tell application "Safari"
    activate
    reopen
    close (every tab of window 1)
    close every window
end tell
