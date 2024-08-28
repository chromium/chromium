// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import {updateBulkPinProgress} from '../state/ducks/bulk_pinning.js';
import {updateDriveConnectionStatus} from '../state/ducks/drive.js';
import {updatePreferences} from '../state/ducks/preferences.js';
import {waitDeepEquals} from '../state/for_tests.js';
import {getEmptyState, getStore} from '../state/store.js';
import {XfCloudPanel} from '../widgets/xf_cloud_panel.js';

import {type BulkPinProgress, BulkPinStage, CloudPanelContainer} from './cloud_panel_container.js';

/**
 * An instance of the cloud panel container.
 */
let container: CloudPanelContainer|null = null;

/**
 * An instance of the `<xf-cloud-panel>`.
 */
let panel: XfCloudPanel|null = null;

/**
 * A preferences object to initialize the store with that ensures bulk pinning
 * preference is enabled.
 */
const PREFERENCES = {
  driveEnabled: false,
  driveSyncEnabledOnMeteredNetwork: true,
  searchSuggestEnabled: false,
  use24hourClock: false,
  timezone: 'GMT+10',
  arcEnabled: false,
  arcRemovableMediaAccessEnabled: false,
  folderShortcuts: [],
  trashEnabled: false,
  officeFileMovedOneDrive: 0,
  officeFileMovedGoogleDrive: 0,
  driveFsBulkPinningAvailable: true,
  driveFsBulkPinningEnabled: true,
  localUserFilesAllowed: true,
  defaultLocation: chrome.fileManagerPrivate.DefaultLocation.MY_FILES,
  skyVaultMigrationDestination:
      chrome.fileManagerPrivate.CloudProvider.NOT_SPECIFIED,
};

export function setUp() {
  loadTimeData.overrideValues({'DRIVE_FS_BULK_PINNING': true});
  panel = document.createElement('xf-cloud-panel');
  document.body.appendChild(panel);
  container = new CloudPanelContainer(panel, /*test_=*/ true);
}

export function tearDown() {
  // Unsubscribe from the store.
  if (container) {
    getStore().unsubscribe(container);
  }
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
}

/**
 * Tests that when bulk pinning is in a progress mode the cloud panel receives
 * that data as attributes.
 */
export async function testProgressAndItemsArePassedToElement() {
  // Initialize the store with bulk pinning pref enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 15% done with 24 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 150,
    filesToPin: 24,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 24,
  };

  // Dispatch an update to the store and wait for the panel to have the
  // attribute `items`.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  await waitUntil(() => panel!.hasAttribute('items'));

  // Assert the items and progress values are representative of the data in the
  // store.
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '15');
}

/**
 * Tests that if somehow any invalid data makes its way into the store, it
 * doesn't propagate to the element.
 */
export async function testOutOfBoundsValuesDoNotUpdateProgress() {
  // Initialize the store with bulk pinning pref enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should contain invalid data.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 150,
    pinnedBytes: 1000,  // Greater than `bytesToPin`.
    filesToPin: -10,    // Negative number of files to pin.
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 24,
  };

  // Dispatch an update to the store and ensure the panel doesn't get
  // attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));
}

/**
 * Tests that updates to the store unrelated to bulk pinning OR any duplicate
 * updates do not get passed onto the underlying element.
 */
export async function testOtherStoreUpdatesDontCauseThisContainerToUpdate() {
  // Initialize the store with bulk pinning pref enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Start using a basic syncing state.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 150,
    filesToPin: 24,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 24,
  };

  // Dispatch an update to the store and ensure the panel does get attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '15');

  // Update the preferences with new values and ensure the `setAttribute` call
  // hasn't happened again and the attributes remain the same.
  store.dispatch(updatePreferences({...PREFERENCES, arcEnabled: true}));
  assertEquals(
      container!.updates, 1,
      'Preferences state change should not increment cloud panel updates');
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '15');

  // Update the value for `pinnedBytes` and dispatch to the store then assert
  // more invocations have occurred and the attributes have updated.
  store.dispatch(updateBulkPinProgress({...bulkPinning, pinnedBytes: 300}));
  assertEquals(
      container!.updates, 2,
      'Bulk pin state change should increment updates to 2');
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '30');

  // Update the bulk pin progress with the exact same data as before, this
  // should not cause another update and the attributes should not be changed.
  store.dispatch(updateBulkPinProgress({...bulkPinning, pinnedBytes: 300}));
  assertEquals(container!.updates, 2, 'Bulk pin state should not be changed');
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '30');
}

