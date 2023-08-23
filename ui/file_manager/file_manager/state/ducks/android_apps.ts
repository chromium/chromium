// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {addReducer, BaseAction, Reducer, ReducersMap} from '../../lib/base_store.js';
import {Action, ActionType} from '../actions.js';

/**
 * @fileoverview Reducer for android apps.
 *
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

/**
 * Actions and reducers for Android apps.
 *
 * Android App is something we get from private API
 * `chrome.fileManagerPrivate.getAndroidPickerApps`, it will be shown as
 * a directory item in FilePicker mode.
 */

/** Map of actions to reducers for the bulk pinning slice. */
export const androidAppsReducersMap: ReducersMap<State, Action> = new Map();

/** Action to add all android app config to the store. */
export interface AddAndroidAppsAction extends BaseAction {
  type: ActionType.ADD_ANDROID_APPS;
  payload: {
    apps: chrome.fileManagerPrivate.AndroidApp[],
  };
}

function addAndroidAppsReducer(
    currentState: State, payload: AddAndroidAppsAction['payload']): State {
  const {apps} = payload;

  const androidApps: Record<string, chrome.fileManagerPrivate.AndroidApp> = {};
  for (const app of apps) {
    androidApps[app.packageName] = app;
  }
  return {
    ...currentState,
    androidApps,
  };
}

/** Action factory to add all android app config to the store. */
export const addAndroidApps = addReducer(
    ActionType.ADD_ANDROID_APPS,
    addAndroidAppsReducer as Reducer<State, Action>, androidAppsReducersMap);
