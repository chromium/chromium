// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {opensInMediaApp} from './open_media_app.js';

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
