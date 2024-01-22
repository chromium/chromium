// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {opensInMediaApp} from './open_media_app.js';

/**
 * Tests opening audio opens MediaApp/Backlight.
 * @param {string} path Directory path (Downloads or Drive).
 */
async function opensAudioInMediaApp(path) {
  // @ts-ignore: error TS4111: Property 'beautiful' comes from an index
  // signature, so it must be accessed with ['beautiful'].
  await opensInMediaApp(path, ENTRIES.beautiful);
}

// Exports test functions.
// @ts-ignore: error TS4111: Property 'audioOpenDrive' comes from an index
// signature, so it must be accessed with ['audioOpenDrive'].
testcase.audioOpenDrive = () => {
  return opensAudioInMediaApp(RootPath.DRIVE);
};

// @ts-ignore: error TS4111: Property 'audioOpenDownloads' comes from an index
// signature, so it must be accessed with ['audioOpenDownloads'].
testcase.audioOpenDownloads = () => {
  return opensAudioInMediaApp(RootPath.DOWNLOADS);
};
