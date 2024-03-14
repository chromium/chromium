// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ElementObject} from '../prod/file_manager/shared_types.js';
import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, DOWNLOADS_FAKE_TASKS} from './test_data.js';

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
export async function toolbarDeleteWithMenuItemNoEntrySelected() {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Right click the list without selecting an entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['list.list']),
      'fakeMouseRightClick failed');

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu delete command is disabled.
  const deleteDisabled = '[command="#delete"][disabled="disabled"]';
  await remoteCall.waitForElement(appId, contextMenu + ' ' + deleteDisabled);
}

/**
 * Tests that the toolbar Delete button opens the delete confirm dialog and
 * that the dialog cancel button has the focus by default.
 */
export async function toolbarDeleteButtonOpensDeleteConfirmDialog() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.desktop]);

  // Select My Desktop Background.png
  await remoteCall.waitUntilSelected(appId, ENTRIES.desktop.nameText);

  // Click the toolbar Delete button.
  await remoteCall.simulateUiClick(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const defaultDialogButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', defaultDialogButton.text);
}

/**
 * Tests that the toolbar Delete button keeps focus after the delete confirm
 * dialog is closed.
 */
export async function toolbarDeleteButtonKeepFocus() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // USB delete never uses trash and always shows the delete dialog.

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Select hello.txt
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Click the toolbar Delete button.
  await remoteCall.simulateUiClick(appId, '#delete-button');

  // Check: the Delete button should lose focus.
  await remoteCall.waitForElementLost(appId, '#delete-button:focus');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const defaultDialogButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', defaultDialogButton.text);

  // Click the dialog 'Cancel' button.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel');

  // Check: the toolbar Delete button should be focused.
  await remoteCall.waitForElement(appId, '#delete-button:focus');
}

/**
 * Tests deleting an entry using the toolbar.
 */
export async function toolbarDeleteEntry() {
  const beforeDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  const afterDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful,
  ]);

  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Confirm entries in the directory before the deletion.
  await remoteCall.waitForFiles(
      appId, beforeDeletion, {ignoreLastModifiedTime: true});

  // Select My Desktop Background.png
  await remoteCall.waitUntilSelected(appId, 'My Desktop Background.png');

  // Click move to trash button in the toolbar.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#move-to-trash-button']));

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      appId, afterDeletion, {ignoreLastModifiedTime: true});
}

/**
 * Tests that refresh button hides in selection mode.
 *
 * Non-watchable volumes (other than Recent views) display the refresh
 * button so users can refresh the file list content. However this
 * button should be hidden when entering the selection mode.
 * crbug.com/978383
 */
export async function toolbarRefreshButtonWithSelection() {
  // Open files app.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('documents_provider');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');

  // Ctrl+A to enter selection mode.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
}

/**
 * Tests that refresh button is not shown when the Recent view is selected.
 */
export async function toolbarRefreshButtonHiddenInRecents() {
  // Open files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Recent.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Recent');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
}

/**
 * Tests that refresh button is shown for non-watchable volumes.
 */
export async function toolbarRefreshButtonShownForNonWatchableVolume() {
  // Open files app.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('documents_provider');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');
}

/**
 * Tests that refresh button is hidden for watchable volumes.
 */
export async function toolbarRefreshButtonHiddenForWatchableVolume() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // It should start in Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
}

/**
 * Tests that command Alt+A focus the toolbar.
 */
export async function toolbarAltACommand() {
  // Open files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Press Alt+A in the File List.
  const altA = ['#file-list', 'a', false, false, true] as const;
  await remoteCall.fakeKeyDown(appId, ...altA);

  // Check that a menu-button should be focused.
  const focusedElement =
      await remoteCall.callRemoteTestUtil<ElementObject|null>(
          'getActiveElement', appId, []);
  const cssClasses = focusedElement?.attributes['class'] || '';
  chrome.test.assertTrue(cssClasses.includes('menu-button'));
}

/**
 * Tests that the menu drop down follows the button if the button moves. This
 * happens when the search box is expanded and then collapsed.
 */
export async function toolbarMultiMenuFollowsButton() {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Override the tasks so the "Open" button becomes a dropdown button.
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select an entry in the file list.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Click the toolbar "Open" dropdown button.
  await remoteCall.simulateUiClick(appId, '#tasks');

  // Wait for the search box to collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check that the dropdown menu and "Open" button are aligned.
  const caller = getCaller();
  await repeatUntil(async () => {
    const openButton =
        await remoteCall.waitForElementStyles(appId, '#tasks', ['width']);
    const menu =
        await remoteCall.waitForElementStyles(appId, '#tasks-menu', ['width']);

    if (openButton.renderedLeft === menu.renderedLeft) {
      return;
    }

    return pending(
        caller,
        `Waiting for the menu and button to be aligned: ` +
            `${openButton.renderedLeft} !== ${menu.renderedLeft}`);
  });
}

/**
 * Tests that the sharesheet button is enabled and executable.
 */
