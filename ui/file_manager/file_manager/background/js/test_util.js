// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Opens the main Files app's window and waits until it is ready.
 *
 * @param {Object} appState App state.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openMainWindow = function(appState, callback) {
  launcher.launchFileManager(appState,
                    undefined,  // opt_type
                    undefined,  // opt_id
                    callback);
};

/**
 * Returns an array with the files currently selected in the file manager.
 * TODO(hirono): Integrate the method into getFileList method.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<string>} Array of selected files.
 */
test.util.sync.getSelectedFiles = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var selected = [];
  for (var i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected')) {
      selected.push(
          rows[i].querySelector('.filename-label').textContent);
    }
  }
  return selected;
};

/**
 * Returns the name of the item currently selected in the directory tree.
 * Returns null if no entry is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {string} Name of selected tree item.
 */
test.util.sync.getSelectedTreeItem = function(contentWindow) {
  var tree = contentWindow.document.querySelector('#directory-tree');
  var items = tree.querySelectorAll('.tree-item');
  var selected = [];
  for (var i = 0; i < items.length; ++i) {
    if (items[i].hasAttribute('selected')) {
      return items[i].querySelector('.label').textContent;
    }
  }
  console.error('Unexpected; no tree item currently selected');
  return null;
};


/**
 * Returns an array with the files on the file manager's file list.
 *
 * TODO(sashab): Since we recycle DOM elements, this only returns the first ~11
 * visible elements. crbug.com/850834.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<Array<string>>} Array of rows.
 */
test.util.sync.getFileList = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var fileList = [];
  for (var j = 0; j < rows.length; ++j) {
    var row = rows[j];
    fileList.push([
      row.querySelector('.filename-label').textContent,
      row.querySelector('.size').textContent,
      row.querySelector('.type').textContent,
      row.querySelector('.date').textContent
    ]);
  }
  return fileList;
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.sync.selectFile = function(contentWindow, filename) {
  var rows = contentWindow.document.querySelectorAll('#detail-table li');
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'Home', false, false, false);
  for (var index = 0; index < rows.length; ++index) {
    var selection = test.util.sync.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename)
      return true;
    test.util.sync.fakeKeyDown(
        contentWindow, '#file-list', 'ArrowDown', false, false, false);
  }
  console.error('Failed to select file "' + filename + '"');
  return false;
};

/**
 * Open the file by selectFile and fakeMouseDoubleClick.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be opened.
 * @return {boolean} True if file got selected and a double click message is
 *     sent, false otherwise.
 */
test.util.sync.openFile = function(contentWindow, filename) {
  var query = '#file-list li.table-row[selected] .filename-label span';
  return test.util.sync.selectFile(contentWindow, filename) &&
         test.util.sync.fakeMouseDoubleClick(contentWindow, query);
};

/**
 * Selects a volume specified by its icon name.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} iconName Name of the volume icon.
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether the target is found and mousedown and click events are sent.
 */
test.util.async.selectVolume = function(contentWindow, iconName, callback) {
  const query = '#directory-tree [volume-type-icon=' + iconName + ']';
  const isDriveSubVolume = iconName == 'drive_recent' ||
      iconName == 'drive_shared_with_me' || iconName == 'drive_offline';
  return test.util.async.selectInDirectoryTree(
      contentWindow, query, isDriveSubVolume, callback);

};

/**
 * Selects element in directory tree specified by query selector.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query to find the element to be selected on directory tree.
 * @param {boolean} isDriveSubVolume set to true if query is targeted to a
 *     sub-item of Drive.
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether the target is found and mousedown and click events are sent.
 */
test.util.async.selectInDirectoryTree = function(
    contentWindow, query, isDriveSubVolume, callback) {
  const driveQuery = '#directory-tree [volume-type-icon=drive]';
  let preSelection = false;
  const steps = {
    checkQuery: function() {
      if (contentWindow.document.querySelector(query)) {
        steps.sendEvents();
        return;
      }
      // If the target volume is sub-volume of drive, we must click 'drive'
      // before clicking the sub-item.
      if (!preSelection) {
        if (!isDriveSubVolume) {
          callback(false);
          return;
        }
        if (!(test.util.sync.fakeMouseDown(contentWindow, driveQuery) &&
              test.util.sync.fakeMouseClick(contentWindow, driveQuery))) {
          callback(false);
          return;
        }
        preSelection = true;
      }
      setTimeout(steps.checkQuery, 50);
    },
    sendEvents: function() {
      // To change the selected volume, we have to send both events 'mousedown'
      // and 'click' to the navigation list.
      callback(test.util.sync.fakeMouseDown(contentWindow, query) &&
               test.util.sync.fakeMouseClick(contentWindow, query));
    }
  };
  steps.checkQuery();
};

