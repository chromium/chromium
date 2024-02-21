// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';
import {getLastVisitedURL} from '../common/js/util.js';
import {updateBulkPinProgress} from '../state/ducks/bulk_pinning.js';
import {getEmptyState, getStore} from '../state/store.js';

import type {XfBulkPinningDialog} from './xf_bulk_pinning_dialog.js';
import {BulkPinStage} from './xf_bulk_pinning_dialog.js';

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-bulk-pinning-dialog></xf-bulk-pinning-dialog>
  `;
}

// Gets the <xf-bulk-pinning-dialog> element.
async function getDialog(): Promise<XfBulkPinningDialog> {
  const dialog =
      document.querySelector<XfBulkPinningDialog>('xf-bulk-pinning-dialog')!;
  assertNotEquals(null, dialog);
  assertEquals('XF-BULK-PINNING-DIALOG', dialog.tagName);
  await waitForElementUpdate(dialog);
  return dialog;
}


// Gets a footer of the given dialog.
function getFooter(dialog: XfBulkPinningDialog, id: string): HTMLDivElement {
  return dialog.shadowRoot!.querySelector(`#${id}`)!;
}

// Gets a button of the given dialog.
function getButton(dialog: XfBulkPinningDialog, id: string): HTMLButtonElement {
  return dialog.shadowRoot!.querySelector(`#${id}`)!;
}

// Gets the `innerText` of the <span> element of the given dialog.
function getSpanText(dialog: XfBulkPinningDialog, id: string): string {
  return dialog.shadowRoot!.querySelector<HTMLSpanElement>(`#${id}`)!.innerText;
}

// Tests that XfBulkPinningDialog.onStateChanged() correctly reacts to app State
// events.
export async function testOnStateChange() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  const saved = chrome.fileManagerPrivate.calculateBulkPinRequiredSpace;
  try {
    let n = 0;
    chrome.fileManagerPrivate.calculateBulkPinRequiredSpace =
        (callback: () => void) => {
          n++;
          setTimeout(callback, 0);
        };


    // Show the dialog.
    await dialog.show();
    assertEquals(1, n);
  } finally {
    chrome.fileManagerPrivate.calculateBulkPinRequiredSpace = saved;
  }

  // Get relevant dialog elements.
  const offlineFooter = getFooter(dialog, 'offline-footer');
  const batteryFooter = getFooter(dialog, 'battery-saver-footer');
  const listingFooter = getFooter(dialog, 'listing-footer');
  const errorFooter = getFooter(dialog, 'error-footer');
  const spaceFooter = getFooter(dialog, 'not-enough-space-footer');
  const readyFooter = getFooter(dialog, 'ready-footer');
  const button = getButton(dialog, 'continue-button');

  // At first, listingFooter should be displayed.
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertNotEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  // Create an empty app State event.
  const state = getEmptyState();

  // This event should be ignored since it doesn't have a bulkPinning member.
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertNotEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  // Set the bulkPinning member.
  state.bulkPinning = {
    stage: BulkPinStage.GETTING_FREE_SPACE,
    freeSpaceBytes: 2000,
    requiredSpaceBytes: 1000,
  } as chrome.fileManagerPrivate.BulkPinProgress;

  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertNotEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  // Test different bulk-pinning stages.
  state.bulkPinning.stage = BulkPinStage.LISTING_FILES;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertNotEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.PAUSED_OFFLINE;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertNotEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.PAUSED_BATTERY_SAVER;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertNotEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.NOT_ENOUGH_SPACE;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertNotEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.CANNOT_GET_FREE_SPACE;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertNotEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.CANNOT_ENABLE_DOCS_OFFLINE;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertNotEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.CANNOT_LIST_FILES;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertNotEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertEquals('none', readyFooter.style.display);
  assertTrue(button.disabled);

  state.bulkPinning.stage = BulkPinStage.SUCCESS;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertNotEquals('none', readyFooter.style.display);
  assertFalse(button.disabled);
  assertEquals(
      'This will use about 1,000 bytes. You currently have 2 KB available.',
      readyFooter.innerText);

  // Change the different sizes in State.bulkPinning.
  state.bulkPinning.freeSpaceBytes = 455314615;
  state.bulkPinning.requiredSpaceBytes = 202679148;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertTrue(dialog.is_open);
  assertEquals('none', offlineFooter.style.display);
  assertEquals('none', batteryFooter.style.display);
  assertEquals('none', listingFooter.style.display);
  assertEquals('none', errorFooter.style.display);
  assertEquals('none', spaceFooter.style.display);
  assertNotEquals('none', readyFooter.style.display);
  assertFalse(button.disabled);
  assertEquals(
      'This will use about 193.3 MB. You currently have 434.2 MB available.',
      readyFooter.innerText);

  // If the pin manager sends an event with the SYNCING stage, this dialog
  // should disappear.
  state.bulkPinning.stage = BulkPinStage.SYNCING;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertFalse(dialog.is_open);
}

