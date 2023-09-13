// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Slice} from '../../lib/base_store.js';

/**
 * @fileoverview Android apps slice of the store.
 * @suppress {checkTypes}
 *
 * Android App is something we get from private API
 * `chrome.fileManagerPrivate.getAndroidPickerApps`, it will be shown as a
 * directory item in FilePicker mode.
 */

const slice = new Slice<State, State['androidApps']>('androidApps');
export {slice as androidAppsSlice};

/** Action factory to add all android app config to the store. */
export const addAndroidApps =
    slice.addReducer('add', addAndroidAppsReducer);

function addAndroidAppsReducer(currentState: State, payload: {
  apps: chrome.fileManagerPrivate.AndroidApp[],
}): State {
  const androidApps: Record<string, chrome.fileManagerPrivate.AndroidApp> = {};
  for (const app of payload.apps) {
    androidApps[app.packageName] = app;
  }
  return {
    ...currentState,
    androidApps,
  };
}