/**
 * Tests that when there are no bytes to be pinned, the percente should be
 * updated to be 100% as any new user who logs in with no new changes will have
 * no bytes to pin on initialization.
 */
export async function testZeroBytesToPinShouldShowAllFilesSynced() {
  // Initialize the store with bulk pinning pref enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 0% done with 0 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 0,
    pinnedBytes: 0,
    filesToPin: 0,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 0,
  };

  // Dispatch an update to the store and wait for the panel to have the
  // attribute `items`.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  await waitUntil(() => panel!.hasAttribute('items'));

  // Assert the items and progress values are representative of the data in the
  // store.
  assertEquals(panel!.getAttribute('items'), '0');
  assertEquals(panel!.getAttribute('percentage'), '100');
}

/**
 * Tests that when a click event is emitted, the correct subpage in settings is
 * opened.
 */
export async function testWhenSettingsClickEventEmittedSettingsSubpageOpened() {
  // Mock the fileManagerPrivate API.
  let pageRequested: string|null = null;
  chrome.fileManagerPrivate.openSettingsSubpage = (page: string) => {
    pageRequested = page;
  };

  // Dispatch an event from the cloud panel indicating the drive settings was
  // clicked.
  panel!.dispatchEvent(
      new CustomEvent(XfCloudPanel.events.DRIVE_SETTINGS_CLICKED, {
        bubbles: true,
        composed: true,
      }));

  // Wait until the page has called the private API and assert it's the correct
  // page.
  await waitUntil(() => pageRequested !== null);
  assertEquals(pageRequested, 'googleDrive');
}

/**
 * Tests that the element doesn't receive updates when the preference is
 * disabled, after enabling the preference updates should propagate through.
 */
export async function
testInProgressStateDoesNotUpdateThePanelWhenPrefDisabled() {
  // Initialize the store with bulk pinning disabled.
  const store = getStore();
  store.init({
    ...getEmptyState(),
    preferences: {...PREFERENCES, driveFsBulkPinningEnabled: false},
  });

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 100,
    filesToPin: 10,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 10,
  };

  // Dispatch an update to the store, wait for the store to update before
  // asserting that the panel doesn't get attributes due to the pref not being
  // enabled.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  await waitDeepEquals(
      store, BulkPinStage.SYNCING,
      (state) => state.bulkPinning && state.bulkPinning!.stage);
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));
  assertEquals(container!.updates, 0);

  // Enable the bulk pinning preference and wait for it to update the cloud
  // panel attributes.
  store.dispatch(updatePreferences({...PREFERENCES}));
  await waitUntil(() => container!.updates === 1);
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '10');
}

/**
 * Tests that updating the syncing stage to offline adds the type attribute and
 * going back to syncing (i.e. back online) removes the type attribute.
 */
export async function
testPausedStateAddsTypeAttributeAndSyncingRemovesAttribute() {
  // Initialize the store with bulk pinning enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 100,
    filesToPin: 10,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 10,
  };

  // Dispatch an update to the store and ensure the panel does get attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '10');

  // Pausing the bulk pinning operation does not update the attributes except
  // changing the type attribute to offline.
  store.dispatch(updateBulkPinProgress(
      {...bulkPinning, pinnedBytes: 200, stage: BulkPinStage.PAUSED_OFFLINE}));
  assertEquals(
      container!.updates, 2,
      'Bulk pin state stage should increment updates to 2');
  assertEquals(panel!.getAttribute('type'), 'offline');
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));
  store.dispatch(updateBulkPinProgress({
    ...bulkPinning,
    pinnedBytes: 200,
    stage: BulkPinStage.PAUSED_BATTERY_SAVER,
  }));
  assertEquals(
      container!.updates, 3,
      'Bulk pin state stage should increment updates to 3');
  assertEquals(panel!.getAttribute('type'), 'battery_saver');
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));

  // Switching back into `SYNCING` with new pinned bytes removes the type
  // attribute and updates the attributes.
  store.dispatch(updateBulkPinProgress({...bulkPinning, pinnedBytes: 300}));
  assertEquals(
      container!.updates, 4,
      'Bulk pin state stage should increment updates to 4');
  assertFalse(panel!.hasAttribute('type'));
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '30');
}

