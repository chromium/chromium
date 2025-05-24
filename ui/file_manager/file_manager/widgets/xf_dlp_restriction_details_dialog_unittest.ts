// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './xf_dlp_restriction_details_dialog.js';

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {XfDlpRestrictionDetailsDialog} from './xf_dlp_restriction_details_dialog.js';

const drive = chrome.fileManagerPrivate.VolumeType.DRIVE;
const removable = chrome.fileManagerPrivate.VolumeType.REMOVABLE;

/**
 * Creates new <xf-dlp-restriction-details-dialog> element for each test.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-dlp-restriction-details-dialog></xf-dlp-restriction-details-dialog>
  `;
}

/** Returns the <xf-dlp-restriction-details-dialog> element. */
function getDialog(): XfDlpRestrictionDetailsDialog {
  const dialog = document.querySelector('xf-dlp-restriction-details-dialog')!;
  assertNotEquals(dialog, null);
  assertNotEquals('none', window.getComputedStyle(dialog).display);
  assertFalse(dialog.hasAttribute('hidden'));
  return dialog;
}

/** Returns the block details span element of the dialog. */
function getBlockDetails(): HTMLSpanElement {
  const details =
      getDialog().shadowRoot!.querySelector<HTMLSpanElement>('#block-details')!;
  assertNotEquals(details, null);
  return details;
}

/** Returns the warn details span element of the dialog. */
function getWarnDetails(): HTMLSpanElement {
  const details =
      getDialog().shadowRoot!.querySelector<HTMLSpanElement>('#warn-details')!;
  assertNotEquals(details, null);
  return details;
}

/** Returns the report details span element of the dialog. */
function getReportDetails(): HTMLSpanElement {
  const details = getDialog().shadowRoot!.querySelector<HTMLSpanElement>(
      '#report-details')!;
  assertNotEquals(details, null);
  return details;
}

/**
 * Checks that the surronding list element for `level` is not hidden and returns
 * the `destinations` label of the dialog.
 * @level DLP level used to get the right elements. One of 'block', 'warn',
 * 'report'.
 * @destinations One of 'urls' or 'components'.
 */
function getDestinationsLabelForLevel(
    level: string, destinations: string): HTMLLabelElement {
  assertFalse(
      getDialog().shadowRoot!.querySelector(`#${level}-li-${
          destinations}`)!.hasAttribute('hidden'));
  return getDialog().shadowRoot!.querySelector<HTMLLabelElement>(
      `#${level}-${destinations}`)!;
}

/**
 * Checks that the "block-li-urls" element is not hidden and returns the
 * corresponding "block-urls" label of the dialog.
 */
function getBlockUrls() {
  return getDestinationsLabelForLevel('block', 'urls');
}

/**
 * Checks that the "warn-li-urls" element is not hidden and returns the
 * corresponding "warn-urls" label of the dialog.
 */
function getWarnUrls() {
  return getDestinationsLabelForLevel('warn', 'urls');
}

/**
 * Checks that the "report-li-urls" element is not hidden and returns the
 * corresponding "report-urls" label of the dialog.
 */
function getReportUrls() {
  return getDestinationsLabelForLevel('report', 'urls');
}

/**
 * Checks that the "block-li-components" element is not hidden and returns the
 * corresponding "block-components" label of the dialog.
 */
function getBlockComponents() {
  return getDestinationsLabelForLevel('block', 'components');
}

/**
 * Checks that the "warn-li-components" element is not hidden and returns the
 * corresponding "warn-components" label of the dialog.
 */
function getWarnComponents() {
  return getDestinationsLabelForLevel('warn', 'components');
}

/**
 * Checks that the "report-li-components" element is not hidden and returns the
 * corresponding "report-components" label of the dialog.
 */
function getReportComponents() {
  return getDestinationsLabelForLevel('report', 'components');
}

/** Closes the dialog. */
function close() {
  const crDialog =
      getDialog().shadowRoot!.querySelector<CrDialogElement>('#dialog')!;
  crDialog.close();
}

/**
 * Tests that only the URL section for the block section is shown.
 */
export async function testBlockUrls(done: () => void) {
  const dialog = getDialog();
  const blockDetails = getBlockDetails();
  assertTrue(blockDetails.hasAttribute('hidden'));

  const details: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [{
    level: chrome.fileManagerPrivate.DlpLevel.BLOCK,
    urls: ['https://external.com', 'https://example.com'],
    components: [],
  }];
  dialog.showDlpRestrictionDetailsDialog(details);
  assertFalse(blockDetails.hasAttribute('hidden'));
  assertFalse(
      blockDetails.querySelector('#block-li-urls')!.hasAttribute('hidden'));
  assertEquals(
      blockDetails.querySelector('#block-urls')!.textContent,
      'File access by https://external.com, https://example.com');
  // Components should still be hidden.
  assertTrue(blockDetails.querySelector('#block-li-components')!.hasAttribute(
      'hidden'));

  // Other restriction levels should still be hidden.
  assertTrue(getWarnDetails().hasAttribute('hidden'));
  assertTrue(getReportDetails().hasAttribute('hidden'));

  done();
}

/**
 * Tests that wildcard is shown as "all urls", any other specific URLs
 * listed along the wildcard are ignored, and components are not affected by the
 * wildcard.
 */
