// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootPath} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, SHARED_DRIVE_ENTRY_SET} from './test_data.js';

/**
 * Test 'Manage in Drive' for a file or directory on Drive.
 *
 * @param path Path of the file or directory to be managed.
 * @param url Expected URL for the browser to visit.
 * @param teamDrive If set, the team drive to switch to.
 */
async function manageWithDriveExpectBrowserURL(
    path: string, url: string, teamDrive: string|undefined = undefined) {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat(SHARED_DRIVE_ENTRY_SET));

  // Navigate to the specified team drive if one is specified.
  if (teamDrive !== undefined) {
    const directoryTree = await DirectoryTreePageObject.create(appId);
    await directoryTree.navigateToPath(
        teamDrive === '' ? '/Shared drives' : `/Shared drives/${teamDrive}`);

    // Wait for the file list to update.
    await remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length);
  }

  // Select the given |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the entry to be selected.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '.table-row[selected]'));

  // Right-click the selected entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseClick failed');

  // Wait for the context menu to appear.
  chrome.test.assertTrue(!!await remoteCall.waitForElement(
      appId, '#file-context-menu:not([hidden])'));

  // Click the "Manage in Drive" menu item.
  await remoteCall.waitAndClickElement(
      appId, '[command="#manage-in-drive"]:not([hidden]):not([disabled])');

  // Wait for the context menu to disappear.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '#file-context-menu[hidden]'));

  // Wait for the browser window to appear and navigate to the expected URL.
  chrome.test.assertEq(
      url, await remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, []));
}

/**
 * Tests managing a file on Drive.
 */
export function manageFileDrive() {
  const URL = 'https://file_alternate_link/world.ogv';
  return manageWithDriveExpectBrowserURL('world.ogv', URL);
}

/**
 * Tests managing a directory on Drive.
 */
export function manageDirectoryDrive() {
  const URL = 'https://folder_alternate_link/photos';
  return manageWithDriveExpectBrowserURL('photos', URL);
}

/**
 * Tests managing a hosted file (gdoc) on Drive.
 */
export function manageHostedFileDrive() {
  const URL = 'https://document_alternate_link/Test%20Document';
  return manageWithDriveExpectBrowserURL('Test Document.gdoc', URL);
}

/**
 * Tests managing a file in a team drive.
 */
export function manageFileTeamDrive() {
  const URL = 'https://file_alternate_link/teamDriveAFile.txt';
  return manageWithDriveExpectBrowserURL(
      'teamDriveAFile.txt', URL, 'Team Drive A');
}

/**
 * Tests managing a directory in a team drive.
 */
export function manageDirectoryTeamDrive() {
  const URL = 'https://folder_alternate_link/teamDriveADirectory';
  return manageWithDriveExpectBrowserURL(
      'teamDriveADirectory', URL, 'Team Drive A');
}

/**
 * Tests managing a hosted file (gdoc) in a team drive.
 */
export function manageHostedFileTeamDrive() {
  const URL = 'https://document_alternate_link/teamDriveAHostedDoc';
  return manageWithDriveExpectBrowserURL(
      'teamDriveAHostedDoc.gdoc', URL, 'Team Drive A');
}

/**
 * Tests managing a team drive.
 */
export function manageTeamDrive() {
  const URL = 'https://folder_alternate_link/Team%20Drive%20A';
  return manageWithDriveExpectBrowserURL('Team Drive A', URL, '');
}