export async function toolbarSharesheetButtonWithSelection() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets':
        ['static_promise_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_promise_fake', []],
  } as any;
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  const entry = ENTRIES.hello;

  // Select an entry in the file list.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  await remoteCall.waitAndClickElement(
      appId, '#sharesheet-button:not([hidden])');

  // Check invoke sharesheet is called.
  chrome.test.assertEq(
      1, await remoteCall.callRemoteTestUtil('staticFakeCounter', appId, [
        'chrome.fileManagerPrivate.invokeSharesheet',
      ]));

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(2, removedCount);
}

/**
 * Tests that the sharesheet command in context menu is enabled and executable.
 */
export async function toolbarSharesheetContextMenuWithSelection() {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets':
        ['static_promise_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_promise_fake', []],
  } as any;
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  const entry = ENTRIES.hello;

  // Select an entry in the file list.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  chrome.test.assertTrue(!!await remoteCall.waitAndRightClick(
      appId, '#file-list .table-row[selected]'));

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu sharesheet command is not hidden.
  const sharesheetEnabled =
      '[command="#invoke-sharesheet"]:not([hidden]):not([disabled])';

  await remoteCall.waitAndClickElement(
      appId, contextMenu + ' ' + sharesheetEnabled);

  // Check invoke sharesheet is called.
  chrome.test.assertEq(
      1, await remoteCall.callRemoteTestUtil('staticFakeCounter', appId, [
        'chrome.fileManagerPrivate.invokeSharesheet',
      ]));

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(2, removedCount);
}

/**
 * Tests that the sharesheet item is hidden if no entry is selected.
 */
export async function toolbarSharesheetNoEntrySelected() {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  const fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Right click the list without selecting an entry.
  chrome.test.assertTrue(
      !!await remoteCall.waitAndRightClick(appId, 'list.list'));

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu sharesheet command is disabled.
  const sharesheetDisabled =
      '[command="#invoke-sharesheet"][hidden][disabled="disabled"]';
  await remoteCall.waitForElement(
      appId, contextMenu + ' ' + sharesheetDisabled);

  await remoteCall.waitForElement(appId, '#sharesheet-button[hidden]');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
}

/**
 * Tests that the cloud icon does not appear if bulk pinning is disabled.
 */
export async function toolbarCloudIconShouldNotShowWhenBulkPinningDisabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
}

/**
 * Tests that the cloud icon does not appear if the bulk pinning preference is
 * disabled and the supplied Stage does not have a UI state in the progress
 * panel.
 */
export async function
toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIStateAvailable() {
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
}

/**
 * Tests that the cloud icon should only show when the bulk pinning is in
 * progress.
 */
export async function toolbarCloudIconShouldShowForInProgress() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 4 GB, the test files
  // initialized on the Drive root are 92 KB so well below the 1GB space
  // requirement.
  await remoteCall.setSpacedFreeSpace(4n << 30n);

  // Enable the bulk pinning preference and assert the cloud button is no longer
  // hidden.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');
  await remoteCall.waitForElement(
      appId, '#cloud-button > xf-icon[type="cloud_sync"]');
}

/**
 * Tests that the cloud icon should show when there is not enough disk space
 * available to pin.
 */
export async function toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space available as 100 MB, this will trigger the
  // `NotEnoughSpace` stage for bulk pinning.
  await remoteCall.setSpacedFreeSpace(100n << 20n);

  // Enable the bulk pinning preference and even though the end state is an
  // error, there is a UI state to show so the toolbar should still be visible.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');
  await remoteCall.waitForElement(
      appId, '#cloud-button > xf-icon[type="cloud_error"]');
}


/**
 * Tests that the cloud icon should not show if an error state has been
 * returned (in this case `CannotGetFreeSpace`).
 */
export async function toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 4 GB.
  await remoteCall.setSpacedFreeSpace(4n << 30n);

  // Enable the bulk pinning preference and assert the cloud button is shown.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');

  // Mock the free space available as -1 which indicates an error returned
  // during the free space retrieval.
  await remoteCall.setSpacedFreeSpace(-1n);

  // Force the bulk pinning manager to check for free space again (this
  // currently is done on a 60s poll).
  await sendTestMessage({name: 'forcePinningManagerSpaceCheck'});
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
}

/**
 * Tests that when the cloud icon is pressed the xf-cloud-panel moves into space
 * and resizes correctly.
 */
export async function toolbarCloudIconWhenPressedShouldOpenCloudPanel() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 4 GB.
  await remoteCall.setSpacedFreeSpace(4n << 30n);

  // Enable the bulk pinning preference and assert the cloud button is no longer
  // hidden.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');

  // Ensure at first the cloud panel is not shown.
  const styles = await remoteCall.waitForElementStyles(
      appId, ['xf-cloud-panel', 'cr-action-menu', 'dialog'], ['left']);
  chrome.test.assertEq(styles.renderedHeight, 0);
  chrome.test.assertEq(styles.renderedWidth, 0);
  chrome.test.assertEq(styles.renderedTop, 0);
  chrome.test.assertEq(styles.renderedLeft, 0);

  // Click the cloud icon and wait for the dialog to move into space.
  await remoteCall.waitAndClickElement(appId, '#cloud-button:not([hidden])');
  await remoteCall.waitForCloudPanelVisible(appId);
}