/**
 * Fakes pressing the down arrow until the given |folderName| is selected in the
 * navigation tree.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} folderName Name of the folder to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.sync.selectFolderInTree = function(contentWindow, folderName) {
  var items =
      contentWindow.document.querySelectorAll('#directory-tree .tree-item');
  test.util.sync.fakeKeyDown(
      contentWindow, '#directory-tree', 'Home', false, false, false);
  for (var index = 0; index < items.length; ++index) {
    var selectedTreeItemName =
        test.util.sync.getSelectedTreeItem(contentWindow);
    if (selectedTreeItemName === folderName) {
      test.util.sync.fakeKeyDown(
          contentWindow, '#directory-tree', 'Enter', false, false, false);
      return true;
    }
    test.util.sync.fakeKeyDown(
        contentWindow, '#directory-tree', 'ArrowDown', false, false, false);
  }

  console.error('Failed to select folder in tree "' + folderName + '"');
  return false;
};

/**
 * Fakes pressing the right arrow to expand the selected folder in the
 * navigation tree.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True if folder got expanded, false otherwise.
 */
test.util.sync.expandSelectedFolderInTree = function(contentWindow) {
  var selectedItem = contentWindow.document.querySelector(
      '#directory-tree .tree-item[selected]');
  if (!selectedItem) {
    console.error('Unexpected; no tree item currently selected.');
    return false;
  }
  test.util.sync.fakeKeyDown(
      contentWindow, '#directory-tree .tree-item[selected]', 'ArrowRight',
      false, false, false);
  return true;
};

/**
 * Fakes pressing the left arrow to collapse the selected folder in the
 * navigation tree.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True if folder got expanded, false otherwise.
 */
test.util.sync.collapseSelectedFolderInTree = function(contentWindow) {
  var selectedItem = contentWindow.document.querySelector(
      '#directory-tree .tree-item[selected]');
  if (!selectedItem) {
    console.error('Unexpected; no tree item currently selected.');
    return false;
  }
  test.util.sync.fakeKeyDown(
      contentWindow, '#directory-tree .tree-item[selected]', 'ArrowLeft',
      false, false, false);
  return true;
};

/**
 * Fakes pressing the down arrow until the given |folderName| is selected in the
 * navigation tree.
 * TODO(sashab): Merge this functionality into selectTeamDrive and remove this
 * function.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} folderName Name of the folder to be selected.
 * @return {boolean} True if Team Drive folder got selected, false otherwise.
 */
test.util.sync.selectTeamDrive = function(contentWindow, teamDriveName) {
  // Select + expand Team Drives gran root.
  const teamDrivesSelector = '#directory-tree .tree-item ' +
      '[entry-label="Team Drives"]:not([hidden])';
  if (!test.util.sync.fakeMouseClick(contentWindow, teamDrivesSelector))
    return false;

  // Expand the 'Team Drives' root.
  if (!test.util.sync.expandSelectedFolderInTree(contentWindow))
    return false;

  // Select the team drive folder.
  const teamDriveNameSelector = '#directory-tree .tree-item ' +
      '[entry-label="' + teamDriveName + '"]:not([hidden])';
  if (!test.util.sync.fakeMouseClick(contentWindow, teamDriveNameSelector))
    return false;

  return true;
};


/**
 * Obtains visible tree items.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {!Array<string>} List of visible item names.
 */
test.util.sync.getTreeItems = function(contentWindow) {
  var items = contentWindow.document.querySelectorAll(
      '#directory-tree .tree-item');
  var result = [];
  for (var i = 0; i < items.length; i++) {
    if (items[i].matches('.tree-children:not([expanded]) *'))
      continue;
    result.push(items[i].querySelector('.entry-name').textContent);
  }
  return result;
};

/**
 * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
 *
 * @param {Window} contentWindow The window where visitURL() was called.
 * @return {!string} The URL of the last URL visited.
 */
test.util.sync.getLastVisitedURL = function(contentWindow) {
  return contentWindow.util.getLastVisitedURL();
};

/**
 * Executes Javascript code on a webview and returns the result.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} webViewQuery Selector for the web view.
 * @param {string} code Javascript code to be executed within the web view.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.executeScriptInWebView = function(
    contentWindow, webViewQuery, code, callback) {
  var webView = contentWindow.document.querySelector(webViewQuery);
  webView.executeScript({code: code}, callback);
};

/**
 * Selects |filename| and fakes pressing Ctrl+C, Ctrl+V (copy, paste).
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be copied.
 * @return {boolean} True if copying got simulated successfully. It does not
 *     say if the file got copied, or not.
 */
