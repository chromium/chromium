// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {DOWNLOADS_FAKE_TASKS} from './tasks.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
// @ts-ignore: error TS4111: Property 'toolbarDeleteWithMenuItemNoEntrySelected'
// comes from an index signature, so it must be accessed with
// ['toolbarDeleteWithMenuItemNoEntrySelected'].
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
// @ts-ignore: error TS4111: Property
// 'toolbarDeleteButtonOpensDeleteConfirmDialog' comes from an index signature,
// so it must be accessed with ['toolbarDeleteButtonOpensDeleteConfirmDialog'].
testcase.toolbarDeleteButtonOpensDeleteConfirmDialog = async () => {
  // Open Files app.
  const appId =
      // @ts-ignore: error TS4111: Property 'desktop' comes from an index
      // signature, so it must be accessed with ['desktop'].
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.desktop]);

  // Select My Desktop Background.png
  // @ts-ignore: error TS4111: Property 'desktop' comes from an index signature,
  // so it must be accessed with ['desktop'].
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
// @ts-ignore: error TS4111: Property 'toolbarDeleteButtonKeepFocus' comes from
// an index signature, so it must be accessed with
// ['toolbarDeleteButtonKeepFocus'].
testcase.toolbarDeleteButtonKeepFocus = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // USB delete never uses trash and always shows the delete dialog.

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
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
};

/**
 * Tests deleting an entry using the toolbar.
 */
