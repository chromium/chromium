// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Actions for Chrome Preferences.
 *
 * Chrome preferences store user data that is persisted to disk OR across
 * profiles, this takes care of initially populating these values then keeping
 * them updated on dynamic changes.
 */

/** Action to update the chrome preferences to the store. */
export interface UpdatePreferencesAction extends BaseAction {
  type: ActionType.UPDATE_PREFERENCES;
  payload: chrome.fileManagerPrivate.PreferencesChange|
      chrome.fileManagerPrivate.Preferences;
}

/** Action factory to update the user preferences to the store. */
export function updatePreferences(payload: UpdatePreferencesAction['payload']):
    UpdatePreferencesAction {
  return {
    type: ActionType.UPDATE_PREFERENCES,
    payload,
  };
}
