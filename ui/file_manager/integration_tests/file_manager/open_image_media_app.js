// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {opensInMediaApp} from './open_media_app.js';

/**
 * Tests opening an image opens MediaApp/Backlight.
 * @param {string} path Directory path (Downloads or Drive).
 */
async function opensImageInMediaApp(path) {
  await opensInMediaApp(path, ENTRIES.image3);
}

// Exports test functions.
testcase.imageOpenMediaAppDownloads = () => {
  return opensImageInMediaApp(RootPath.DOWNLOADS);
};

testcase.imageOpenMediaAppDrive = () => {
  return opensImageInMediaApp(RootPath.DRIVE);
};
