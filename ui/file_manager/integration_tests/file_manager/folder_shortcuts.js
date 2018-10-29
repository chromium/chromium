// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function(){

/**
 * Directory tree selector constants.
 */
const TREEITEM_A = TREEITEM_DRIVE + ' [entry-label="A"] ';
const TREEITEM_B = TREEITEM_A + '[entry-label="B"] ';
const TREEITEM_C = TREEITEM_B + '[entry-label="C"] ';

const TREEITEM_D = TREEITEM_DRIVE + ' [entry-label="D"] ';
const TREEITEM_E = TREEITEM_D + '[entry-label="E"] ';

/**
 * Entry set used for the folder shortcut tests.
 * @type {Array<TestEntryInfo>}
 */
const FOLDER_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
  ENTRIES.directoryD,
  ENTRIES.directoryE,
  ENTRIES.directoryF
];

/**
 * Constants for each folder.
 * @type {Object}
 */
const DIRECTORY = {
  Drive: {
    contents: [
      ENTRIES.directoryA.getExpectedRow(), ENTRIES.directoryD.getExpectedRow()
    ],
    name: 'Drive',
    navItem: '.tree-item[entry-label="My Drive"]',
    treeItem: TREEITEM_DRIVE
  },
  A: {
    contents: [ENTRIES.directoryB.getExpectedRow()],
    name: 'A',
    navItem: '.tree-item[label="A"]',
    treeItem: TREEITEM_A
  },
  B: {
    contents: [ENTRIES.directoryC.getExpectedRow()],
    name: 'B',
    treeItem: TREEITEM_B
  },
  C: {
    contents: [],
    name: 'C',
    navItem: '.tree-item[label="C"]',
    treeItem: TREEITEM_C
  },
  D: {
    contents: [ENTRIES.directoryE.getExpectedRow()],
    name: 'D',
    navItem: '.tree-item[label="D"]',
    treeItem: TREEITEM_D
  },
  E: {
    contents: [ENTRIES.directoryF.getExpectedRow()],
    name: 'E',
    treeItem: TREEITEM_E
  }
};

/**
 * Expands a directory tree item by clicking on its expand icon.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory whose tree item should be expanded.
 * @return {Promise} Promise fulfilled on success.
 */
function expandTreeItem(appId, directory) {
  const expandIcon =
      directory.treeItem + '> .tree-row[has-children=true] > .expand-icon';
  return remoteCall.waitForElement(appId, expandIcon).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [expandIcon]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    const expandedSubtree = directory.treeItem + '> .tree-children[expanded]';
    return remoteCall.waitForElement(appId, expandedSubtree);
  });
}

/**
 * Expands whole directory tree under DIRECTORY.Drive.
 *
 * @param {string} appId Files app windowId.
 * @return {Promise} Promise fulfilled on success.
 */
function expandDirectoryTree(appId) {
  return expandTreeItem(appId, DIRECTORY.Drive).then(function() {
    return expandTreeItem(appId, DIRECTORY.A);
  }).then(function() {
    return expandTreeItem(appId, DIRECTORY.B);
  }).then(function() {
    return expandTreeItem(appId, DIRECTORY.D);
  });
}

/**
 * Navigate to |directory| (makes |directory| the current directory).
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory to navigate to.
 * @return {Promise} Promise fulfilled on success.
 */
function navigateToDirectory(appId, directory) {
  const itemIcon = directory.treeItem + '> .tree-row > .item-icon';
  return remoteCall.waitForElement(appId, itemIcon).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [itemIcon]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForFiles(appId, directory.contents);
  });
}

/**
 * Creates a folder shortcut to |directory| using the context menu. Note the
 * current directory must be a parent of the given |directory|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory of shortcut to be created.
 * @return {Promise} Promise fulfilled on success.
 */
function createShortcut(appId, directory) {
  return remoteCall.callRemoteTestUtil(
      'selectFile', appId, [directory.name]).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(appId, ['.table-row[selected]']);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseRightClick', appId, ['.table-row[selected]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(
        appId, '#file-context-menu:not([hidden])');
  }).then(function() {
    return remoteCall.waitForElement(
        appId,
        '[command="#create-folder-shortcut"]:not([hidden]):not([disabled])');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId,
        ['[command="#create-folder-shortcut"]:not([hidden]):not([disabled])']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(appId, directory.navItem);
  });
}

/**
 * Removes the folder shortcut to |directory|. Note the current directory must
 * be a parent of the given |directory|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory of shortcut to be removed.
 * @return {Promise} Promise fulfilled on success.
 */
