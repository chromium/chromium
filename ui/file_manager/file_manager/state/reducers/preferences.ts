// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Reducer for preferences.
 *
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

import {State} from '../../externs/ts/state.js';
import {UpdatePreferencesAction} from '../actions/preferences.js';

/**
 * Type alises to avoid writing the `chrome.fileManagerPrivate` prefix.
 */
export type Preferences = chrome.fileManagerPrivate.Preferences;
export type PreferencesChange = chrome.fileManagerPrivate.PreferencesChange;

/**
 * A type guard to see if the payload supplied is a change of preferences or the
 * entire preferences object. Useful in ensuring subsequent type checks are done
 * on the correct type (instead of the union type).
 */
function isPreferencesChange(payload: Preferences|
                             PreferencesChange): payload is PreferencesChange {
  // The field `driveEnabled` is only on a `Preferences` object, so if this is
  // undefined the payload is a `Preferences` object otherwise it's a
  // `PreferencesChange` object.
  if ((payload as Preferences).driveEnabled !== undefined) {
    return false;
  }
  return true;
}

/**
 * Only update the existing preferences with their new values if they are
 * defined. In the event of spreading the change event over the existing
 * preferences, undefined values should not overwrite their existing values.
 */
function updateIfDefined(
    updatedPreferences: Preferences, newPreferences: PreferencesChange,
    key: keyof PreferencesChange): boolean {
  if (!(key in newPreferences) || newPreferences[key] === undefined) {
    return false;
  }
  if (updatedPreferences[key] === newPreferences[key]) {
    return false;
  }
  // We're updating the `Preferences` original here and it doesn't type union
  // well with `PreferencesChange`. Given we've done all the type validation
  // above, cast them both to the `Preferences` type to ensure subsequent
  // updates can work.
  (updatedPreferences[key] as Preferences[keyof Preferences]) =
      newPreferences[key] as Preferences[keyof Preferences];
  return true;
}

export function updatePreferences(
    currentState: State, action: UpdatePreferencesAction): State {
  const preferences = action.payload;

  // This action takes two potential payloads:
  //  - chrome.fileManagerPrivate.Preferences
  //  - chrome.fileManagerPrivate.PreferencesChange
  // Both of these have different type requirements. If we receive a
  // `Preferences` update, just store the data directly in the store. If we
  // receive a `PreferencesChange` the individual fields need to be checked to
  // ensure they are different to what we have in the store AND they won't
  // remove the existing data (i.e. they are not null or undefined).
  if (!isPreferencesChange(preferences)) {
    return {
      ...currentState,
      preferences,
    };
  }

  const updatedPreferences = {...currentState.preferences!};
  const keysToCheck: Array<keyof PreferencesChange> = [
    'cellularDisabled',
    'arcEnabled',
    'arcRemovableMediaAccessEnabled',
    'folderShortcuts',
    'driveFsBulkPinningEnabled',
  ];
  let updated = false;
  for (const key of keysToCheck) {
    updated = updateIfDefined(updatedPreferences, preferences, key) || updated;
  }

  // If no keys have been updated in the preference change, then send back the
  // original state as nothing has changed.
  if (!updated) {
    return currentState;
  }

  return {
    ...currentState,
    preferences: updatedPreferences,
  };
}
