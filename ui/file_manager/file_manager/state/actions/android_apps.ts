// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Actions for Android apps.
 *
 * Android App is something we get from private API
 * `chrome.fileManagerPrivate.getAndroidPickerApps`, it will be shown as
 * a directory item in FilePicker mode.
 */

/** Action to add all android app config to the store. */
export interface AddAndroidAppsAction extends BaseAction {
  type: ActionType.ADD_ANDROID_APPS;
  payload: {
    apps: chrome.fileManagerPrivate.AndroidApp[],
  };
}

/** Action factory to add all android app config to the store. */
export function addAndroidApps(payload: AddAndroidAppsAction['payload']):
    AddAndroidAppsAction {
  return {
    type: ActionType.ADD_ANDROID_APPS,
    payload,
  };
}
