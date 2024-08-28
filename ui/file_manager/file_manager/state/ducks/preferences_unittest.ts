// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {setupStore} from '../for_tests.js';

import {type Preferences, updatePreferences} from './preferences.js';

/**
 * Defines an initial state for user preferences that is used in all the tests.
 */
const INITIAL_PREFERENCES: Preferences = {
  driveEnabled: false,
  driveSyncEnabledOnMeteredNetwork: false,
  searchSuggestEnabled: false,
  use24hourClock: false,
  timezone: 'GMT+10',
  arcEnabled: false,
  arcRemovableMediaAccessEnabled: false,
  folderShortcuts: [],
  trashEnabled: false,
  officeFileMovedOneDrive: 0,
  officeFileMovedGoogleDrive: 0,
  driveFsBulkPinningAvailable: false,
  driveFsBulkPinningEnabled: false,
  localUserFilesAllowed: true,
  defaultLocation: chrome.fileManagerPrivate.DefaultLocation.MY_FILES,
  skyVaultMigrationDestination:
      chrome.fileManagerPrivate.CloudProvider.NOT_SPECIFIED,
};

/**
 * Tests that bulk pin progress updates the store and overwrites existing values
 * on each update.
 */
export async function testUpdatePreferences(done: () => void) {
  // Dispatch an action to update user preferences.
  const store = setupStore();
  store.dispatch(updatePreferences(INITIAL_PREFERENCES));

  // Expect the preferences in the store to be updated.
  const firstState = store.getState().preferences;
  const want = INITIAL_PREFERENCES;
  assertDeepEquals(
      want, firstState,
      `${JSON.stringify(want)} !== ${JSON.stringify(firstState)}`);

  done();
}

export async function testPreferencesWithNoKeysUpdates(done: () => void) {
  // Dispatch an action to update bulk pin progress.
  const store = setupStore();
  store.dispatch(updatePreferences(INITIAL_PREFERENCES));

  /**
   * Test an individual preference update type.
   * NOTE: Closure types defined on the fileManagerPrivate.PreferencesChange
   * require that if 1 preference is being updated all others must have a key of
   * undefined, however, this doesn't mimic the real world scenario of the keys
   * not existing. Use `any` to ensure this can be passed through.
   */
  const testPreferenceUpdate =
      (preferenceUpdate: any, initialPreferences: Preferences): Preferences => {
        store.dispatch(updatePreferences(preferenceUpdate));

        const want = {
          ...initialPreferences,
          ...preferenceUpdate,
        };

        // Expect the preferences in the store to be updated.
        const state = store.getState().preferences;
        assertDeepEquals(
            want, state,
            `${JSON.stringify(want)} !== ${JSON.stringify(state)}`);

        return want;
      };

  // Verify all the `boolean` type preferences update appropriately, they are
  // all initially `false` and this updates them all to `true` one by one.
  let preferences = INITIAL_PREFERENCES;
  const booleanPreferences = [
    'driveSyncEnabledOnMeteredNetwork',
    'arcEnabled',
    'arcRemovableMediaAccessEnabled',
    'driveFsBulkPinningEnabled',
  ];
  for (const pref of booleanPreferences) {
    preferences = testPreferenceUpdate({[pref]: true}, preferences);
  }

  // Folder shortcuts are an array, so test them separately.
  const folderShortcutsUpdate = {folderShortcuts: ['some/shortcut']};
  testPreferenceUpdate(folderShortcutsUpdate, preferences);

  done();
}
