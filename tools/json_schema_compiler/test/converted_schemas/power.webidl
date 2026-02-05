// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum Level {
  // Prevents the system from sleeping in response to user inactivity.
  "system",

  // Prevents the display from being turned off or dimmed, or the system
  // from sleeping in response to user inactivity.
  "display"
};

// Use the <code>chrome.power</code> API to override the system's power
// management features.
interface Power {
  // Requests that power management be temporarily disabled. |level|
  // describes the degree to which power management should be disabled.
  // If a request previously made by the same app is still active, it
  // will be replaced by the new request.
  static undefined requestKeepAwake(Level level);

  // Releases a request previously made via requestKeepAwake().
  static undefined releaseKeepAwake();

  // Reports a user activity in order to awake the screen from a dimmed or
  // turned off state or from a screensaver. Exits the screensaver if it is
  // currently active.
  [platforms = ("chromeos")] static Promise<undefined> reportActivity();
};

partial interface Browser {
  static attribute Power power;
};
