// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniShare = {};

crostiniShare.testSharePathsCrostiniSuccess = (done) => {
  const oldSharePaths = chrome.fileManagerPrivate.sharePathsWithCrostini;
  let sharePathsCalled = false;
  let sharePathsPersist;
  chrome.fileManagerPrivate.sharePathsWithCrostini =
      (entry, persist, callback) => {
        oldSharePaths(entry, persist, () => {
          sharePathsCalled = true;
          sharePathsPersist = persist;
          callback();
        });
      };
  chrome.metricsPrivate.smallCounts_ = [];
  chrome.metricsPrivate.values_ = [];

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right-click 'photos' directory.
        // Check 'Share with Linux' is shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click photos');
        return test.waitForElement(
            '#file-context-menu:not([hidden]) ' +
            '[command="#share-with-linux"]:not([hidden]):not([disabled])');
      })
      .then(() => {
        // Click on 'Share with Linux'.
        assertTrue(
            test.fakeMouseClick(
                '#file-context-menu [command="#share-with-linux"]'),
            'Share with Linux');
        // Check sharePathsWithCrostini is called.
        return test.repeatUntil(() => {
          return sharePathsCalled || test.pending('wait for sharePathsCalled');
        });
      })
      .then(() => {
        // Share should persist when right-click > Share with Linux.
        assertTrue(sharePathsPersist);
        // Validate UMAs.
        assertEquals(1, chrome.metricsPrivate.smallCounts_.length);
        assertArrayEquals(
            ['FileBrowser.CrostiniSharedPaths.Depth.Downloads', 1],
            chrome.metricsPrivate.smallCounts_[0]);
        const lastEnumUma = chrome.metricsPrivate.values_.pop();
        assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
        assertEquals(12 /* Share with Linux */, lastEnumUma[1]);

        // Restore fmp.*.
        chrome.fileManagerPrivate.sharePathsWithCrostini = oldSharePaths;
        done();
      });
};

// Verify right-click menu with 'Share with Linux' is not shown for:
// * Files (not directory).
// * Any folder already shared.
// * Root Downloads folder.
// * Any folder outside of downloads (e.g. crostini or drive).
// * Is shown for drive if DriveFS is enabled.
crostiniShare.testSharePathShown = (done) => {
  const myFiles = '#directory-tree .tree-item [root-type-icon="my_files"]';
  const downloads = '#file-list li [file-type-icon="downloads"]';
  const linuxFiles = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const googleDrive = '#directory-tree .tree-item [volume-type-icon="drive"]';
  const menuHidden = '#file-context-menu[hidden]';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  let alreadySharedPhotosDir;

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right-click 'hello.txt' file.
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="hello.txt"]'),
            'right-click hello.txt');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Set a folder as already shared.
        alreadySharedPhotosDir =
            mockVolumeManager
                .getCurrentProfileVolumeInfo(
                    VolumeManagerCommon.VolumeType.DOWNLOADS)
                .fileSystem.entries['/photos'];
        Crostini.registerSharedPath(alreadySharedPhotosDir, mockVolumeManager);
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click hello.txt');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Select 'My files' in directory tree to show Downloads in file list.
        assertTrue(test.fakeMouseClick(myFiles), 'click My files');
        return test.waitForElement(downloads);
      })
      .then(() => {
        // Right-click 'Downloads' directory.
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick(downloads), 'right-click downloads');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Select 'Linux files' in directory tree to show dir A in file list.
        return test.waitForElement(linuxFiles);
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(linuxFiles), 'click Linux files');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="A"]'),
            'right-click directory A');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Select 'Google Drive' to show dir photos in file list.
        return test.waitForElement(googleDrive);
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(googleDrive), 'click Google Drive');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_DRIVE_ENTRY_SET));
      })
      .then(() => {
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click photos');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Close menu by clicking file-list.
        assertTrue(test.fakeMouseClick('#file-list'));
        return test.waitForElement(menuHidden);
      })
      .then(() => {
        // Set DRIVE_FS_ENABLED, and check that 'Share with Linux' is shown.
        loadTimeData.data_['DRIVE_FS_ENABLED'] = true;
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click photos');
        return test.waitForElement(menuShareWithLinux);
      })
      .then(() => {
        // Reset Linux files back to unmounted.
        chrome.fileManagerPrivate.removeMount('crostini');
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Unset DRIVE_FS_ENABLED.
        loadTimeData.data_['DRIVE_FS_ENABLED'] = false;
        // Clear Crostini shared folders.
        Crostini.unregisterSharedPath(
            alreadySharedPhotosDir, mockVolumeManager);
        done();
      });
};

// Verify gear menu 'Manage Linux sharing'.
crostiniShare.testGearMenuManageLinuxSharing = (done) => {
  const gearMenuClosed = '#gear-menu[hidden]';
  const manageLinuxSharingOptionHidden =
      '#gear-menu:not([hidden]) #gear-menu-manage-linux-sharing[hidden]';
  const manageLinuxSharingOptionShown =
      '#gear-menu:not([hidden]) #gear-menu-manage-linux-sharing:not([hidden])';
  chrome.metricsPrivate.values_ = [];

  test.setupAndWaitUntilReady()
      .then(() => {
        // Setup with crostini disabled.
        chrome.fileManagerPrivate.crostiniEnabled_ = false;
        fileManager.setupCrostini_();
        return test.repeatUntil(
            () => !Crostini.IS_CROSTINI_FILES_ENABLED ||
                test.pending('crostini setup'));
      })
      .then(() => {
        // Click gear menu, ensure 'Manage Linux sharing' is hidden.
        assertTrue(test.fakeMouseClick('#gear-button'));
        return test.waitForElement(manageLinuxSharingOptionHidden);
      })
      .then(() => {
        // Close gear menu.
        assertTrue(test.fakeMouseClick('#gear-button'));
        return test.waitForElement(gearMenuClosed);
      })
      .then(() => {
        // Setup with crostini enabled.
        chrome.fileManagerPrivate.crostiniEnabled_ = true;
        fileManager.setupCrostini_();
        return test.repeatUntil(
            () => Crostini.IS_CROSTINI_FILES_ENABLED ||
                test.pending('crostini setup'));
      })
      .then(() => {
        // Click gear menu, ensure 'Manage Linux sharing' is shown.
        assertTrue(test.fakeMouseClick('#gear-button'));
        return test.waitForElement(manageLinuxSharingOptionShown);
      })
      .then(() => {
        // Click 'Manage Linux sharing'.
        assertTrue(test.fakeMouseClick('#gear-menu-manage-linux-sharing'));
        return test.waitForElement(gearMenuClosed);
      })
      .then(() => {
        // Verify UMA.
        const lastEnumUma = chrome.metricsPrivate.values_.pop();
        assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
        assertEquals(13 /* Manage Linux sharing */, lastEnumUma[1]);
        done();
      });
};
