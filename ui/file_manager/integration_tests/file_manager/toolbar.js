// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {DOWNLOADS_FAKE_TASKS} from './tasks.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.toolbarDeleteWithMenuItemNoEntrySelected = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
};

/**
 * Tests that the toolbar Delete button opens the delete confirm dialog and
 * that the dialog cancel button has the focus by default.
 */
testcase.toolbarDeleteButtonOpensDeleteConfirmDialog = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.desktop]);

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
};

/**
 * Tests that the toolbar Delete button keeps focus after the delete confirm
 * dialog is closed.
 */
testcase.toolbarDeleteButtonKeepFocus = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // USB delete never uses trash and always shows the delete dialog.
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY]),
      'fakeMouseClick failed');

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
};

/**
 * Tests deleting an entry using the toolbar.
 */
testcase.toolbarDeleteEntry = async () => {
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
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Confirm entries in the directory before the deletion.
  await remoteCall.waitForFiles(
      appId, beforeDeletion, {ignoreLastModifiedTime: true});

  // Select My Desktop Background.png
  await remoteCall.waitUntilSelected(appId, 'My Desktop Background.png');

  // Click delete button in the toolbar.
  if (await sendTestMessage({name: 'isTrashEnabled'}) === 'true') {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#move-to-trash-button']));
  } else {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#delete-button']));

    // Confirm that the confirmation dialog is shown.
    await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

    // Press delete button.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, ['button.cr-dialog-ok']),
        'fakeMouseClick failed');
  }

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      appId, afterDeletion, {ignoreLastModifiedTime: true});
};

/**
 * Tests that refresh button hides in selection mode.
 *
 * Non-watchable volumes (other than Recent views) display the refresh
 * button so users can refresh the file list content. However this
 * button should be hidden when entering the selection mode.
 * crbug.com/978383
 */
testcase.toolbarRefreshButtonWithSelection = async () => {
  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const documentsProviderVolumeQuery =
      '[volume-type-icon="documents_provider"]';
  await remoteCall.waitAndClickElement(appId, documentsProviderVolumeQuery);
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');

  // Ctrl+A to enter selection mode.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is not shown when the Recent view is selected.
 */
testcase.toolbarRefreshButtonHiddenInRecents = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Recent.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree [entry-label="Recent"]');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is shown for non-watchable volumes.
 */
testcase.toolbarRefreshButtonShownForNonWatchableVolume = async () => {
  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const documentsProviderVolumeQuery =
      '[volume-type-icon="documents_provider"]';
  await remoteCall.waitAndClickElement(appId, documentsProviderVolumeQuery);
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');
};

/**
 * Tests that refresh button is hidden for watchable volumes.
 */
testcase.toolbarRefreshButtonHiddenForWatchableVolume = async () => {
  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // It should start in Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that command Alt+A focus the toolbar.
 */
testcase.toolbarAltACommand = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Press Alt+A in the File List.
  const altA = ['#file-list', 'a', false, false, true];
  await remoteCall.fakeKeyDown(appId, ...altA);

  // Check that a menu-button should be focused.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  const cssClasses = focusedElement.attributes['class'] || '';
  chrome.test.assertTrue(cssClasses.includes('menu-button'));
};

/**
 * Tests that the menu drop down follows the button if the button moves. This
 * happens when the search box is expanded and then collapsed.
 */
testcase.toolbarMultiMenuFollowsButton = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

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

    if (openButton.renderedLeft !== menu.renderedLeft) {
      return pending(
          caller,
          `Waiting for the menu and button to be aligned: ` +
              `${openButton.renderedLeft} != ${menu.renderedLeft}`);
    }
  });
};

/**
 * Tests that the sharesheet button is enabled and executable.
 */
testcase.toolbarSharesheetButtonWithSelection = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_fake', []],
  };
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
};

/**
 * Tests that the sharesheet command in context menu is enabled and executable.
 */
testcase.toolbarSharesheetContextMenuWithSelection = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_fake', []],
  };
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
};

/**
 * Tests that the sharesheet item is hidden if no entry is selected.
 */
testcase.toolbarSharesheetNoEntrySelected = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
};

/**
 * Tests that the cloud icon does not appear if bulk pinning is disabled.
 */
