#!/usr/bin/osascript

-- Copyright 2021 The Chromium Authors. All rights reserved.
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script repeatedly sends a key to ensure the system considers
-- the user active.

tell application "System Events"
  repeat
    -- Send "shift" keycode.
	  key code 57
    -- user_idle_level is reset after 5min.
    delay 120.0
  end repeat
end tell