/**
 * Tests that the cloud icon should not show if bulk pinning is paused (which
 * represents an offline state) and the user preference is disabled.
 */
export async function toolbarCloudIconShouldNotShowWhenPrefDisabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Force the bulk pinning preference off.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});

  // Mock the free space returned by spaced to be 4 GB.
  await remoteCall.setSpacedFreeSpace(4n << 30n);

  // Set the bulk pinning manager to enter offline mode. This will surface a
  // `PAUSED` state which has a UI representation iff the pref is enabled.
  // This is done to ensure the bulk pinning doesn't finish before our
  // assertions are able to run (small amount of test files make this finish
  // super quick).
  await sendTestMessage({name: 'setBulkPinningOnline', enabled: false});

  // Force the bulk pinning to calculate required space which will kick it
  // into a `PAUSED` state from a `STOPPED` state.
  await sendTestMessage({name: 'forceBulkPinningCalculateRequiredSpace'});

  // Assert the stage is `PAUSED` and the cloud button is still hidden.
  await remoteCall.waitForBulkPinningStage('PausedOffline');
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
}

/**
 * Tests that the cloud icon should show if bulk pinning is paused (which
 * represents an offline state) and the user preference is enabled.
 */
export async function toolbarCloudIconShouldShowWhenPausedState() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Force the bulk pinning preference on.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});

  // Set the bulk pinning manager to enter offline mode. This will surface a
  // `PAUSED_OFFLINE` state which has a UI representation iff the pref is
  // enabled.
  await sendTestMessage({name: 'setBulkPinningOnline', enabled: false});

  // Assert the stage is `PAUSED_OFFLINE`, the cloud button is visible and the
  // icon is the offline icon.
  await remoteCall.waitForBulkPinningStage('PausedOffline');
  await remoteCall.waitForElement(appId, '#cloud-button:not([hidden])');
  await remoteCall.waitForElement(
      appId, '#cloud-button > xf-icon[type="bulk_pinning_offline"]');
}

/**
 * Tests that the cloud icon should show when a Files app window has started.
 * This mainly tests that on startup the bulk pin progress is fetched and
 * doesn't require an async event to show.
 */
export async function toolbarCloudIconShouldShowOnStartupEvenIfSyncing() {
  await addEntries(['drive'], [ENTRIES.hello]);

  // Mock the free space returned by spaced to be 4 GB.
  await remoteCall.setSpacedFreeSpace(4n << 30n);

  // Enable the bulk pinning preference.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});

  // Wait until the pin manager enters the syncing stage otherwise the next
  // syncing event will get ignored.
  await remoteCall.waitForBulkPinningStage('Syncing');

  // Mock the Drive pinning event completing downloading all the data.
  await sendTestMessage({
    name: 'setDrivePinSyncingEvent',
    path: `/root/${ENTRIES.hello.targetPath}`,
    bytesTransferred: 100,
    bytesToTransfer: 100,
  });

  // Wait until the bulk pinning required space reaches 0 (no bytes left to
  // pin). This indicates the bulk pinning manager has finished. When Files app
  // starts after this, no event will go via onBulkPinProgress.
  await remoteCall.waitForBulkPinningRequiredSpace(0);

  // Open a new window to the Drive root and ensure the cloud button is not
  // hidden. The cloud button will show on startup as it relies on the bulk
  // pinning preference to be set.
  const appId =
      await remoteCall.openNewWindow(RootPath.DRIVE, /*appState=*/ {});
  await remoteCall.waitForElement(appId, '#detail-table');
  await remoteCall.waitForElement(appId, '#cloud-button:not([hidden])');

  // The underlying pin manager has a 60s timer to get free disk space. When
  // this happens it emits a progress event and updates the UI. However, once
  // the cloud button is visible the `<xf-cloud-panel>` should have it's data
  // set. To bypass async issues, only wait 10s to check the data is available
  // to ensure its done prior to the 60s free disk space check.
  await remoteCall.waitForCloudPanelState(
      appId, /*items=*/ 1, /*percentage=*/ 100);
}

/**
 * Tests that the cloud icon should show if bulk pinning is paused due to being
 * on a metered network.
 */
export async function toolbarCloudIconShouldShowWhenOnMeteredNetwork() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Force the bulk pinning preference on.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});

  // Update the drive connection status to return metered and then disable the
  // sync on metered property.
  await sendTestMessage({name: 'setSyncOnMeteredNetwork', enabled: false});
  await sendTestMessage({name: 'setDriveConnectionStatus', status: 'metered'});

  // Assert the cloud icon should have the paused subicon.
  await remoteCall.waitForElement(appId, '#cloud-button:not([hidden])');
  await remoteCall.waitForElement(
      appId, '#cloud-button > xf-icon[type="cloud_paused"]');
}
