// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Reducer for android apps.
 *
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

import {State} from '../../externs/ts/state.js';
import {AddAndroidAppsAction} from '../actions/android_apps.js';

export function addAndroidApps(
    currentState: State, action: AddAndroidAppsAction): State {
  const {apps} = action.payload;

  const androidApps: Record<string, chrome.fileManagerPrivate.AndroidApp> = {};
  for (const app of apps) {
    androidApps[app.packageName] = app;
  }
  return {
    ...currentState,
    androidApps,
  };
}