export async function testBlockAllUrls(done: () => void) {
  const dialog = getDialog();
  const blockDetails = getBlockDetails();
  assertTrue(blockDetails.hasAttribute('hidden'));

  const details: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [{
    level: chrome.fileManagerPrivate.DlpLevel.BLOCK,
    urls: ['https://external.com', '*'],
    components: [drive],
  }];
  dialog.showDlpRestrictionDetailsDialog(details);
  assertFalse(blockDetails.hasAttribute('hidden'));
  assertEquals(
      getBlockUrls().textContent, 'File access by all websites and URLs');
  assertEquals(
      getBlockComponents().textContent, 'File transfer to Google Drive');

  // Other restriction levels should still be hidden.
  assertTrue(getWarnDetails().hasAttribute('hidden'));
  assertTrue(getReportDetails().hasAttribute('hidden'));

  done();
}

/**
 * Tests that wildcard is shown as "all urls except..." if there are some URLs
 * with a higher restriction level.
 */
export async function testBlockAllUrlsExcept(done: () => void) {
  const dialog = getDialog();
  const blockDetails = getBlockDetails();
  assertTrue(blockDetails.hasAttribute('hidden'));

  const details: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [
    {
      level: chrome.fileManagerPrivate.DlpLevel.BLOCK,
      urls: ['*'],
      components: [],
    },
    {
      level: chrome.fileManagerPrivate.DlpLevel.ALLOW,
      urls: ['https://internal.com'],
      components: [],
    },
  ];
  dialog.showDlpRestrictionDetailsDialog(details);
  assertFalse(blockDetails.hasAttribute('hidden'));
  assertEquals(
      getBlockUrls().textContent,
      'File access by all websites and URLs except https://internal.com');
  // Components should still be hidden.
  assertTrue(blockDetails.querySelector('#block-li-components')!.hasAttribute(
      'hidden'));

  // Other restriction levels should still be hidden.
  assertTrue(getWarnDetails().hasAttribute('hidden'));
  assertTrue(getReportDetails().hasAttribute('hidden'));

  done();
}

/**
 * Tests that only the components section for the block section is shown.
 */
export async function testBlockComponents(done: () => void) {
  const dialog = getDialog();
  const blockDetails = getBlockDetails();
  assertTrue(blockDetails.hasAttribute('hidden'));

  const details: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [{
    level: chrome.fileManagerPrivate.DlpLevel.BLOCK,
    urls: [],
    components: [drive, removable],
  }];
  dialog.showDlpRestrictionDetailsDialog(details);
  assertFalse(blockDetails.hasAttribute('hidden'));
  // Urls should still be hidden.
  assertTrue(
      blockDetails.querySelector('#block-li-urls')!.hasAttribute('hidden'));
  assertEquals(
      getBlockComponents().textContent,
      'File transfer to Google Drive, removable storage');

  // Other restriction levels should still be hidden.
  assertTrue(getWarnDetails().hasAttribute('hidden'));
  assertTrue(getReportDetails().hasAttribute('hidden'));

  done();
}

/**
 * Tests that showing the dialog multiple times in a row still shows correct
 * data.
 */
export async function testMultipleDialogs(done: () => void) {
  const dialog = getDialog();
  const blockDetails = getBlockDetails();
  const warnDetails = getWarnDetails();
  const reportDetails = getReportDetails();
  assertTrue(blockDetails.hasAttribute('hidden'));


  const details1: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [{
    level: chrome.fileManagerPrivate.DlpLevel.BLOCK,
    urls: ['https://external.com'],
    components: [drive],
  }];
  dialog.showDlpRestrictionDetailsDialog(details1);
  assertFalse(blockDetails.hasAttribute('hidden'));
  assertEquals(
      getBlockUrls().textContent, 'File access by https://external.com');
  assertEquals(
      getBlockComponents().textContent, 'File transfer to Google Drive');

  // Other restriction levels should still be hidden.
  assertTrue(warnDetails.hasAttribute('hidden'));
  assertTrue(reportDetails.hasAttribute('hidden'));

  close();

  const details2: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [
    {
      level: chrome.fileManagerPrivate.DlpLevel.WARN,
      urls: ['https://example.com'],
      components: [drive, removable],
    },
    {
      level: chrome.fileManagerPrivate.DlpLevel.REPORT,
      urls: ['https://external.com'],
      components: [],
    },
  ];
  dialog.showDlpRestrictionDetailsDialog(details2);
  assertFalse(warnDetails.hasAttribute('hidden'));
  assertEquals(getWarnUrls().textContent, 'File access by https://example.com');
  assertEquals(
      getWarnComponents().textContent,
      'File transfer to Google Drive, removable storage');
  assertFalse(reportDetails.hasAttribute('hidden'));
  assertEquals(
      getReportUrls().textContent, 'File access by https://external.com');
  assertTrue(reportDetails.querySelector('#report-li-components')!.hasAttribute(
      'hidden'));

  // Block section should now be hidden.
  assertTrue(blockDetails.hasAttribute('hidden'));

  close();

  const details3: chrome.fileManagerPrivate.DlpRestrictionDetails[] = [
    {
      level: chrome.fileManagerPrivate.DlpLevel.REPORT,
      urls: [],
      components: [drive, removable],
    },
  ];
  dialog.showDlpRestrictionDetailsDialog(details3);
  // Report urls should now be hidden.
  assertFalse(reportDetails.hasAttribute('hidden'));
  assertTrue(
      reportDetails.querySelector('#report-li-urls')!.hasAttribute('hidden'));
  assertEquals(
      getReportComponents().textContent,
      'File transfer to Google Drive, removable storage');

  // Block and warn sections should now be hidden.
  assertTrue(blockDetails.hasAttribute('hidden'));
  assertTrue(warnDetails.hasAttribute('hidden'));

  done();
}