function removeShortcut(appId, directory) {
  // Focus the item first since actions are calculated asynchronously. The
  // context menu wouldn't show if there are no visible items. Focusing first,
  // will force the actions controller to refresh actions.
  // TODO(mtomasz): Remove this hack (if possible).
  return remoteCall.callRemoteTestUtil(
      'focus', appId, [directory.navItem]).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.callRemoteTestUtil(
        'fakeMouseRightClick', appId, [directory.navItem]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(
        appId, '#roots-context-menu:not([hidden])');
  }).then(function() {
    return remoteCall.waitForElement(
        appId,
        '[command="#remove-folder-shortcut"]:not([hidden]):not([disabled])');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId,
        ['#roots-context-menu [command="#remove-folder-shortcut"]:' +
         'not([hidden])']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElementLost(appId, directory.navItem);
  });
}

/**
 * Waits until the current directory becomes |currentDir| and current selection
 * becomes the shortcut to |shortcutDir|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} currentDir Directory which should be a current directory.
 * @param {Object} shortcutDir Directory whose shortcut should be selected.
 * @return {Promise} Promise fulfilled on success.
 */
function expectSelection(appId, currentDir, shortcutDir) {
  const shortcut = shortcutDir.navItem;
  return remoteCall.waitForFiles(appId, currentDir.contents).then(function() {
    return remoteCall.waitForElement(appId, shortcut + '[selected]');
  });
}

/**
 * Clicks folder shortcut to |directory|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory whose shortcut will be clicked.
 * @return {Promise} Promise fulfilled on success.
 */
function clickShortcut(appId, directory) {
  const shortcut = directory.navItem;
  return remoteCall.waitForElement(appId, shortcut).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [shortcut]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
  });
}

/**
 * Creates some shortcuts and traverse them and some other directories.
 */
testcase.traverseFolderShortcuts = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], FOLDER_ENTRY_SET);
    },
    // Expand the directory tree.
    function(results) {
      appId = results.windowId;
      expandDirectoryTree(appId).then(this.next);
    },
    // Create a shortcut to directory D.
    function() {
      createShortcut(appId, DIRECTORY.D).then(this.next);
    },
    // Navigate to directory B.
    function() {
      navigateToDirectory(appId, DIRECTORY.B).then(this.next);
    },
    // Create a shortcut to directory C.
    function() {
      createShortcut(appId, DIRECTORY.C).then(this.next);
    },
    // Click the Drive root (My Drive) shortcut.
    function() {
      clickShortcut(appId, DIRECTORY.Drive).then(this.next);
    },
    // Check: current directory and selection should be the Drive root.
    function() {
      expectSelection(appId, DIRECTORY.Drive, DIRECTORY.Drive).then(this.next);
    },
    // Send Ctrl+3 key to file-list to select 3rd shortcut.
    function() {
      const key = ['#file-list', '3', true, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: current directory and selection should be D.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(appId, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Send UpArrow key to directory tree to select the shortcut above D.
    function() {
      const key = ['#directory-tree', 'ArrowUp', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: current directory should be D, with shortcut C selected.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(appId, DIRECTORY.D, DIRECTORY.C).then(this.next);
    },
    // Send Enter key to the directory tree to change to directory C.
    function() {
      const key = ['#directory-tree', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: current directory and selection should be C.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(appId, DIRECTORY.C, DIRECTORY.C).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Adds and removes shortcuts from other window and check if the active
 * directories and selected navigation items are correct.
 */
testcase.addRemoveFolderShortcuts = function() {
  let appId1;
  let appId2;

  function openFilesAppOnDrive() {
    let appId;
    return new Promise(function(resolve) {
      return openNewWindow(null, RootPath.DRIVE, resolve);
    }).then(function(windowId) {
      appId = windowId;
      return remoteCall.waitForElement(appId, '#file-list');
    }).then(function() {
      return remoteCall.waitForFiles(appId, DIRECTORY.Drive.contents);
    }).then(function() {
      return appId;
    });
  }

  StepsRunner.run([
    // Add entries to Drive.
    function() {
      addEntries(['drive'], FOLDER_ENTRY_SET, this.next);
    },
    // Open one Files app window on Drive.
    function(result) {
      chrome.test.assertTrue(result);
      openFilesAppOnDrive().then(this.next);
    },
    // Open another Files app window on Drive.
    function(windowId) {
      appId1 = windowId;
      openFilesAppOnDrive().then(this.next);
    },
    // Create a shortcut to D.
    function(windowId) {
      appId2 = windowId;
      createShortcut(appId1, DIRECTORY.D).then(this.next);
    },
    // Click the shortcut to D.
    function() {
      clickShortcut(appId1, DIRECTORY.D).then(this.next);
    },
    // Check: current directory and selection should be D.
    function() {
      expectSelection(appId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Create a shortcut to A from the other window.
    function() {
      createShortcut(appId2, DIRECTORY.A).then(this.next);
    },
    // Check: current directory and selection should still be D.
    function() {
      expectSelection(appId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Remove shortcut to D from the other window.
    function() {
      removeShortcut(appId2, DIRECTORY.D).then(this.next);
    },
    // Check: directory D in the directory tree should be selected.
    function() {
      const selection = TREEITEM_D + '[selected]';
      remoteCall.waitForElement(appId1, selection).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

})();
