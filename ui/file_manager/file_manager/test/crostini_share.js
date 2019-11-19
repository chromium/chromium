// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const shareBase = {
  // Params for 'Share with Linux'.
  vmNameTermina: 'termina',
  vmNameSelectorLinux: 'linux',
  toastSharedTextLinux: '1 folder shared with Linux',
  toastActionTextLinux: 'Manage Linux sharing',
  enumUmaShareWithLinux: 12,
  enumUmaManageLinuxSharing: 13,
  // Params for 'Share with Plugin VM'.
  vmNamePluginVm: 'PvmDefault',
  vmNameSelectorPluginVm: 'plugin-vm',
  toastSharedTextPluginVm: '1 folder shared with Plugin VM',
  toastActionTextPluginVm: 'Manage Plugin VM sharing',
  enumUmaShareWithPluginVm: 16,
  enumUmaManagePluginVmSharing: 17,
};

const crostiniShare = {};
const pluginVmShare = {};

// Right click clickTarget, ensure menuVisible is shown,
// Click menuTarget, ensure dialog is shown with expectedText.
shareBase.verifyShareWithDialog =
    async (name, clickTarget, menuVisible, menuTarget, expectedText) => {
  const dialog = '.cr-dialog-container';
  const dialogVisible = '.cr-dialog-container.shown';
  const dialogText = '.cr-dialog-container.shown .cr-dialog-text';
  const dialogCancel = 'button.cr-dialog-cancel';

  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(clickTarget), 'right-click ' + name);
  await test.waitForElement(menuVisible);

  // Click 'Share with <VM>', verify dialog and text.
  assertTrue(test.fakeMouseClick(menuTarget, 'Share with <VM> ' + name));
  await test.waitForElement(dialogVisible);
  assertEquals(expectedText, document.querySelector(dialogText).innerText);

  // Click Cancel button to close.
  assertTrue(test.fakeMouseClick(dialogCancel));
  await test.waitForElementLost(dialog);
};

