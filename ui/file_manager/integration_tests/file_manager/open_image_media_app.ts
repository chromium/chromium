// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';

import {opensInMediaApp} from './open_media_app.js';

/**
 * Tests opening an image opens MediaApp/Backlight.
 * @param path Directory path (Downloads or Drive).
 */
async function opensImageInMediaApp(path: string) {
  await opensInMediaApp(path, ENTRIES.image3);
}

// Exports test functions.
export function imageOpenMediaAppDownloads() {
  return opensImageInMediaApp(RootPath.DOWNLOADS);
}

export function imageOpenMediaAppDrive() {
  return opensImageInMediaApp(RootPath.DRIVE);
}