test.util.sync.copyFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Ctrl+C and Ctrl+V
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'c', true, false, false);
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'v', true, false, false);
  return true;
};

/**
 * Selects |filename| and fakes pressing the Delete key.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be deleted.
 * @return {boolean} True if deleting got simulated successfully. It does not
 *     say if the file got deleted, or not.
 */
test.util.sync.deleteFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Delete
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'Delete', false, false, false);
  return true;
};

/**
 * Execute a command on the document in the specified window.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} command Command name.
 * @return {boolean} True if the command is executed successfully.
 */
test.util.sync.execCommand = function(contentWindow, command) {
  return contentWindow.document.execCommand(command);
};

/**
 * Override the installWebstoreItem method in private api for test.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} expectedItemId Item ID to be called this method with.
 * @param {?string} intendedError Error message to be returned when the item id
 *     matches. 'null' represents no error.
 * @return {boolean} Always return true.
 */
test.util.sync.overrideInstallWebstoreItemApi =
    function(contentWindow, expectedItemId, intendedError) {
  var setLastError = function(message) {
    contentWindow.chrome.runtime.lastError =
        message ? {message: message} : undefined;
  };

  var installWebstoreItem = function(itemId, silentInstallation, callback) {
    setTimeout(function() {
      if (itemId !== expectedItemId) {
        setLastError('Invalid Chrome Web Store item ID');
        callback();
        return;
      }

      setLastError(intendedError);
      callback();
    }, 0);
  };

  test.util.executedTasks_ = [];
  contentWindow.chrome.webstoreWidgetPrivate.installWebstoreItem =
      installWebstoreItem;
  return true;
};

/**
 * Override the task-related methods in private api for test.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<Object>} taskList List of tasks to be returned in
 *     fileManagerPrivate.getFileTasks().
 * @return {boolean} Always return true.
 */
test.util.sync.overrideTasks = function(contentWindow, taskList) {
  var getFileTasks = function(entries, onTasks) {
    // Call onTask asynchronously (same with original getFileTasks).
    setTimeout(function() {
      onTasks(taskList);
    }, 0);
  };

  var executeTask = function(taskId, entry) {
    test.util.executedTasks_.push(taskId);
  };

  var setDefaultTask = function(taskId) {
    for (var i = 0; i < taskList.length; i++) {
      taskList[i].isDefault = taskList[i].taskId === taskId;
    }
  };

  test.util.executedTasks_ = [];
  contentWindow.chrome.fileManagerPrivate.getFileTasks = getFileTasks;
  contentWindow.chrome.fileManagerPrivate.executeTask = executeTask;
  contentWindow.chrome.fileManagerPrivate.setDefaultTask = setDefaultTask;
  return true;
};

/**
 * Obtains the list of executed tasks.
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<string>} List of executed task ID.
 */
test.util.sync.getExecutedTasks = function(contentWindow) {
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    return null;
  }
  return test.util.executedTasks_;
};

/**
 * Runs the 'Move to profileId' menu.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} profileId Destination profile's ID.
 * @return {boolean} True if the menu is found and run.
 */
test.util.sync.runVisitDesktopMenu = function(contentWindow, profileId) {
  var list = contentWindow.document.querySelectorAll('.visit-desktop');
  for (var i = 0; i < list.length; ++i) {
    if (list[i].label.indexOf(profileId) != -1) {
      var activateEvent = contentWindow.document.createEvent('Event');
      activateEvent.initEvent('activate', false, false);
      list[i].dispatchEvent(activateEvent);
      return true;
    }
  }
  return false;
};

/**
 * Calls the unload handler for the window.
 * @param {Window} contentWindow Window to be tested.
 */
test.util.sync.unload = function(contentWindow) {
  contentWindow.fileManager.onUnload_();
};

/**
 * Obtains the path which is shown in the breadcrumb.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {string} Path which is shown in the breadcrumb.
 */
test.util.sync.getBreadcrumbPath = function(contentWindow) {
  var breadcrumb = contentWindow.document.querySelector(
      '#location-breadcrumbs');
  var paths = breadcrumb.querySelectorAll('.breadcrumb-path');

  var path = '';
  for(var i = 0; i < paths.length; i++) {
    path += '/' + paths[i].textContent;
  }
  return path;
};

/**
 * Obtains the preferences.
 * @param {function(Object)} callback Callback function with results returned by
 *     the script.
 */
test.util.async.getPreferences = function(callback) {
  chrome.fileManagerPrivate.getPreferences(callback);
};

// Register the test utils.
test.util.registerRemoteTestUtils();
