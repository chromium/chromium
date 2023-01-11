// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../../common/js/files_app_state.js';
import {ProgressCenterItem} from '../../common/js/progress_center_common.js';
import {util} from '../../common/js/util.js';

import {background} from './file_manager_base.js';
import {launcher} from './launcher.js';
import {test} from './test_util_base.js';

export {test};

/**
 * Sanitizes the formatted date. Replaces unusual space with normal space.
 * @param {string} strDate the date already in the string format.
 * @return {string}
 */
export function sanitizeDate(strDate) {
  return strDate.replace('\u202f', ' ');
}
/**
 * Opens the main Files app's window and waits until it is ready.
 *
 * @param {!FilesAppState} appState App state.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openMainWindow = (appState, callback) => {
  launcher.launchFileManager(appState).then(callback);
};

/**
 * Returns the name of the item currently selected in the directory tree.
 * Returns null if no entry is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {string} Name of selected tree item.
 */
test.util.sync.getSelectedTreeItem = contentWindow => {
  const tree = contentWindow.document.querySelector('#directory-tree');
  const items = tree.querySelectorAll('.tree-item');
  const selected = [];
  for (let i = 0; i < items.length; ++i) {
    if (items[i].hasAttribute('selected')) {
      return items[i].querySelector('.label').textContent;
    }
  }
  console.error('Unexpected; no tree item currently selected');
  return null;
};

/**
 * Returns details about each file shown in the file list: name, size, type and
 * modification time.
 *
 * Since FilesApp normally has a fixed display size in test, and also since the
 * #detail-table recycles its file row elements, this call only returns details
 * about the visible file rows (11 rows normally, see crbug.com/850834).
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<Array<string>>} Details for each visible file row.
 */
test.util.sync.getFileList = contentWindow => {
  const table = contentWindow.document.querySelector('#detail-table');
  const rows = table.querySelectorAll('li');
  const fileList = [];
  for (let j = 0; j < rows.length; ++j) {
    const row = rows[j];
    fileList.push([
      row.querySelector('.filename-label').textContent,
      row.querySelector('.size').textContent,
      row.querySelector('.type').textContent,
      sanitizeDate(row.querySelector('.date').textContent || ''),
    ]);
  }
  return fileList;
};

/**
 * Returns the name of the files currently selected in the file list. Note the
 * routine has the same 'visible files' limitation as getFileList() above.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<string>} Selected file names.
 */