// Tests that XfBulkPinningDialog.onStateChanged() correctly reacts when the
// bulk-pinning feature gets activated.
export async function testOnBulkPinningEnabled() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  // Show the dialog.
  await dialog.show();
  assertTrue(dialog.is_open);

  const state = getEmptyState();
  state.preferences = {driveFsBulkPinningEnabled: true} as
      chrome.fileManagerPrivate.Preferences;

  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);
  assertFalse(dialog.is_open);
}

// Tests that clicking the "Cancel" button closes the XfBulkPinningDialog.
export async function testCancel() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  // Show the dialog.
  await dialog.show();
  assertTrue(dialog.is_open);

  // Click the "Cancel" button.
  const button = getButton(dialog, 'cancel-button');
  assertFalse(button.disabled);
  button.click();
  await waitForElementUpdate(dialog);

  // The dialog should now be closed.
  assertFalse(dialog.is_open);
}

// Tests that clicking the "Continue" button enables bulk-pinning and closes the
// XfBulkPinningDialog.
export async function testContinue() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  {
    const saved = chrome.fileManagerPrivate.calculateBulkPinRequiredSpace;
    try {
      chrome.fileManagerPrivate.calculateBulkPinRequiredSpace =
          (callback: () => void) => setTimeout(callback, 0);

      // Show the dialog.
      await dialog.show();
    } finally {
      chrome.fileManagerPrivate.calculateBulkPinRequiredSpace = saved;
    }
  }

  assertTrue(dialog.is_open);

  // Make sure the dialog is ready to continue.
  const state = getEmptyState();
  state.bulkPinning = {
    stage: BulkPinStage.SUCCESS,
    freeSpaceBytes: 455314615,
    requiredSpaceBytes: 202679148,
  } as chrome.fileManagerPrivate.BulkPinProgress;
  dialog.onStateChanged(state);
  await waitForElementUpdate(dialog);

  {
    const saved = chrome.fileManagerPrivate.setPreferences;
    try {
      let n = 0;
      chrome.fileManagerPrivate.setPreferences =
          (pref: Partial<chrome.fileManagerPrivate.PreferencesChange>) => {
            n++;
            assertTrue(pref.driveFsBulkPinningEnabled === true);
          };

      // Click the "Continue" button.
      const button = getButton(dialog, 'continue-button');
      assertFalse(button.disabled);
      button.click();
      assertEquals(1, n);
    } finally {
      chrome.fileManagerPrivate.setPreferences = saved;
    }
  }

  // The dialog should now be closed.
  await waitForElementUpdate(dialog);
  assertFalse(dialog.is_open);
}

// Tests that clicking the "Learn More" link opens the appropriate URL.
export async function testLearnMore() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  // Show the dialog.
  await dialog.show();
  assertTrue(dialog.is_open);

  // Click the "Learn More" link.
  const link =
      dialog.shadowRoot!.querySelector<HTMLAnchorElement>('#learn-more-link')!;
  assertNotEquals(null, link);
  link.click();
  assertEquals(
      getLastVisitedURL(),
      'https://support.google.com/chromebook?p=my_drive_cbx');
}


// Tests that clicking the "View Storage" link opens the appropriate settings
// page.
export async function testViewStorage() {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  // Show the dialog.
  await dialog.show();
  assertTrue(dialog.is_open);

  // Click the "View Storage" link.
  const link = dialog.shadowRoot!.querySelector<HTMLAnchorElement>(
      '#view-storage-link')!;
  assertNotEquals(null, link);
  let gotPage = '';
  chrome.fileManagerPrivate.openSettingsSubpage = (page: string) => {
    gotPage = page;
  };
  link.click();
  assertEquals('storage', gotPage);
}

// Test when listed files has a count, it appears in the footer dialog.
export async function testFileCountUpdates() {
  const store = getStore();
  store.init(getEmptyState());

  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  assertFalse(dialog.is_open);

  // Show the dialog.
  await dialog.show();
  assertTrue(dialog.is_open);

  // Dispatch the listing files state with 100 files listed.
  const bulkPinning: chrome.fileManagerPrivate.BulkPinProgress = {
    stage: BulkPinStage.LISTING_FILES,
    freeSpaceBytes: 0,
    requiredSpaceBytes: 0,
    bytesToPin: 0,
    pinnedBytes: 0,
    filesToPin: 100,
    remainingSeconds: 0,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 100,
  };
  store.dispatch(updateBulkPinProgress(bulkPinning));
  await waitForElementUpdate(dialog);
  assertEquals(
      'Checking storage space… 100 items found',
      getSpanText(dialog, 'listing-files-text'));
}