shareBase.testSharePaths = async (
    vmName, vmNameSelector, toastSharedText, toastActionText, enumUma,
    done) => {
  const share = 'share-with-' + vmNameSelector;
  const manage = 'manage-' + vmNameSelector + '-sharing';
  const menuNoShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"][hidden][disabled="disabled"]';
  const menuShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const menuNoManageSharing = '#file-context-menu:not([hidden]) ' +
      '[command="#' + manage + '"][hidden][disabled="disabled"]';
  const menuManageSharing = '#file-context-menu:not([hidden]) ' +
      '[command="#' + manage + '"]:not([hidden]):not([disabled])';
  const shareWith = '#file-context-menu [command="#' + share + '"]';
  const menuShareWithDirTree = '#directory-tree-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const menuNoShareWithDirTree = '#directory-tree-context-menu:not([hidden]) ' +
      '[command="#' + share + '"][hidden][disabled="disabled"]';
  const shareWithDirTree =
      '#directory-tree-context-menu [command="#' + share + '"]';
  const photos = '#file-list [file-name="photos"]';
  const myFilesDirTree = '#directory-tree [root-type-icon="my_files"]';
  const removableRoot = '#directory-tree [volume-type-icon="removable"]';
  const menuShareWithVolumeRoot = '#roots-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const shareWithVolumeRoot = '#roots-context-menu [command="#' + share + '"]';
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const googleDrive = '#directory-tree .tree-item [volume-type-icon="drive"]';
  const menuHidden = '#file-context-menu[hidden]';
  const androidRoot = '#directory-tree [volume-type-icon="android_files"]';

  const shareLabel = {'termina': 'Linux apps', 'PvmDefault': 'Plugin VM'};
  const givePermission = `Give ${shareLabel[vmName]} permission to modify `;

  const oldSharePaths = chrome.fileManagerPrivate.sharePathsWithCrostini;
  let sharePathsCalled = false;
  let sharePathsPersist;
  chrome.fileManagerPrivate.sharePathsWithCrostini =
      (vmName, entry, persist, callback) => {
        oldSharePaths(vmName, entry, persist, () => {
          sharePathsCalled = true;
          sharePathsPersist = persist;
          callback();
        });
      };
  const oldCrostiniUnregister = fileManager.crostini.unregisterSharedPath;
  let unregisterCalled = false;
  fileManager.crostini.unregisterSharedPath = function(vmName, entry) {
    unregisterCalled = true;
    oldCrostiniUnregister.call(fileManager.crostini, vmName, entry);
  };
  chrome.metricsPrivate.smallCounts_ = [];
  chrome.metricsPrivate.values_ = [];

  await test.setupAndWaitUntilReady();
  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuShareWith);

  // Check 'Manage <VM> sharing' is not shown in menu.
  assertTrue(!!document.querySelector(menuNoManageSharing));
  // Click on 'Share with <VM>'.
  assertTrue(test.fakeMouseClick(shareWith, 'Share with <VM>'));
  // Check sharePathsWithCrostini is called.
  await test.repeatUntil(() => {
    return sharePathsCalled || test.pending('wait for sharePathsCalled');
  });

  // Check toast is shown.
  await test.repeatUntil(() => {
    return document.querySelector('#toast').shadowRoot.querySelector(
               '#container:not([hidden])') ||
        test.pending('wait for toast');
  });
  assertEquals(
      document.querySelector('#toast')
          .shadowRoot.querySelector('#text')
          .innerText,
      toastSharedText);
  assertEquals(
      document.querySelector('#toast')
          .shadowRoot.querySelector('#action')
          .innerText,
      toastActionText);

  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuNoShareWith);

  // Check 'Manage <VM> sharing' is shown in menu.
  assertTrue(!!document.querySelector(menuManageSharing));

  // Share should persist when right-click > Share with <VM>.
  assertTrue(sharePathsPersist);
  // Validate UMAs.
  assertEquals(1, chrome.metricsPrivate.smallCounts_.length);
  assertArrayEquals(
      ['FileBrowser.CrostiniSharedPaths.Depth.Downloads', 1],
      chrome.metricsPrivate.smallCounts_[0]);
  const lastEnumUma = chrome.metricsPrivate.values_.pop();
  assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
  assertEquals(enumUma, lastEnumUma[1]);

  // Dispatch unshare event which is normally initiated when the user
  // manages shared paths in the settings page.
  const photosEntries =
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem.entries['/photos'];
  chrome.fileManagerPrivate.onCrostiniChanged.dispatchEvent(
      {eventType: 'unshare', vmName: vmName, entries: [photosEntries]});
  // Check unregisterSharedPath is called.
  await test.repeatUntil(() => {
    return unregisterCalled || test.pending('wait for unregisterCalled');
  });

  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuShareWith);

  // Verify share for MyFiles.
  await shareBase.verifyShareWithDialog(
      'MyFiles root', myFilesDirTree, menuShareWithDirTree, shareWithDirTree,
      givePermission + 'files in the My files folder');

  // Right-click 'hello.txt' file.
  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="hello.txt"]'),
      'right-click hello.txt');
  await test.waitForElement(menuNoShareWith);

  // Verify share for removable root.
  test.mountRemovable();
  await test.waitForElement(removableRoot);
  await shareBase.verifyShareWithDialog(
      'Removable root', removableRoot, menuShareWithVolumeRoot,
      shareWithVolumeRoot, givePermission + 'files in the MyUSB folder');

  // Select 'Linux files' in directory tree to show dir A in file list.
  await test.waitForElement(fakeLinuxFiles);

  assertTrue(test.fakeMouseClick(fakeLinuxFiles), 'click Linux files');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));

  // Check 'Share with <VM>' is not shown in menu for termina.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="A"]'),
      'right-click directory A');
  if (vmName === 'termina') {
    await test.waitForElement(menuNoShareWith);
  } else {
    await test.waitForElement(menuShareWith);
  }

  // Select 'Google Drive' to show dir photos in file list.
  await test.waitForElement(googleDrive);

  assertTrue(test.fakeMouseClick(googleDrive), 'click Google Drive');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_DRIVE_ENTRY_SET));

  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuShareWith);

  // Verify share with dialog for MyDrive.
  await shareBase.verifyShareWithDialog(
      'My Drive', googleDrive, menuShareWithVolumeRoot, shareWithVolumeRoot,
      givePermission + 'files in your Google Drive. ' +
          'Changes will sync to your other devices.');

  // Verify no share for Play files root.
  /** @type {MockFileSystem} */ (
      mockVolumeManager
          .getCurrentProfileVolumeInfo(
              VolumeManagerCommon.VolumeType.ANDROID_FILES)
          .fileSystem)
      .populate(['/Pictures/']);
  test.mountAndroidFiles();
  await test.waitForElement(androidRoot);
  assertTrue(test.fakeMouseRightClick(androidRoot), 'right-click Play files');
  await test.waitForElement(menuNoShareWithDirTree);

  // Verify share for folders under Play files root.
  assertTrue(test.fakeMouseClick(androidRoot), 'click Play files');
  const pictures = '#file-list [file-name="Pictures"]';
  await test.waitForElement(pictures);
  assertTrue(test.fakeMouseRightClick(pictures), 'right-click Pictures');
  await test.waitForElement(menuShareWith);

  // Reset Linux files and Play files back to unmounted.
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(fakeLinuxFiles);
  chrome.fileManagerPrivate.removeMount('android_files');

  // Restore fmp.*.
  chrome.fileManagerPrivate.sharePathsWithCrostini = oldSharePaths;
  // Restore Crostini.unregisterSharedPath.
  fileManager.crostini.unregisterSharedPath = oldCrostiniUnregister;
  done();
};