/**
 * Tests that updating the syncing stage to not enough space adds the type
 * attribute and goes back to syncing (i.e. user has started it again) removes
 * the type attribute.
 */
export async function
testNotEnoughSpaceStateAddsTypeAttributeAndSyncingRemovesAttribute() {
  // Initialize the store with bulk pinning enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 100,
    filesToPin: 10,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 10,
  };

  // Dispatch an update to the store and ensure the panel does get attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '10');

  // Entering into a not enough space state ensures the type is updated and the
  // items and percentage attributes are removed.
  store.dispatch(updateBulkPinProgress({
    ...bulkPinning,
    pinnedBytes: 200,
    stage: BulkPinStage.NOT_ENOUGH_SPACE,
  }));
  assertEquals(
      container!.updates, 2,
      'Bulk pin state stage should increment updates to 2');
  assertEquals(panel!.getAttribute('type'), 'not_enough_space');
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));

  // Switching back into `SYNCING` with new pinned bytes removes the type
  // attribute and updates the attributes.
  store.dispatch(updateBulkPinProgress({...bulkPinning, pinnedBytes: 300}));
  assertEquals(
      container!.updates, 3,
      'Bulk pin state stage should increment updates to 3');
  assertFalse(panel!.hasAttribute('type'));
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '30');
}

/**
 * Test that any existing properties are removed when moving to the listing
 * files stage.
 */
export async function testExistingPropertiesAreRemovedOnSubsequentSyncds() {
  // Initialize the store with bulk pinning enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 100,
    filesToPin: 10,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 10,
  };

  // Dispatch an update to the store and ensure the panel does get attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '10');
  assertEquals(panel!.getAttribute('percentage'), '10');

  // Dispatch an update to the store to move back to the listing files stage,
  // this should clear the percentage attribute.
  store.dispatch(updateBulkPinProgress({
    ...bulkPinning,
    stage: BulkPinStage.LISTING_FILES,
    pinnedBytes: 0,
  }));
  assertEquals(
      container!.updates, 2,
      'Bulk pin state change should increment updates to 2');
  assertEquals(panel!.getAttribute('items'), '10');
  assertFalse(panel!.hasAttribute('percentage'));
}

/**
 * Tests that if the user has any files to pin but no bytes (i.e. has ONLY
 * 0-byte files) the percentage is also attached (a pre-requisite to show the
 * File sync is on page).
 */
export async function testNoBytesToPinButHasFilesAddsPercentage() {
  // Initialize the store with bulk pinning enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 0,
    pinnedBytes: 0,
    filesToPin: 1,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 1,
  };

  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '1');
  assertEquals(panel!.getAttribute('seconds'), '0');
  assertEquals(panel!.getAttribute('percentage'), '100');
}

/**
 * Tests that a metered network update to the store passes the state down to the
 * cloud panel.
 */
export async function testMeteredNetworkState() {
  // Initialize the store with bulk pinning enabled.
  const store = getStore();
  store.init({...getEmptyState(), preferences: PREFERENCES});

  // Setup a syncing state that should be 10% done with 10 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 0,
    pinnedBytes: 0,
    filesToPin: 1,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 1,
  };

  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '1');
  assertEquals(panel!.getAttribute('seconds'), '0');
  assertEquals(panel!.getAttribute('percentage'), '100');

  // Entering into a not enough space state ensures the type is updated and the
  // items and percentage attributes are removed.
  store.dispatch(updateDriveConnectionStatus({
    type: chrome.fileManagerPrivate.DriveConnectionStateType.METERED,
  }));
  assertEquals(
      container!.updates, 2,
      'Bulk pin state stage should increment updates to 2');
  assertEquals(panel!.getAttribute('type'), 'metered_network');
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));
}
