// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets up for directory tree context menu test. In addition to normal setup, we
 * add destination directory.
 */
function setupForDirectoryTreeContextMenuTest() {
  var windowId;
  return setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS).then(function(results) {
    windowId = results.windowId;

    // Add destination directory.
    return new addEntries(['local'], [new TestEntryInfo({
                            type: EntryType.DIRECTORY,
                            targetPath: 'destination',
                            lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                            nameText: 'destination',
                            sizeText: '--',
                            typeText: 'Folder'
                          })]);
  }).then(function() {
    return windowId;
  });
}

/**
 * @const
 */
var ITEMS_IN_DEST_DIR_BEFORE_PASTE = TestEntryInfo.getExpectedRows([]);

/**
 * @const
 */
var ITEMS_IN_DEST_DIR_AFTER_PASTE =
    TestEntryInfo.getExpectedRows([new TestEntryInfo({
      type: EntryType.DIRECTORY,
      targetPath: 'photos',
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      nameText: 'photos',
      sizeText: '--',
      typeText: 'Folder'
    })]);

/**
 * Clicks context menu item of id in directory tree.
 */
function clickDirectoryTreeContextMenuItem(windowId, path, id) {
  const contextMenu = '#directory-tree-context-menu:not([hidden])';

  return remoteCall.callRemoteTestUtil('focus', windowId,
      [`[full-path-for-testing="${path}"]`]).then(function(result) {
    chrome.test.assertTrue(!!result, 'focus failed');
    // Right click photos directory.
    return remoteCall.callRemoteTestUtil('fakeMouseRightClick', windowId,
        [`[full-path-for-testing="${path}"]`]);
  }).then(function(result) {
    chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
    // Check: context menu item |id| should be shown enabled.
    return remoteCall.waitForElement(windowId,
        `${contextMenu} [command="#${id}"]:not([disabled])`);
  }).then(function() {
    // Click the menu item specified by |id|.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        [`${contextMenu} [command="#${id}"]`]);
  });
}

/**
 * Navigates to destination directory and test paste operation to check whether
 * the paste operation is done correctly or not. This method does NOT check
 * source entry is deleted or not for cut operation.
 */
function navigateToDestinationDirectoryAndTestPaste(windowId) {
  // Navigates to destination directory.
  return remoteCall
      .navigateWithDirectoryTree(windowId, '/destination', 'My files/Downloads')
      .then(function() {
        // Confirm files before paste.
        return remoteCall.waitForFiles(
            windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
            {ignoreLastModifiedTime: true});
      })
      .then(function() {
        // Paste
        return remoteCall.callRemoteTestUtil(
            'fakeKeyDown', windowId,
            ['body', 'v', true /* ctrl */, false, false]);
      })
      .then(function() {
        // Confirm the photos directory is pasted correctly.
        return remoteCall.waitForFiles(
            windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
            {ignoreLastModifiedTime: true});
      });
}

/**
 * Rename photos directory to specified name by using directory tree.
 */
function renamePhotosDirectoryTo(windowId, newName, useKeyboardShortcut) {
  return (useKeyboardShortcut ?
      remoteCall.callRemoteTestUtil(
          'fakeKeyDown', windowId,
          ['body', 'Enter', true /* ctrl */, false, false]) :
      clickDirectoryTreeContextMenuItem(windowId, '/photos', 'rename')
      ).then(function() {
    return remoteCall.waitForElement(windowId, '.tree-row > input');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', newName]);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId,
        ['.tree-row > input', 'Enter', false, false, false]);
  });
}

/**
 * Renames directory and confirm current directory is moved to the renamed
 * directory.
 */
function renameDirectoryFromDirectoryTreeSuccessCase(useKeyboardShortcut) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    return renamePhotosDirectoryTo(windowId, 'New photos', useKeyboardShortcut);
  }).then(function() {
    // Confirm that current directory has moved to new folder.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, '/My files/Downloads/New photos');
  });
}

/**
 * Renames directory and confirms that an alert dialog is shown.
 */
function renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(newName) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    return renamePhotosDirectoryTo(windowId, newName, false);
  }).then(function() {
    // Confirm that a dialog is shown.
    return remoteCall.waitForElement(windowId, '.cr-dialog-container.shown');
  });
}

