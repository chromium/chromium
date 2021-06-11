// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests opening a video opens MediaApp/Backlight.
   * @param {string} path Directory path (Downloads or Drive).
   */
  async function opensVideoInMediaApp(path) {
    await opensInMediaApp(path, ENTRIES.world);
  }

  // Exports test functions.
  testcase.videoOpenDrive = () => {
    return opensVideoInMediaApp(RootPath.DRIVE);
  };

  testcase.videoOpenDownloads = () => {
    return opensVideoInMediaApp(RootPath.DOWNLOADS);
  };
})();