// @ts-ignore: error TS4111: Property 'toolbarDeleteEntry' comes from an index
// signature, so it must be accessed with ['toolbarDeleteEntry'].
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
      // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime:
      // true; }' is not assignable to parameter of type '{ orderCheck: boolean
      // | null | undefined; ignoreFileSize: boolean | null | undefined;
      // ignoreLastModifiedTime: boolean | null | undefined; }'.
      appId, beforeDeletion, {ignoreLastModifiedTime: true});

  // Select My Desktop Background.png
  await remoteCall.waitUntilSelected(appId, 'My Desktop Background.png');

  // Click move to trash button in the toolbar.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#move-to-trash-button']));

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime:
      // true; }' is not assignable to parameter of type '{ orderCheck: boolean
      // | null | undefined; ignoreFileSize: boolean | null | undefined;
      // ignoreLastModifiedTime: boolean | null | undefined; }'.
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
// @ts-ignore: error TS4111: Property 'toolbarRefreshButtonWithSelection' comes
// from an index signature, so it must be accessed with
// ['toolbarRefreshButtonWithSelection'].
testcase.toolbarRefreshButtonWithSelection = async () => {
  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByType('documents_provider');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');

  // Ctrl+A to enter selection mode.
  const ctrlA = ['#file-list', 'a', true, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is not shown when the Recent view is selected.
 */
// @ts-ignore: error TS4111: Property 'toolbarRefreshButtonHiddenInRecents'
// comes from an index signature, so it must be accessed with
// ['toolbarRefreshButtonHiddenInRecents'].
testcase.toolbarRefreshButtonHiddenInRecents = async () => {
  // Open files app.
  const appId =
      // @ts-ignore: error TS4111: Property 'beautiful' comes from an index
      // signature, so it must be accessed with ['beautiful'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Recent.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByLabel('Recent');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is shown for non-watchable volumes.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarRefreshButtonShownForNonWatchableVolume' comes from an index
// signature, so it must be accessed with
// ['toolbarRefreshButtonShownForNonWatchableVolume'].
testcase.toolbarRefreshButtonShownForNonWatchableVolume = async () => {
  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume (which is non-watchable)
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByType('documents_provider');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');
};

/**
 * Tests that refresh button is hidden for watchable volumes.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarRefreshButtonHiddenForWatchableVolume' comes from an index signature,
// so it must be accessed with ['toolbarRefreshButtonHiddenForWatchableVolume'].
testcase.toolbarRefreshButtonHiddenForWatchableVolume = async () => {
  // Open Files app on local Downloads.
  const appId =
      // @ts-ignore: error TS4111: Property 'beautiful' comes from an index
      // signature, so it must be accessed with ['beautiful'].
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
// @ts-ignore: error TS4111: Property 'toolbarAltACommand' comes from an index
// signature, so it must be accessed with ['toolbarAltACommand'].
testcase.toolbarAltACommand = async () => {
  // Open files app.
  const appId =
      // @ts-ignore: error TS4111: Property 'beautiful' comes from an index
      // signature, so it must be accessed with ['beautiful'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Press Alt+A in the File List.
  const altA = ['#file-list', 'a', false, false, true];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
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
// @ts-ignore: error TS4111: Property 'toolbarMultiMenuFollowsButton' comes from
// an index signature, so it must be accessed with
// ['toolbarMultiMenuFollowsButton'].
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
  // @ts-ignore: error TS7030: Not all code paths return a value.
  await repeatUntil(async () => {
    const openButton =
        await remoteCall.waitForElementStyles(appId, '#tasks', ['width']);
    const menu =
        await remoteCall.waitForElementStyles(appId, '#tasks-menu', ['width']);

    if (openButton.renderedLeft !== menu.renderedLeft) {
      return pending(
          caller,
          `Waiting for the menu and button to be aligned: ` +
              `${openButton.renderedLeft} !== ${menu.renderedLeft}`);
    }
  });
};

/**
 * Tests that the sharesheet button is enabled and executable.
 */
// @ts-ignore: error TS4111: Property 'toolbarSharesheetButtonWithSelection'
// comes from an index signature, so it must be accessed with
// ['toolbarSharesheetButtonWithSelection'].
testcase.toolbarSharesheetButtonWithSelection = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    // @ts-ignore: error TS2353: Object literal may only specify known
    // properties, and ''chrome.fileManagerPrivate.invokeSharesheet'' does not
    // exist in type '{ 'chrome.fileManagerPrivate.sharesheetHasTargets':
    // (string | boolean[])[]; }'.
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_fake', []],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  const entry = ENTRIES.hello;

  // Select an entry in the file list.
  // @ts-ignore: error TS18048: 'entry' is possibly 'undefined'.
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
// @ts-ignore: error TS4111: Property
// 'toolbarSharesheetContextMenuWithSelection' comes from an index signature, so
// it must be accessed with ['toolbarSharesheetContextMenuWithSelection'].
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
    // @ts-ignore: error TS2353: Object literal may only specify known
    // properties, and ''chrome.fileManagerPrivate.invokeSharesheet'' does not
    // exist in type '{ 'chrome.fileManagerPrivate.sharesheetHasTargets':
    // (string | boolean[])[]; }'.
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
// @ts-ignore: error TS4111: Property 'toolbarSharesheetNoEntrySelected' comes
// from an index signature, so it must be accessed with
// ['toolbarSharesheetNoEntrySelected'].
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
      // @ts-ignore: error TS1345: An expression of type 'void' cannot be tested
      // for truthiness.
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
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldNotShowWhenBulkPinningDisabled' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShouldNotShowWhenBulkPinningDisabled'].
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
    // @ts-ignore: error TS4111: Property
    // 'toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIStateAvailable'
    // comes from an index signature, so it must be accessed with
    // ['toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIStateAvailable'].
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
// @ts-ignore: error TS4111: Property 'toolbarCloudIconShouldShowForInProgress'
// comes from an index signature, so it must be accessed with
// ['toolbarCloudIconShouldShowForInProgress'].
testcase.toolbarCloudIconShouldShowForInProgress = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
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
};

/**
 * Tests that the cloud icon should show when there is not enough disk space
 * available to pin.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned'].
testcase.toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
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
};


/**
 * Tests that the cloud icon should not show if an error state has been
 * returned (in this case `CannotGetFreeSpace`).
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace'].
testcase.toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
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
};

/**
 * Tests that when the cloud icon is pressed the xf-cloud-panel moves into space
 * and resizes correctly.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconWhenPressedShouldOpenCloudPanel' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconWhenPressedShouldOpenCloudPanel'].
testcase.toolbarCloudIconWhenPressedShouldOpenCloudPanel = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
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
};

/**
 * Tests that the cloud icon should not show if bulk pinning is paused (which
 * represents an offline state) and the user preference is disabled.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldNotShowWhenPrefDisabled' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShouldNotShowWhenPrefDisabled'].
testcase.toolbarCloudIconShouldNotShowWhenPrefDisabled = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
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
};

/**
 * Tests that the cloud icon should show if bulk pinning is paused (which
 * represents an offline state) and the user preference is enabled.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldShowWhenPausedState' comes from an index signature, so
// it must be accessed with ['toolbarCloudIconShouldShowWhenPausedState'].
testcase.toolbarCloudIconShouldShowWhenPausedState = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
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
};

/**
 * Tests that the cloud icon should show when a Files app window has started.
 * This mainly tests that on startup the bulk pin progress is fetched and
 * doesn't require an async event to show.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldShowOnStartupEvenIfSyncing' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShouldShowOnStartupEvenIfSyncing'].
testcase.toolbarCloudIconShouldShowOnStartupEvenIfSyncing = async () => {
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
  // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
  // parameter of type 'FilesAppState'.
  const appId = await openNewWindow(RootPath.DRIVE, /*appState=*/ {});
  await remoteCall.waitForElement(appId, '#detail-table');
  await remoteCall.waitForElement(appId, '#cloud-button:not([hidden])');

  // The underlying pin manager has a 60s timer to get free disk space. When
  // this happens it emits a progress event and updates the UI. However, once
  // the cloud button is visible the `<xf-cloud-panel>` should have it's data
  // set. To bypass async issues, only wait 10s to check the data is available
  // to ensure its done prior to the 60s free disk space check.
  await remoteCall.waitForCloudPanelState(
      appId, /*items=*/ 1, /*percentage=*/ 100);
};

/**
 * Tests that the cloud icon should show if bulk pinning is paused due to being
 * on a metered network.
 */
// @ts-ignore: error TS4111: Property
// 'toolbarCloudIconShouldShowWhenOnMeteredNetwork' comes from an index
// signature, so it must be accessed with
// ['toolbarCloudIconShouldShowWhenOnMeteredNetwork'].
testcase.toolbarCloudIconShouldShowWhenOnMeteredNetwork = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);
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
};