/**
 * Creates directory from directory tree.
 */
function createDirectoryFromDirectoryTree(
    useKeyboardShortcut, changeCurrentDirectory) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;

    if (changeCurrentDirectory)
      return remoteCall.navigateWithDirectoryTree(
          windowId, '/photos', 'My files/Downloads');
    else
      return remoteCall.expandDownloadVolumeInDirectoryTree(windowId);
  }).then(function() {
    if (useKeyboardShortcut) {
      return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
          ['body', 'e', true /* ctrl */, false, false]);
    } else {
      return clickDirectoryTreeContextMenuItem(
          windowId, '/photos', 'new-folder');
    }
  }).then(function() {
    return remoteCall.waitForElement(windowId, '.tree-row > input');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', 'test']);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId,
        ['.tree-row > input', 'Enter', false, false, false]);
  }).then(function() {
    // Confirm that new directory is added to the directory tree.
    return remoteCall.waitForElement(
        windowId, '[full-path-for-testing="/photos/test"]');
  }).then(function() {
    // Confirm that current directory is not changed at this timing.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId,
        changeCurrentDirectory ? '/My files/Downloads/photos' :
                                 '/My files/Downloads');
  }).then(function() {
    // Confirm that new directory is actually created by navigating to it.
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos/test', 'My files/Downloads');
  });
}

/**
 * Tests copying a directory from directory tree with context menu.
 */
testcase.dirCopyWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'copy');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests copying a directory from directory tree with the keyboard shortcut.
 */
testcase.dirCopyWithKeyboard = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    // Press Ctrl+C.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'c', true /* ctrl */, false, false]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests copying a directory without changing the current directory.
 */
testcase.dirCopyWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.expandDownloadVolumeInDirectoryTree(windowId);
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'copy');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests cutting a directory with the context menu.
 */
testcase.dirCutWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'cut');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
    // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }));
};

/**
 * Tests cutting a directory with the keyboard shortcut.
 */
testcase.dirCutWithKeyboard = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, '/photos', 'My files/Downloads');
  }).then(function() {
    // Press Ctrl+X.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'x', true /* ctrl */, false, false]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
     // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }));
};

/**
 * Tests cutting a directory without changing the current directory.
 */
testcase.dirCutWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.expandDownloadVolumeInDirectoryTree(windowId);
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'cut');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }));
};

/**
 * Tests pasting into folder with the context menu.
 */
testcase.dirPasteWithContextMenu = function() {
  var windowId;
  testPromise(
      setupForDirectoryTreeContextMenuTest()
          .then(function(id) {
            // Copy photos directory as a test data.
            windowId = id;
            return remoteCall.navigateWithDirectoryTree(
                windowId, '/photos', 'My files/Downloads');
          })
          .then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeKeyDown', windowId,
                ['body', 'c', true /* ctrl */, false, false]);
          })
          .then(function() {
            return remoteCall.navigateWithDirectoryTree(
                windowId, '/destination', 'My files/Downloads');
          })
          .then(function() {
            // Confirm files before paste.
            return remoteCall.waitForFiles(
                windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
                {ignoreLastModifiedTime: true});
          })
          .then(function() {
            return clickDirectoryTreeContextMenuItem(
                windowId, '/destination', 'paste-into-folder');
          })
          .then(function() {
            // Confirm the photos directory is pasted correctly.
            return remoteCall.waitForFiles(
                windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
                {ignoreLastModifiedTime: true});
          })
          .then(function() {
            // Expand the directory tree.
            return remoteCall.waitForElement(
                windowId,
                '[full-path-for-testing="/destination"] .expand-icon');
          })
          .then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeMouseClick', windowId,
                ['[full-path-for-testing="/destination"] .expand-icon']);
          })
          .then(function() {
            // Confirm the copied directory is added to the directory tree.
            return remoteCall.waitForElement(
                windowId, '[full-path-for-testing="/destination/photos"]');
          }));
};

/**
 * Tests pasting into a folder without changing the current directory.
 */
testcase.dirPasteWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.expandDownloadVolumeInDirectoryTree(windowId);
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'copy');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, '/destination', 'paste-into-folder');
  }).then(function() {
    return remoteCall.waitForElement(windowId,
        '[full-path-for-testing="/destination"][may-have-children]');
  }).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['[full-path-for-testing="/destination"] .expand-icon']);
  }).then(function() {
    // Confirm the copied directory is added to the directory tree.
    return remoteCall.waitForElement(windowId,
        '[full-path-for-testing="/destination/photos"]');
  }));
};

