// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import {updateBulkPinProgress} from '../state/actions/bulk_pinning.js';
import {updatePreferences} from '../state/actions/preferences.js';
import {getEmptyState, getStore} from '../state/store.js';
import {XfCloudPanel} from '../widgets/xf_cloud_panel.js';

import {BulkPinProgress, BulkPinStage, CloudPanelContainer} from './cloud_panel_container.js';

/**
 * An instance of the cloud panel container.
 */
let container: CloudPanelContainer|null = null;

/**
 * An instance of the `<xf-cloud-panel>`.
 */
let panel: XfCloudPanel|null = null;

export function setUp() {
  panel = document.createElement('xf-cloud-panel');
  document.body.appendChild(panel);
  container = new CloudPanelContainer(panel, /*test_=*/ true);
}

export function tearDown() {
  // Unsubscribe from the store.
  if (container) {
    getStore().unsubscribe(container);
  }
  document.body.innerHTML = '';
}

/**
 * Tests that when bulk pinning is in a progress mode the cloud panel receives
 * that data as attributes.
 */
export async function testProgressAndItemsArePassedToElement(done: () => void) {
  // Initialize the empty store.
  const store = getStore();
  store.init(getEmptyState());

  // Setup a syncing state that should be 15% done with 24 items.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 150,
    filesToPin: 24,
  };

  // Dispatch an update to the store and wait for the panel to have the
  // attribute `items`.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  await waitUntil(() => panel!.hasAttribute('items'));

  // Assert the items and progress values are representative of the data in the
  // store.
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '15');

  done();
}

/**
 * Tests that if somehow any invalid data makes its way into the store, it
 * doesn't propagate to the element.
 */
export async function testOutOfBoundsValuesDoNotUpdateProgress(
    done: () => void) {
  // Initialize the empty store.
  const store = getStore();
  store.init(getEmptyState());

  // Setup a syncing state that should contain invalid data.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 150,
    pinnedBytes: 1000,  // Greater than `bytesToPin`.
    filesToPin: -10,    // Negative number of files to pin.
  };

  // Dispatch an update to the store and ensure the panel doesn't get
  // attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertFalse(panel!.hasAttribute('items'));
  assertFalse(panel!.hasAttribute('percentage'));

  done();
}

/**
 * Tests that if somehow any invalid data makes its way into the store, it
 * doesn't propagate to the element.
 */
export async function testOtherStoreUpdatesDontCauseThisContainerToUpdate(
    done: () => void) {
  // Initialize the empty store.
  const store = getStore();
  store.init(getEmptyState());

  // Start using a basic syncing state.
  const bulkPinning: BulkPinProgress = {
    stage: BulkPinStage.SYNCING,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 1000,
    pinnedBytes: 150,
    filesToPin: 24,
  };

  // Dispatch an update to the store and ensure the panel does get attributes.
  store.dispatch(updateBulkPinProgress(bulkPinning));
  assertEquals(
      container!.updates, 1,
      'Bulk pin state change should increment updates to 1');
  assertEquals(panel!.getAttribute('items'), '24');
  assertEquals(panel!.getAttribute('percentage'), '15');

  // Setup a basic prefs object to update the store with.
  const prefs = {
    driveEnabled: false,
    cellularDisabled: false,
    searchSuggestEnabled: false,
    use24hourClock: false,
    timezone: 'GMT+10',
    arcEnabled: false,
    arcRemovableMediaAccessEnabled: false,
    folderShortcuts: [],
    trashEnabled: false,
    officeFileMovedOneDrive: 0,
    officeFileMovedGoogleDrive: 0,
    driveFsBulkPinningEnabled: false,
  };

  // Update the preferences and ensure the `setAttribute` call hasn't happened
  // again and the attributes remain the same.
  store.dispatch(updatePreferences(prefs));
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

  done();
}

/**
 * Tests that when a click event is emitted, the correct subpage in settings is
 * opened.
 */
export async function testWhenSettingsClickEventEmittedSettingsSubpageOpened(
    done: () => void) {
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

  done();
}