test.util.sync.getSelectedFiles = contentWindow => {
  const table = contentWindow.document.querySelector('#detail-table');
  const rows = table.querySelectorAll('li');
  const selected = [];
  for (let i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected')) {
      selected.push(rows[i].querySelector('.filename-label').textContent);
    }
  }
  return selected;
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.sync.selectFile = (contentWindow, filename) => {
  const rows = contentWindow.document.querySelectorAll('#detail-table li');
  test.util.sync.focus(contentWindow, '#file-list');
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'Home', false, false, false);
  for (let index = 0; index < rows.length; ++index) {
    const selection = test.util.sync.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename) {
      return true;
    }
    test.util.sync.fakeKeyDown(
        contentWindow, '#file-list', 'ArrowDown', false, false, false);
  }
  console.warn('Failed to select file "' + filename + '"');
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
test.util.sync.openFile = (contentWindow, filename) => {
  const query = '#file-list li.table-row[selected] .filename-label span';
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
test.util.async.selectVolume = (contentWindow, iconName, callback) => {
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
test.util.async.selectInDirectoryTree =
    (contentWindow, query, isDriveSubVolume, callback) => {
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
          // To change the selected volume, we have to send both events
          // 'mousedown' and 'click' to the navigation list.
          callback(
              test.util.sync.fakeMouseDown(contentWindow, query) &&
              test.util.sync.fakeMouseClick(contentWindow, query));
        },
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
test.util.sync.selectFolderInTree = (contentWindow, folderName) => {
  const items =
      contentWindow.document.querySelectorAll('#directory-tree .tree-item');
  test.util.sync.fakeKeyDown(
      contentWindow, '#directory-tree', 'Home', false, false, false);
  for (let index = 0; index < items.length; ++index) {
    const selectedTreeItemName =
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
test.util.sync.expandSelectedFolderInTree = contentWindow => {
  const selectedItem = contentWindow.document.querySelector(
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
test.util.sync.collapseSelectedFolderInTree = contentWindow => {
  const selectedItem = contentWindow.document.querySelector(
      '#directory-tree .tree-item[selected]');
  if (!selectedItem) {
    console.error('Unexpected; no tree item currently selected.');
    return false;
  }
  test.util.sync.fakeKeyDown(
      contentWindow, '#directory-tree .tree-item[selected]', 'ArrowLeft', false,
      false, false);
  return true;
};

/**
 * Obtains visible tree items.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {!Array<string>} List of visible item names.
 */
test.util.sync.getTreeItems = contentWindow => {
  const items =
      contentWindow.document.querySelectorAll('#directory-tree .tree-item');
  const result = [];
  for (let i = 0; i < items.length; i++) {
    if (items[i].matches('.tree-children:not([expanded]) *')) {
      continue;
    }
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
test.util.sync.getLastVisitedURL = contentWindow => {
  return contentWindow.fileManager.getLastVisitedURL();
};

/**
 * Returns a string translation from its translation ID.
 * @param {string} id The id of the translated string.
 * @return {string}
 */
test.util.sync.getTranslatedString = (contentWindow, id) => {
  return contentWindow.fileManager.getTranslatedString(id);
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
test.util.async.executeScriptInWebView =
    (contentWindow, webViewQuery, code, callback) => {
      const webView = contentWindow.document.querySelector(webViewQuery);
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
test.util.sync.copyFile = (contentWindow, filename) => {
  if (!test.util.sync.selectFile(contentWindow, filename)) {
    return false;
  }
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
test.util.sync.deleteFile = (contentWindow, filename) => {
  if (!test.util.sync.selectFile(contentWindow, filename)) {
    return false;
  }
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
test.util.sync.execCommand = (contentWindow, command) => {
  const ret = contentWindow.document.execCommand(command);
  if (!ret) {
    // TODO(b/191831968): Fix execCommand for SWA.
    console.warn(
        `execCommand(${command}) returned false for SWA, forcing ` +
        `return value to true. b/191831968`);
    return true;
  }
  return ret;
};

/**
 * Override the task-related methods in private api for test.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<Object>} taskList List of tasks to be returned in
 *     fileManagerPrivate.getFileTasks().
 * @param {boolean}
 *     isPolicyDefault Whether the default is set by policy.
 * @return {boolean} Always return true.
 */
test.util.sync
    .overrideTasks = (contentWindow, taskList, isPolicyDefault = false) => {
  const getFileTasks = (entries, onTasks) => {
    // Call onTask asynchronously (same with original getFileTasks).
    setTimeout(() => {
      const policyDefaultHandlerStatus = isPolicyDefault ?
          chrome.fileManagerPrivate.PolicyDefaultHandlerStatus
              .DEFAULT_HANDLER_ASSIGNED_BY_POLICY :
          undefined;
      onTasks({tasks: taskList, policyDefaultHandlerStatus});
    }, 0);
  };

  const executeTask = (descriptor, entries, callback) => {
    test.util.executedTasks_.push({descriptor, entries, callback});
  };

  const setDefaultTask = descriptor => {
    for (let i = 0; i < taskList.length; i++) {
      taskList[i].isDefault =
          util.descriptorEqual(taskList[i].descriptor, descriptor);
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
 * @return {Array<!{{descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
 *     fileNames: !Array<string>}}>} List of executed tasks.
 */
test.util.sync.getExecutedTasks = contentWindow => {
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    return null;
  }
  return test.util.executedTasks_.map(task => {
    return {
      descriptor: task.descriptor,
      fileNames: task.entries.map(e => e.name),
    };
  });
};

/**
 * Obtains the list of executed tasks.
 * @param {Window} contentWindow Window to be tested.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor the task to
 *     check.
 * @param {!Array<string>} fileNames Name of files that should have been passed
 *     to the executeTasks().
 * @return {boolean} True if the task was executed.
 */
test.util.sync.taskWasExecuted = (contentWindow, descriptor, fileNames) => {
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    return null;
  }
  const task = test.util.executedTasks_.find(
      task => util.descriptorEqual(task.descriptor, descriptor));
  if (!task) {
    return false;
  }

  return JSON.stringify(fileNames) ===
      JSON.stringify(task.entries.map(e => e.name));
};

/**
 * Invokes an executed task with |responseArgs|.
 * @param {Window} contentWindow Window to be tested.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor the task to
 *     be replied to.
 * @param {Array<Object>} responseArgs the arguments to inoke the callback with.
 */
test.util.sync.replyExecutedTask =
    (contentWindow, descriptor, responseArgs) => {
      if (!test.util.executedTasks_) {
        console.error('Please call overrideTasks() first.');
        return false;
      }
      const found = test.util.executedTasks_.find(
          task => util.descriptorEqual(task.descriptor, descriptor));
      if (!found) {
        const {appId, taskType, actionId} = descriptor;
        console.error(`No task with id ${appId}|${taskType}|${actionId}`);
        return false;
      }
      found.callback(...responseArgs);
      return true;
    };

/**
 * Calls the unload handler for the window.
 * @param {Window} contentWindow Window to be tested.
 */
test.util.sync.unload = contentWindow => {
  contentWindow.fileManager.onUnload_();
};

/**
 * Returns the path shown in the breadcrumb.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {string} The breadcrumb path.
 */
test.util.sync.getBreadcrumbPath = contentWindow => {
  const doc = contentWindow.document;
  const breadcrumb = doc.querySelector('#location-breadcrumbs xf-breadcrumb');

  if (!breadcrumb) {
    return '';
  }

  return '/' + breadcrumb.path;
};

/**
 * Obtains the preferences.
 * @param {function(Object)} callback Callback function with results returned by
 *     the script.
 */
test.util.async.getPreferences = callback => {
  chrome.fileManagerPrivate.getPreferences(callback);
};

/**
 * Stubs out the formatVolume() function in fileManagerPrivate.
 *
 * @param {Window} contentWindow Window to be affected.
 */
test.util.sync.overrideFormat = contentWindow => {
  contentWindow.chrome.fileManagerPrivate.formatVolume =
      (volumeId, filesystem, volumeLabel) => {};
  return true;
};

/**
 * Run a contentWindow.requestAnimationFrame() cycle and resolve the callback
 * when that requestAnimationFrame completes.
 * @param {Window} contentWindow Window to be tested.
 * @param {function(boolean)} callback Completion callback.
 */
test.util.async.requestAnimationFrame = (contentWindow, callback) => {
  contentWindow.requestAnimationFrame(() => {
    callback(true);
  });
};

/**
 * Set the window text direction to RTL and wait for the window to redraw.
 * @param {Window} contentWindow Window to be tested.
 * @param {function(boolean)} callback Completion callback.
 */
test.util.async.renderWindowTextDirectionRTL = (contentWindow, callback) => {
  contentWindow.document.documentElement.setAttribute('dir', 'rtl');
  contentWindow.document.body.setAttribute('dir', 'rtl');
  contentWindow.requestAnimationFrame(() => {
    callback(true);
  });
};

/**
 * Maps the path to the replaced attribute to the PrepareFake instance that
 * replaced it, to be able to restore the original value.
 *
 * @private {Object<string, test.util.PrepareFake>}
 */
test.util.backgroundReplacedObjects_ = {};

/**
 * Map the appId to a map of all fakes applied in the foreground window e.g.:
 *  {'files#0': {'chrome.bla.api': FAKE}
 *
 * @private {Object<string, Object<string, test.util.PrepareFake>>}
 */
test.util.foregroundReplacedObjects_ = {};

/**
 * @param {string} attrName
 * @param {*} staticValue
 * @return {function(...)}
 */
test.util.staticFakeFactory = (attrName, staticValue) => {
  const fake = (...args) => {
    setTimeout(() => {
      // Find the first callback.
      for (const arg of args) {
        if (typeof arg === 'function') {
          console.warn(`staticFake for ${attrName} value: ${staticValue}`);
          return arg(staticValue);
        }
      }
      throw new Error(`Couldn't find callback for ${attrName}`);
    }, 0);
  };
  return fake;
};

/**
 * Registry of available fakes, it maps the an string ID to a factory function
 * which returns the actual fake used to replace an implementation.
 *
 * @private {Object<string, function(string, *)>}
 */
test.util.fakes_ = {
  'static_fake': test.util.staticFakeFactory,
};

/**
 * @enum {string}
 */
test.util.FakeType = {
  FOREGROUND_FAKE: 'FOREGROUND_FAKE',
  BACKGROUND_FAKE: 'BACKGROUND_FAKE',
};

/**
 * Class holds the information for applying and restoring fakes.
 */
test.util.PrepareFake = class {
  /**
   * @param {string} attrName Name of the attribute to be replaced by the fake
   *   e.g.: "chrome.app.window.create".
   * @param {string} fakeId The name of the fake to be used from
   *   test.util.fakes_.
   * @param {*} context The context where the attribute will be traversed from,
   *   e.g.: Window object.
   * @param {...} args Additinal args provided from the integration test to the
   *   fake, e.g.: static return value.
   */
  constructor(attrName, fakeId, context, ...args) {
    /**
     * The instance of the fake to be used, ready to be used.
     * @private {*}
     */
    this.fake_ = null;

    /**
     * The attribute name to be traversed in the |context_|.
     * @private {string}
     */
    this.attrName_ = attrName;

    /**
     * The fake id the key to retrieve from test.util.fakes_.
     * @private {string}
     */
    this.fakeId_ = fakeId;

    /**
     * The context where |attrName_| will be traversed from, e.g. Window.
     * @private {*}
     */
    this.context_ = context;

    /**
     * After traversing |context_| the object that holds the attribute to be
     * replaced by the fake.
     * @private {*}
     */
    this.parentObject_ = null;

    /**
     * After traversing |context_| the attribute name in |parentObject_| that
     * will be replaced by the fake.
     * @private {string}
     */
    this.leafAttrName_ = '';

    /**
     * Additional data provided from integration tests to the fake constructor.
     * @private {!Array}
     */
    this.args_ = args;

    /**
     * Original object that was replaced by the fake.
     * @private {*}
     */
    this.original_ = null;

    /**
     * If this fake object has been constructed and everything initialized.
     * @private {boolean}
     */
    this.prepared_ = false;

    /**
     * Counter to record the number of times the static fake is called.
     * @private {number}
     */
    this.callCounter_ = 0;

    /**
     * List to record the arguments provided to the static fake calls.
     * @private {!Array}
     */
    this.calledArgs_ = [];
  }

  /**
   * Initializes the fake and traverse |context_| to be ready to replace the
   * original implementation with the fake.
   */
  prepare() {
    this.buildFake_();
    this.traverseContext_();
    this.prepared_ = true;
  }

  /**
   * Replaces the original implementation with the fake.
   * NOTE: It requires prepare() to have been called.
   * @param {test.util.FakeType} fakeType Foreground or background fake.
   * @param {Window} contentWindow Window to be tested.
   */
  replace(fakeType, contentWindow) {
    const suffix = `for ${this.attrName_} ${this.fakeId_}`;
    if (!this.prepared_) {
      throw new Error(`PrepareFake prepare() not called ${suffix}`);
    }
    if (!this.parentObject_) {
      throw new Error(`Missing parentObject_ ${suffix}`);
    }
    if (!this.fake_) {
      throw new Error(`Missing fake_ ${suffix}`);
    }
    if (!this.leafAttrName_) {
      throw new Error(`Missing leafAttrName_ ${suffix}`);
    }

    this.saveOriginal_(fakeType, contentWindow);
    this.parentObject_[this.leafAttrName_] = (...args) => {
      this.fake_(...args);
      this.callCounter_++;
      this.calledArgs_.push([...args]);
    };
  }

  /**
   * Restores the original implementation that had been rpeviously replaced by
   * the fake.
   */
  restore() {
    if (!this.original_) {
      return;
    }
    this.parentObject_[this.leafAttrName_] = this.original_;
    this.original_ = null;
  }

  /**
   * Saves the original implementation to be able restore it later.
   * @param {test.util.FakeType} fakeType Foreground or background fake.
   * @param {Window} contentWindow Window to be tested.
   */
  saveOriginal_(fakeType, contentWindow) {
    if (fakeType === test.util.FakeType.FOREGROUND_FAKE) {
      const windowFakes =
          test.util.foregroundReplacedObjects_[contentWindow.appID] || {};
      test.util.foregroundReplacedObjects_[contentWindow.appID] = windowFakes;

      // Only save once, otherwise it can save an object that is already fake.
      if (!windowFakes[this.attrName_]) {
        const original = this.parentObject_[this.leafAttrName_];
        this.original_ = original;
        windowFakes[this.attrName_] = this;
      }
      return;
    }

    if (fakeType === test.util.FakeType.BACKGROUND_FAKE) {
      // Only save once, otherwise it can save an object that is already fake.
      if (!test.util.backgroundReplacedObjects_[this.attrName_]) {
        const original = this.parentObject_[this.leafAttrName_];
        this.original_ = original;
        test.util.backgroundReplacedObjects_[this.attrName_] = this;
      }
    }
  }

  /**
   * Constructs the fake.
   */
  buildFake_() {
    const factory = test.util.fakes_[this.fakeId_];
    if (!factory) {
      throw new Error(`Failed to find the fake factory for ${this.fakeId_}`);
    }

    this.fake_ = factory(this.attrName_, ...this.args_);
  }

  /**
   * Finds the parent and the object to be replaced by fake.
   */
  traverseContext_() {
    let target = this.context_;
    let parentObj;
    let attr = '';

    for (const a of this.attrName_.split('.')) {
      attr = a;
      parentObj = target;
      target = target[a];

      if (target === undefined) {
        throw new Error(`Couldn't find "${0}" from "${this.attrName_}"`);
      }
    }

    this.parentObject_ = parentObj;
    this.leafAttrName_ = attr;
  }
};

/**
 * Replaces implementations in the background page with fakes.
 *
 * @param {Object{<string, Array>}} fakeData An object mapping the path to the
 * object to be replaced and the value is the Array with fake id and additinal
 * arguments for the fake constructor, e.g.:
 *   fakeData = {
 *     'chrome.app.window.create' : [
 *       'static_fake',
 *       ['some static value', 'other arg'],
 *     ]
 *   }
 *
 *  This will replace the API 'chrome.app.window.create' with a static fake,
 *  providing the additional data to static fake: ['some static value', 'other
 *  value'].
 */
test.util.sync.backgroundFake = (fakeData) => {
  for (const [path, mockValue] of Object.entries(fakeData)) {
    const fakeId = mockValue[0];
    const fakeArgs = mockValue[1] || [];

    const fake = new test.util.PrepareFake(path, fakeId, window, ...fakeArgs);
    fake.prepare();
    fake.replace(test.util.FakeType.BACKGROUND_FAKE, window);
  }
};

/**
 * Removes all fakes that were applied to the background page.
 */
test.util.sync.removeAllBackgroundFakes = () => {
  const savedFakes = Object.entries(test.util.backgroundReplacedObjects_);
  let removedCount = 0;
  for (const [path, fake] of savedFakes) {
    fake.restore();
    removedCount++;
  }

  return removedCount;
};

/**
 * Replaces implementations in the foreground page with fakes.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Object{<string, Array>}} fakeData An object mapping the path to the
 * object to be replaced and the value is the Array with fake id and additinal
 * arguments for the fake constructor, e.g.:
 *   fakeData = {
 *     'chrome.app.window.create' : [
 *       'static_fake',
 *       ['some static value', 'other arg'],
 *     ]
 *   }
 *
 *  This will replace the API 'chrome.app.window.create' with a static fake,
 *  providing the additional data to static fake: ['some static value', 'other
 *  value'].
 */
test.util.sync.foregroundFake = (contentWindow, fakeData) => {
  const entries = Object.entries(fakeData);
  for (const [path, mockValue] of entries) {
    const fakeId = mockValue[0];
    const fakeArgs = mockValue[1] || [];
    const fake =
        new test.util.PrepareFake(path, fakeId, contentWindow, ...fakeArgs);
    fake.prepare();
    fake.replace(test.util.FakeType.FOREGROUND_FAKE, contentWindow);
  }
  return entries.length;
};

/**
 * Removes all fakes that were applied to the foreground page.
 * @param {Window} contentWindow Window to be tested.
 */
test.util.sync.removeAllForegroundFakes = (contentWindow) => {
  const savedFakes =
      Object.entries(test.util.foregroundReplacedObjects_[contentWindow.appID]);
  let removedCount = 0;
  for (const [path, fake] of savedFakes) {
    fake.restore();
    removedCount++;
  }

  return removedCount;
};

/**
 * Obtains the number of times the static fake api is called.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} fakedApi Path of the method that is faked.
 * @return {number} Number of times the fake api called.
 */
test.util.sync.staticFakeCounter = (contentWindow, fakedApi) => {
  const fake =
      test.util.foregroundReplacedObjects_[contentWindow.appID][fakedApi];
  return fake.callCounter_;
};

/**
 * Obtains the list of arguments with which the static fake api was called.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} fakedApi Path of the method that is faked.
 * @param {!Array<!Array<*>>} An array with all calls to this fake, each item is
 *     an array with all args passed in when the fake was called.
 */
test.util.sync.staticFakeCalledArgs = (contentWindow, fakedApi) => {
  const fake =
      test.util.foregroundReplacedObjects_[contentWindow.appID][fakedApi];
  return fake.calledArgs_;
};

/**
 * Send progress item to Foreground page to display.
 * @param {string} id Progress item id.
 * @param {ProgressItemType} type Type of progress item.
 * @param {ProgressItemState} state State of the progress item.
 * @param {string} message Message of the progress item.
 * @param {number} remainingTime The remaining time of the progress in second.
 * @param {number} progressMax Max value of the progress.
 * @param {number} progressValue Current value of the progress.
 * @param {number} count Number of items being processed.
 */
test.util.sync.sendProgressItem =
    (id, type, state, message, remainingTime, progressMax = 1,
     progressValue = 0, count = 1) => {
      const item = new ProgressCenterItem();
      item.id = id;
      item.type = type;
      item.state = state;
      item.message = message;
      item.remainingTime = remainingTime;
      item.progressMax = progressMax;
      item.progressValue = progressValue;
      item.itemCount = count;

      background.progressCenter.updateItem(item);
      return true;
    };