crostiniShare.testSharePaths = done => {
  shareBase.testSharePaths(
      shareBase.vmNameTermina, shareBase.vmNameSelectorLinux,
      shareBase.toastSharedTextLinux, shareBase.toastActionTextLinux,
      shareBase.enumUmaShareWithLinux, done);
};

pluginVmShare.testSharePaths = done => {
  shareBase.testSharePaths(
      shareBase.vmNamePluginVm, shareBase.vmNameSelectorPluginVm,
      shareBase.toastSharedTextPluginVm, shareBase.toastActionTextPluginVm,
      shareBase.enumUmaShareWithPluginVm, done);
};

// Verify gear menu 'Manage ? sharing'.
shareBase.testGearMenuManage =
    async (vmName, vmNameSelector, enumUma, done) => {
  const manage = '#gear-menu-manage-' + vmNameSelector + '-sharing';
  const gearMenuClosed = '#gear-menu[hidden]';
  const manageSharingOptionHidden =
      '#gear-menu:not([hidden]) ' + manage + '[hidden]';
  const manageSharingOptionShown =
      '#gear-menu:not([hidden]) ' + manage + ':not([hidden])';
  chrome.metricsPrivate.values_ = [];

  await test.setupAndWaitUntilReady();

  // Setup with crostini disabled.
  fileManager.crostini.setEnabled(vmName, false);
  // Click gear menu, ensure 'Manage <VM> sharing' is hidden.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(manageSharingOptionHidden);

  // Close gear menu.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(gearMenuClosed);

  // Setup with crostini enabled.
  fileManager.crostini.setEnabled(vmName, true);
  // Click gear menu, ensure 'Manage <VM> sharing' is shown.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(manageSharingOptionShown);

  // Click 'Manage <VM> sharing'.
  assertTrue(test.fakeMouseClick(manage));
  await test.waitForElement(gearMenuClosed);

  // Verify UMA.
  const lastEnumUma = chrome.metricsPrivate.values_.pop();
  assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
  assertEquals(enumUma, lastEnumUma[1]);
  done();
};

crostiniShare.testGearMenuManageLinuxSharing = done => {
  shareBase.testGearMenuManage(
      shareBase.vmNameTermina, shareBase.vmNameSelectorLinux,
      shareBase.enumUmaManageLinuxSharing, done);
};

pluginVmShare.testGearMenuManagePluginVmSharing = done => {
  shareBase.testGearMenuManage(
      shareBase.vmNamePluginVm, shareBase.vmNameSelectorPluginVm,
      shareBase.enumUmaManagePluginVmSharing, done);
};