testcase.toolbarCloudIconShouldNotShowWhenBulkPinningDisabled = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
};

/**
 * Tests that the cloud icon does not appear if the bulk pinning preference is
 * disabled and the supplied Stage does not have a UI state in the progress
 * panel.
 */
testcase
    .toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIStateAvailable =
    async () => {
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});

  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
};

/**
 * Tests that the cloud icon should only show when the bulk pinning is in
 * progress.
 */
testcase.toolbarCloudIconShouldShowForInProgress = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 1 GB, the test files
  // initialized on the Drive root are 92 KB so well below the 1GB space
  // requirement.
  await sendTestMessage(
      {name: 'setSpacedFreeSpace', freeSpace: 1024 * 1024 * 1024});

  // Enable the bulk pinning preference and assert the cloud button is no longer
  // hidden.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');
};

/**
 * Tests that the cloud icon should show when there is not enough disk space
 * available to pin.
 */
testcase.toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space available as 100 MB, this will trigger the
  // `NotEnoughSpace` stage for bulk pinning.
  await sendTestMessage({name: 'setSpacedFreeSpace', freeSpace: 100 * 1024});

  // Enable the bulk pinning preference and even though the end state is an
  // error, there is a UI state to show so the toolbar should still be visible.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');
};


/**
 * Tests that the cloud icon should not show if an error state has been
 * returned (in this case `CannotGetFreeSpace`).
 */
testcase.toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 1 GB.
  await sendTestMessage(
      {name: 'setSpacedFreeSpace', freeSpace: 1024 * 1024 * 1024});

  // Enable the bulk pinning preference and assert the cloud button is shown.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElementLost(appId, '#cloud-button[hidden]');

  // Mock the free space available as -1 which indicates an error returned
  // during the free space retrieval.
  await sendTestMessage({name: 'setSpacedFreeSpace', freeSpace: -1});

  // Force the bulk pinning manager to check for free space again (this
  // currently is done on a 60s poll).
  await sendTestMessage({name: 'forcePinManagerSpaceCheck'});
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
};

/**
 * Tests that when the cloud icon is pressed the xf-cloud-panel moves into space
 * and resizes correctly.
 */
testcase.toolbarCloudIconWhenPressedShouldOpenCloudPanel = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Mock the free space returned by spaced to be 1 GB.
  await sendTestMessage(
      {name: 'setSpacedFreeSpace', freeSpace: 1024 * 1024 * 1024});

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
  const caller = getCaller();
  await repeatUntil(async () => {
    const styles = await remoteCall.waitForElementStyles(
        appId, ['xf-cloud-panel', 'cr-action-menu', 'dialog'], ['left']);

    if (styles.renderedHeight > 0 && styles.renderedWidth > 0 &&
        styles.renderedTop > 0 && styles.renderedLeft > 0) {
      return true;
    }

    return pending(
        caller, `Waiting for xf-cloud-panel to appear on left click.`);
  });
};

/**
 * Tests that the cloud icon should not show if bulk pinning is paused (which
 * represents an offline state) and the user preference is disabled.
 */
testcase.toolbarCloudIconShouldNotShowWhenPrefDisabled = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');

  // Force the bulk pinning preference off.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});

  // Mock the free space returned by spaced to be 1 GB.
  await sendTestMessage(
      {name: 'setSpacedFreeSpace', freeSpace: 1024 * 1024 * 1024});

  // Set the bulk pinning manager to enter offline mode. This will surface a
  // `PAUSED` state which has a UI representation iff the pref is enabled. This
  // is done to ensure the bulk pinning doesn't finish before our assertions are
  // able to run (small amount of test files make this finish super quick).
  await sendTestMessage({name: 'setBulkPinningOnline', enabled: false});

  // Force the bulk pinning to calculate required space which will kick it into
  // a `PAUSED` state from a `STOPPED` state.
  await sendTestMessage({name: 'forceBulkPinningCalculateRequiredSpace'});

  // Assert the stage is `PAUSED` and the cloud button is still hidden.
  const stage = await sendTestMessage({name: 'getBulkPinningStage'});
  chrome.test.assertEq('Paused', stage);
  await remoteCall.waitForElement(appId, '#cloud-button[hidden]');
};
