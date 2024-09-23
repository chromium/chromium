// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ElementObject} from '../prod/file_manager/shared_types.js';
import {ENTRIES, getCaller, pending, repeatUntil, RootPath, wait} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

const tooltipQuery = 'files-tooltip';
const tooltipQueryHidden = 'files-tooltip:not([visible])';
const tooltipQueryVisible = 'files-tooltip[visible=true]';
const searchButton = '#search-button[has-tooltip]';
const viewButton = '#view-button[has-tooltip]';
const readonlyIndicator =
    '#read-only-indicator[has-tooltip][show-card-tooltip]';
const fileList = '#file-list';
const cancelButton = '#cancel-selection-button[has-tooltip]';
const deleteButton = '#delete-button[has-tooltip]';

const tooltipShowTimeout = 500;  // ms

/**
 * Waits until the element by |id| is the document.activeElement.
 *
 * @param appId The Files app windowId.
 * @param id The element id.
 */
function getActiveElementById(appId: string, id: string): Promise<void> {
  const caller = getCaller();
  return repeatUntil(async () => {
    const element = await remoteCall.callRemoteTestUtil<ElementObject|null>(
        'getActiveElement', appId, []);
    if (!element || element.attributes['id'] !== id) {
      return pending(caller, 'Waiting for active element by id #%s.', id);
    }
    return undefined;
  });
}

/**
 * Tests that tooltip is displayed when focusing an element with tooltip.
 */
export async function filesTooltipFocus() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Focus a button with a tooltip: the search button.
  await remoteCall.focus(appId, [searchButton]);
  await getActiveElementById(appId, 'search-button');

  // Check: the search button tooltip should be visible.
  let label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', label.text);

  // Focus an element that has no tooltip: the file-list.
  await remoteCall.focus(appId, [fileList]);
  await getActiveElementById(appId, 'file-list');

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select all the files to enable the cancel selection button.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Focus the cancel selection button.
  await remoteCall.focus(appId, [cancelButton]);
  await getActiveElementById(appId, 'cancel-selection-button');

  // Check: the cancel selection button tooltip should be visible.
  label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Cancel selection', label.text);
}

/**
 * Tests that tooltip is displayed when focusing an element with tooltip.
 */
export async function filesTooltipLabelChange() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Focus a button with a tooltip: the view button.
  await remoteCall.focus(appId, [viewButton]);
  await getActiveElementById(appId, 'view-button');

  // Check: the view button tooltip should be visible.
  let label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Switch to thumbnail view', label.text);

  // Click the view button to update its label.
  await remoteCall.waitAndClickElement(appId, [viewButton]);

  // Check: the view button should still be focused.
  await getActiveElementById(appId, 'view-button');

  // Check: the tooltip text should be updated.
  label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Switch to list view', label.text);
}

/**
 * Tests that tooltips display when hovering an element that has a tooltip.
 */
export async function filesTooltipMouseOver() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Mouse hover over a button that has a tooltip: the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const firstElement =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', firstElement.text);

  // Move the mouse away from the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOut', appId, [searchButton]));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Move the mouse over the search button again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const lastElement =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', lastElement.text);
}

/**
 * Tests that tooltips stay open when hovering over the tooltip.
 */
export async function filesTooltipMouseOverStaysOpen() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Mouse hover over a button that has a tooltip: the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const firstElement =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', firstElement.text);

  // Move the mouse away from the search button, but on the tooltip.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOut', appId, [searchButton]));
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [tooltipQuery]));

  // Check: the tooltip should still be visible.
  await remoteCall.waitForElement(appId, tooltipQueryVisible);

  // Move the mouse away from the tooltip.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOut', appId, [tooltipQuery]));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
}

/**
 * Tests that tooltip is hidden when clicking on body (or anything else).
 */
export async function filesTooltipClickHides() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Hover over a button that has a tooltip: the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', label.text);

  // Click the body element.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
}

/**
 * Tests that card tooltip is hidden when clicking on body (or anything else).
 */
export async function filesCardTooltipClickHides() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Click the 'Android files' volume tab in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('android_files');

  // Wait for the read-only bubble to appear in the files app tool bar.
  const readonlyBubbleShown = '#read-only-indicator:not([hidden])';
  await remoteCall.waitForElement(appId, readonlyBubbleShown);

  // Check: initially, no tooltip should be visible.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Hover the mouse over the read-only bubble.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [readonlyIndicator]));

  // Check: the read-only bubble card tooltip should be visible.
  const expectedLabelText = 'The contents of this folder are read-only. ' +
      'Some activities are not supported.';
  const tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);
  chrome.test.assertEq('card-tooltip', tooltip.attributes['class']);
  chrome.test.assertEq('card-label', label.attributes['class']);

  // Click the body element.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
}

/**
 * Tests that the tooltip should hide when the window resizes.
 */
export async function filesTooltipHidesOnWindowResize() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Focus a button with tooltip: the search button.
  await remoteCall.focus(appId, [searchButton]);
  await getActiveElementById(appId, 'search-button');

  // Check: the search button tooltip should be visible.
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq('Search', label.text);

  // Resize the window.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('resizeWindow', appId, [1200, 1200]));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
}

/**
 * Tests that the tooltip is hidden after the delete confirm dialog closes.
 */
export async function filesTooltipHidesOnDeleteDialogClosed() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.beautiful, ENTRIES.photos]);

  const fileListItemQuery = '#file-list li[file-name="Beautiful Song.ogg"]';

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select file.
  await remoteCall.waitAndClickElement(appId, fileListItemQuery);

  // Mouse over the delete button and leave time for tooltip to show.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [deleteButton]));
  await wait(tooltipShowTimeout);

  // Click the toolbar delete button.
  await remoteCall.waitAndClickElement(appId, deleteButton);

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Cancel' button.
  const dialogCancelButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogCancelButton.text);

  // Leave time for tooltip to show.
  await wait(tooltipShowTimeout);

  // Check: the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select file.
  await remoteCall.waitAndClickElement(appId, fileListItemQuery);

  // Mouse over the delete button and leave time for tooltip to show.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [deleteButton]));
  await wait(tooltipShowTimeout);

  // Click the toolbar delete button.
  await remoteCall.waitAndClickElement(appId, deleteButton);

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Delete' button.
  const dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Check: the delete confirm dialog should close.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');

  // Leave time for tooltip to show.
  await wait(tooltipShowTimeout);

  // Check: the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
}