/**
 * Tests renaming a folder with the context menu.
 */
testcase.dirRenameWithContextMenu = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      false /* do not use keyboard shortcut */));
};

/**
 * Tests that a child folder breadcrumbs is updated when renaming its parent
 * folder. crbug.com/885328.
 */
testcase.dirRenameUpdateChildrenBreadcrumbs = function() {
  let appId;
  testPromise(
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS)
          .then(function(results) {
            appId = results.windowId;

            // Add child-folder inside /photos/
            return new addEntries(['local'], [new TestEntryInfo({
                                    type: EntryType.DIRECTORY,
                                    targetPath: 'photos/child-folder',
                                    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                                    nameText: 'child-folder',
                                    sizeText: '--',
                                    typeText: 'Folder'
                                  })]);
          })
          .then(function() {
            // Navigate to child folder.
            return remoteCall.navigateWithDirectoryTree(
                appId, '/photos/child-folder', 'My files/Downloads');
          })
          .then(function() {
            // Rename parent folder.
            return clickDirectoryTreeContextMenuItem(appId, '/photos', 'rename')
                .then(function() {
                  return remoteCall.waitForElement(appId, '.tree-row > input');
                })
                .then(function() {
                  return remoteCall.callRemoteTestUtil(
                      'inputText', appId, ['.tree-row > input', 'photos-new']);
                })
                .then(function() {
                  const enterKey = [
                    '.tree-row > input', 'Enter', false, false, false
                  ];
                  return remoteCall.callRemoteTestUtil(
                      'fakeKeyDown', appId, enterKey);
                })
                .then(function(result) {
                  chrome.test.assertTrue(result, 'Enter key failed');
                });
          })
          .then(function() {
            // Confirm that current directory is now /Downloads, because it
            // can't find the previously selected folder
            // /Downloads/photos/child-folder, since its path/parent has been
            // renamed.
            return remoteCall.waitUntilCurrentDirectoryIsChanged(
                appId, '/My files/Downloads');
          })
          .then(function() {
            // Navigate to child-folder using the new path.
            // |navigateWithDirectoryTree| already checks for breadcrumbs to
            // match the path.
            return remoteCall.navigateWithDirectoryTree(
                appId, '/photos-new/child-folder', 'My files/Downloads');
          }));
};

/**
 * Tests renaming folder with the keyboard shortcut.
 */
testcase.dirRenameWithKeyboard = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      true /* use keyboard shortcut */));
};

/**
 * Tests renaming folder without changing the current directory.
 */
testcase.dirRenameWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.expandDownloadVolumeInDirectoryTree(windowId);
  }).then(function() {
    return remoteCall.waitForElement(
        windowId, '[full-path-for-testing="/photos"]');
  }).then(function() {
    return renamePhotosDirectoryTo(
        windowId, 'New photos', false /* Do not use keyboard shortcut. */);
  }).then(function() {
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }).then(function() {
    return remoteCall.waitForElement(
        windowId, '[full-path-for-testing="/New photos"]');
  }));
};

/**
 * Tests renaming a folder to an empty string.
 */
testcase.dirRenameToEmptyString = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(''));
};

/**
 * Tests renaming folder an existing name.
 */
testcase.dirRenameToExisting = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(
      'destination'));
};

/**
 * Tests creating a folder with the context menu.
 */
testcase.dirCreateWithContextMenu = function() {
  testPromise(createDirectoryFromDirectoryTree(
      false /* do not use keyboard shortcut */,
      true /* change current directory */));
};

/**
 * Tests creating a folder with the keyboard shortcut.
 */
testcase.dirCreateWithKeyboard = function() {
  testPromise(createDirectoryFromDirectoryTree(
      true /* use keyboard shortcut */,
      true /* change current directory */));
};

/**
 * Tests creating folder without changing the current directory.
 */
testcase.dirCreateWithoutChangingCurrent = function() {
  testPromise(createDirectoryFromDirectoryTree(
      false /* Do not use keyboard shortcut */,
      false /* Do not change current directory */));
};
