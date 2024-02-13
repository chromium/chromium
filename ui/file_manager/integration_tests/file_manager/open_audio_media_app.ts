// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';

import {opensInMediaApp} from './open_media_app.js';

/**
 * Tests opening audio opens MediaApp/Backlight.
 * @param path Directory path (Downloads or Drive).
 */
async function opensAudioInMediaApp(path: string) {
  await opensInMediaApp(path, ENTRIES.beautiful);
}

// Exports test functions.
export function audioOpenDrive() {
  return opensAudioInMediaApp(RootPath.DRIVE);
}

export function audioOpenDownloads() {
  return opensAudioInMediaApp(RootPath.DOWNLOADS);
}
