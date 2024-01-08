// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {iconSetToCSSBackgroundImageValue} from '../../common/js/util.js';
import {ICON_TYPES} from '../../foreground/js/constants.js';
import {Slice} from '../../lib/base_store.js';
import type {AndroidApp, State} from '../../state/state.js';

/**
 * @fileoverview Android apps slice of the store.
 *
 * Android App is something we get from private API
 * `chrome.fileManagerPrivate.getAndroidPickerApps`, it will be shown as a
 * directory item in FilePicker mode.
 */

const slice = new Slice<State, State['androidApps']>('androidApps');
export {slice as androidAppsSlice};

/** Action factory to add all android app config to the store. */
export const addAndroidApps = slice.addReducer('add', addAndroidAppsReducer);

function addAndroidAppsReducer(currentState: State, payload: {
  apps: chrome.fileManagerPrivate.AndroidApp[],
}): State {
  const androidApps: Record<string, AndroidApp> = {};
  for (const app of payload.apps) {
    // For android app item, if no icon is derived from IconSet, set the icon to
    // the generic one.
    let icon: string|chrome.fileManagerPrivate.IconSet = ICON_TYPES.GENERIC;
    if (app.iconSet) {
      const backgroundImage = iconSetToCSSBackgroundImageValue(app.iconSet);
      if (backgroundImage !== 'none') {
        icon = app.iconSet;
      }
    }
    androidApps[app.packageName] = {
      ...app,
      icon,
    };
  }
  return {
    ...currentState,
    androidApps,
  };
}
