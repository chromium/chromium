// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {ScriptLoader} from '../../common/js/script_loader.js';
import {descriptorEqual} from '../../common/js/util.js';

// @ts-ignore: error TS2440: Import declaration conflicts with local declaration
// of 'test'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.getFileList = contentWindow => {
  const table = contentWindow.document.querySelector('#detail-table');
  // @ts-ignore: error TS18047: 'table' is possibly 'null'.
  const rows = table.querySelectorAll('li');
  const fileList = [];
  for (let j = 0; j < rows.length; ++j) {
    const row = rows[j];
    fileList.push([
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      row.querySelector('.filename-label').textContent,
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      row.querySelector('.size').textContent,
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      row.querySelector('.type').textContent,
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      sanitizeDate(row.querySelector('.date').textContent || ''),
    ]);
  }
  // @ts-ignore: error TS2322: Type '(string | null)[][]' is not assignable to
  // type 'string[][]'.
  return fileList;
};

/**
 * Returns the name of the files currently selected in the file list. Note the
 * routine has the same 'visible files' limitation as getFileList() above.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array<string>} Selected file names.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.getSelectedFiles = contentWindow => {
  const table = contentWindow.document.querySelector('#detail-table');
  // @ts-ignore: error TS18047: 'table' is possibly 'null'.
  const rows = table.querySelectorAll('li');
  const selected = [];
  for (let i = 0; i < rows.length; ++i) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (rows[i].hasAttribute('selected')) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      selected.push(rows[i].querySelector('.filename-label').textContent);
    }
  }
  // @ts-ignore: error TS2322: Type '(string | null)[]' is not assignable to
  // type 'string[]'.
  return selected;
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.selectFile = (contentWindow, filename) => {
  const rows = contentWindow.document.querySelectorAll('#detail-table li');
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  test.util.sync.focus(contentWindow, '#file-list');
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'Home', false, false, false);
  for (let index = 0; index < rows.length; ++index) {
    // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
    // util'.
    const selection = test.util.sync.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename) {
      return true;
    }
    // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
    // util'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.openFile = (contentWindow, filename) => {
  const query = '#file-list li.table-row[selected] .filename-label span';
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  return test.util.sync.selectFile(contentWindow, filename) &&
      // @ts-ignore: error TS2339: Property 'sync' does not exist on type
      // 'typeof util'.
      test.util.sync.fakeMouseDoubleClick(contentWindow, query);
};

/**
 * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
 *
 * @param {Window} contentWindow The window where visitURL() was called.
 * @return {!string} The URL of the last URL visited.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.getLastVisitedURL = contentWindow => {
  // @ts-ignore: error TS2339: Property 'getLastVisitedURL' does not exist on
  // type 'FileManager'.
  return contentWindow.fileManager.getLastVisitedURL();
};

/**
 * Returns a string translation from its translation ID.
 * @param {string} id The id of the translated string.
 * @return {string}
 */
// @ts-ignore: error TS7006: Parameter 'contentWindow' implicitly has an 'any'
// type.
test.util.sync.getTranslatedString = (contentWindow, id) => {
  return contentWindow.fileManager.getTranslatedString(id);
};

/**
 * Executes Javascript code on a webview and returns the result.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} webViewQuery Selector for the web view.
 * @param {string} code Javascript code to be executed within the web view.
 * @param {function(*):void} callback Callback function with results returned by
the
 *     script.
// @ts-ignore: error TS7014: Function type, which lacks return-type annotation,
implicitly has an 'any' return type.
 */
// @ts-ignore: error TS2339: Property 'async' does not exist on type 'typeof
// util'.
test.util.async.executeScriptInWebView =
    (contentWindow, webViewQuery, code, callback) => {
      const webView = contentWindow.document.querySelector(webViewQuery);
      // @ts-ignore: error TS2339: Property 'executeScript' does not exist on
      // type 'Element'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.copyFile = (contentWindow, filename) => {
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  if (!test.util.sync.selectFile(contentWindow, filename)) {
    return false;
  }
  // Ctrl+C and Ctrl+V
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  test.util.sync.fakeKeyDown(
      contentWindow, '#file-list', 'c', true, false, false);
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.deleteFile = (contentWindow, filename) => {
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
  if (!test.util.sync.selectFile(contentWindow, filename)) {
    return false;
  }
  // Delete
  // @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
  // util'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync
    .overrideTasks = (contentWindow, taskList, isPolicyDefault = false) => {
  // @ts-ignore: error TS7006: Parameter 'onTasks' implicitly has an 'any' type.
  const getFileTasks = (entries, sourceUrls, onTasks) => {
    // Call onTask asynchronously (same with original getFileTasks).
    setTimeout(() => {
      const policyDefaultHandlerStatus = isPolicyDefault ?
          chrome.fileManagerPrivate.PolicyDefaultHandlerStatus
              .DEFAULT_HANDLER_ASSIGNED_BY_POLICY :
          undefined;
      onTasks({tasks: taskList, policyDefaultHandlerStatus});
    }, 0);
  };

  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  const executeTask = (descriptor, entries, callback) => {
    // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on
    // type 'typeof util'.
    test.util.executedTasks_.push({descriptor, entries, callback});
  };

  // @ts-ignore: error TS7006: Parameter 'descriptor' implicitly has an 'any'
  // type.
  const setDefaultTask = descriptor => {
    for (let i = 0; i < taskList.length; i++) {
      // @ts-ignore: error TS2339: Property 'isDefault' does not exist on type
      // 'Object'.
      taskList[i].isDefault =
          // @ts-ignore: error TS2339: Property 'descriptor' does not exist on
          // type 'Object'.
          descriptorEqual(taskList[i].descriptor, descriptor);
    }
  };

  // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on type
  // 'typeof util'.
  test.util.executedTasks_ = [];
  // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
  // 'Window'.
  contentWindow.chrome.fileManagerPrivate.getFileTasks = getFileTasks;
  // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
  // 'Window'.
  contentWindow.chrome.fileManagerPrivate.executeTask = executeTask;
  // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
  // 'Window'.
  contentWindow.chrome.fileManagerPrivate.setDefaultTask = setDefaultTask;
  return true;
};

/**
 * Obtains the list of executed tasks.
 * @param {Window} contentWindow Window to be tested.
// @ts-ignore: error TS1131: Property or signature expected.
 * @return {Array<!{descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
 *     fileNames: !Array<string>}>} List of executed tasks.
// @ts-ignore: error TS1131: Property or signature expected.
 */
// @ts-ignore: error TS6133: 'contentWindow' is declared but its value is never
// read.
test.util.sync.getExecutedTasks = contentWindow => {
  // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on type
  // 'typeof util'.
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type '{}[]'.
    return null;
  }
  // @ts-ignore: error TS7006: Parameter 'task' implicitly has an 'any' type.
  return test.util.executedTasks_.map(task => {
    return {
      descriptor: task.descriptor,
      // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
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
// @ts-ignore: error TS6133: 'contentWindow' is declared but its value is never
// read.
test.util.sync.taskWasExecuted = (contentWindow, descriptor, fileNames) => {
  // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on type
  // 'typeof util'.
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'boolean'.
    return null;
  }
  const fileNamesStr = JSON.stringify(fileNames);
  // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on type
  // 'typeof util'.
  const task = test.util.executedTasks_.find(
      // @ts-ignore: error TS7006: Parameter 'task' implicitly has an 'any'
      // type.
      task => descriptorEqual(task.descriptor, descriptor) &&
          // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any'
          // type.
          fileNamesStr === JSON.stringify(task.entries.map(e => e.name)));
  return task !== undefined;
};

/**
 * Invokes an executed task with |responseArgs|.
 * @param {Window} contentWindow Window to be tested.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor the task to
 *     be replied to.
 * @param {Array<Object>} responseArgs the arguments to inoke the callback with.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.replyExecutedTask =
    // @ts-ignore: error TS6133: 'contentWindow' is declared but its value is
    // never read.
    (contentWindow, descriptor, responseArgs) => {
      // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on
      // type 'typeof util'.
      if (!test.util.executedTasks_) {
        console.error('Please call overrideTasks() first.');
        return false;
      }
      // @ts-ignore: error TS2339: Property 'executedTasks_' does not exist on
      // type 'typeof util'.
      const found = test.util.executedTasks_.find(
          // @ts-ignore: error TS7006: Parameter 'task' implicitly has an 'any'
          // type.
          task => descriptorEqual(task.descriptor, descriptor));
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.unload = contentWindow => {
  // @ts-ignore: error TS2339: Property 'onUnload_' does not exist on type
  // 'FileManager'.
  contentWindow.fileManager.onUnload_();
};

/**
 * Returns the path shown in the breadcrumb.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {string} The breadcrumb path.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.getBreadcrumbPath = contentWindow => {
  const doc = contentWindow.document;
  const breadcrumb = doc.querySelector('#location-breadcrumbs xf-breadcrumb');

  if (!breadcrumb) {
    return '';
  }

  // @ts-ignore: error TS2339: Property 'path' does not exist on type 'Element'.
  return '/' + breadcrumb.path;
};

/**
 * Obtains the preferences.
 * @param {function(Object):void} callback Callback function with results
 *     returned by the script.
 */
// @ts-ignore: error TS2339: Property 'async' does not exist on type 'typeof
// util'.
test.util.async.getPreferences = callback => {
  chrome.fileManagerPrivate.getPreferences(callback);
};

/**
 * Stubs out the formatVolume() function in fileManagerPrivate.
 *
 * @param {Window} contentWindow Window to be affected.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.overrideFormat = contentWindow => {
  // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
  // 'Window'.
  contentWindow.chrome.fileManagerPrivate.formatVolume =
      // @ts-ignore: error TS7006: Parameter 'volumeLabel' implicitly has an
      // 'any' type.
      (volumeId, filesystem, volumeLabel) => {};
  return true;
};

/**
 * Run a contentWindow.requestAnimationFrame() cycle and resolve the callback
 * when that requestAnimationFrame completes.
 * @param {Window} contentWindow Window to be tested.
 * @param {function(boolean):void} callback Completion callback.
 */
// @ts-ignore: error TS2339: Property 'async' does not exist on type 'typeof
// util'.
test.util.async.requestAnimationFrame = (contentWindow, callback) => {
  // @ts-ignore: error TS7014: Function type, which lacks return-type
  // annotation, implicitly has an 'any' return type.
  contentWindow.requestAnimationFrame(() => {
    callback(true);
  });
};

/**
 * Set the window text direction to RTL and wait for the window to redraw.
 * @param {Window} contentWindow Window to be tested.
 * @param {function(boolean):void} callback Completion callback.
 */
// @ts-ignore: error TS2339: Property 'async' does not exist on type 'typeof
// util'.
test.util.async.renderWindowTextDirectionRTL = (contentWindow, callback) => {
  contentWindow.document.documentElement.setAttribute('dir', 'rtl');
  // @ts-ignore: error TS7014: Function type, which lacks return-type
  // annotation, implicitly has an 'any' return type.
  contentWindow.document.body.setAttribute('dir', 'rtl');
  contentWindow.requestAnimationFrame(() => {
    callback(true);
  });
};

/**
 * Maps the path to the replaced attribute to the PrepareFake instance that
 * replaced it, to be able to restore the original value.
 *
 * @private @type {Record<string, PrepareFake>}
 */
// @ts-ignore: error TS2339: Property 'backgroundReplacedObjects_' does not
// exist on type 'typeof util'.
test.util.backgroundReplacedObjects_ = {};

/**
 * Map the appId to a map of all fakes applied in the foreground window e.g.:
 *  {'files#0': {'chrome.bla.api': FAKE}
 *
 * @private @type {Record<string, Object<string, PrepareFake>>}
 */
// @ts-ignore: error TS2339: Property 'foregroundReplacedObjects_' does not
// exist on type 'typeof util'.
test.util.foregroundReplacedObjects_ = {};

/**
 * @param {string} attrName
 * @param {*} staticValue
 * @return {function(...*)}
 */
// @ts-ignore: error TS2339: Property 'staticFakeFactory' does not exist on type
// 'typeof util'.
test.util.staticFakeFactory = (attrName, staticValue) => {
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  const fake = (...args) => {
    // @ts-ignore: error TS1110: Type expected.
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
 * @private @type {Record<string, function(string, *):void>}
 */
// @ts-ignore: error TS2339: Property 'fakes_' does not exist on type 'typeof
// util'.
test.util.fakes_ = {
  // @ts-ignore: error TS2339: Property 'staticFakeFactory' does not exist on
  // type 'typeof util'.
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
class PrepareFake {
  /**
   * @param {string} attrName Name of the attribute to be replaced by the fake
   *   e.g.: "chrome.app.window.create".
   * @param {string} fakeId The name of the fake to be used from
   *   test.util.fakes_.
   * @param {*} context The context where the attribute will be traversed from,
   *   e.g.: Window object.
   * @param {...*} args Additional args provided from the integration test to
   *     the
   *   fake, e.g.: static return value.
   */
  constructor(attrName, fakeId, context, ...args) {
    /**
     * The instance of the fake to be used, ready to be used.
     * @private @type {*}
     */
    this.fake_ = null;

    /**
     * The attribute name to be traversed in the |context_|.
     * @private @type {string}
     */
    this.attrName_ = attrName;

    /**
     * The fake id the key to retrieve from test.util.fakes_.
     * @private @type {string}
     */
    this.fakeId_ = fakeId;

    /**
     * The context where |attrName_| will be traversed from, e.g. Window.
     * @private @type {*}
     */
    this.context_ = context;

    /**
     * After traversing |context_| the object that holds the attribute to be
     * replaced by the fake.
     * @private @type {*}
     */
    this.parentObject_ = null;

    /**
     * After traversing |context_| the attribute name in |parentObject_| that
     * will be replaced by the fake.
     * @private @type {string}
     */
    this.leafAttrName_ = '';

    /**
     * Additional data provided from integration tests to the fake constructor.
     * @private @type {!Array<*>}
     */
    this.args_ = args;

    /**
     * Original object that was replaced by the fake.
     * @private @type {*}
     */
    this.original_ = null;

    /**
     * If this fake object has been constructed and everything initialized.
     * @private @type {boolean}
     */
    this.prepared_ = false;

    /**
     * Counter to record the number of times the static fake is called.
     * @private @type {number}
     */
    this.callCounter_ = 0;

    /**
     * List to record the arguments provided to the static fake calls.
     * @private @type {!Array<*>}
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
    // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
    // type.
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
    // @ts-ignore: error TS2339: Property 'FOREGROUND_FAKE' does not exist on
    // type 'typeof FakeType'.
    if (fakeType === test.util.FakeType.FOREGROUND_FAKE) {
      const windowFakes =
          // @ts-ignore: error TS2339: Property 'appID' does not exist on type
          // 'Window'.
          test.util.foregroundReplacedObjects_[contentWindow.appID] || {};
      // @ts-ignore: error TS2339: Property 'appID' does not exist on type
      // 'Window'.
      test.util.foregroundReplacedObjects_[contentWindow.appID] = windowFakes;

      // Only save once, otherwise it can save an object that is already fake.
      if (!windowFakes[this.attrName_]) {
        const original = this.parentObject_[this.leafAttrName_];
        this.original_ = original;
        windowFakes[this.attrName_] = this;
      }
      return;
    }

    // @ts-ignore: error TS2339: Property 'BACKGROUND_FAKE' does not exist on
    // type 'typeof FakeType'.
    if (fakeType === test.util.FakeType.BACKGROUND_FAKE) {
      // Only save once, otherwise it can save an object that is already fake.
      // @ts-ignore: error TS2339: Property 'backgroundReplacedObjects_' does
      // not exist on type 'typeof util'.
      if (!test.util.backgroundReplacedObjects_[this.attrName_]) {
        const original = this.parentObject_[this.leafAttrName_];
        this.original_ = original;
        // @ts-ignore: error TS2339: Property 'backgroundReplacedObjects_' does
        // not exist on type 'typeof util'.
        test.util.backgroundReplacedObjects_[this.attrName_] = this;
      }
    }
  }

  /**
   * Constructs the fake.
   */
  buildFake_() {
    // @ts-ignore: error TS2339: Property 'fakes_' does not exist on type
    // 'typeof util'.
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
}

// @ts-ignore: error TS2339: Property 'PrepareFake' does not exist on type
// 'typeof util'.
test.util.PrepareFake = PrepareFake;

/**
 * Replaces implementations in the background page with fakes.
 *
 * @param {Record<string, Array<*>>} fakeData An object mapping the path to the
 * object to be replaced and the value is the Array with fake id and additional
 * arguments for the fake constructor, e.g.:
 *   fakeData = {
 *     'chrome.app.window.create' : [
 *       'static_fake',
 *       ['some static value', 'other arg'],
// @ts-ignore: error TS1005: '}' expected.
 *     ]
 *   }
 *
 *  This will replace the API 'chrome.app.window.create' with a static fake,
 *  providing the additional data to static fake: ['some static value', 'other
 *  value'].
 */
// @ts-ignore: error TS7006: Parameter 'fakeData' implicitly has an 'any' type.
test.util.sync.backgroundFake = (fakeData) => {
  for (const [path, mockValue] of Object.entries(fakeData)) {
    const fakeId = mockValue[0];
    const fakeArgs = mockValue[1] || [];

    const fake = new PrepareFake(path, fakeId, window, ...fakeArgs);
    fake.prepare();
    // @ts-ignore: error TS2339: Property 'BACKGROUND_FAKE' does not exist on
    // type 'typeof FakeType'.
    fake.replace(test.util.FakeType.BACKGROUND_FAKE, window);
  }
};

/**
 * Removes all fakes that were applied to the background page.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.removeAllBackgroundFakes = () => {
  // @ts-ignore: error TS2339: Property 'backgroundReplacedObjects_' does not
  // exist on type 'typeof util'.
  const savedFakes = Object.entries(test.util.backgroundReplacedObjects_);
  let removedCount = 0;
  // @ts-ignore: error TS6133: 'path' is declared but its value is never read.
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
 * @param {Record<string, Array<*>>} fakeData An object mapping the path to the
 * object to be replaced and the value is the Array with fake id and additional
 * arguments for the fake constructor, e.g.:
 *   fakeData = {
 *     'chrome.app.window.create' : [
 *       'static_fake',
 *       ['some static value', 'other arg'],
 *     ]
// @ts-ignore: error TS1005: '}' expected.
 *   }
 *
 *  This will replace the API 'chrome.app.window.create' with a static fake,
 *  providing the additional data to static fake: ['some static value', 'other
 *  value'].
 */
// @ts-ignore: error TS7006: Parameter 'fakeData' implicitly has an 'any' type.
test.util.sync.foregroundFake = (contentWindow, fakeData) => {
  const entries = Object.entries(fakeData);
  for (const [path, mockValue] of entries) {
    const fakeId = mockValue[0];
    const fakeArgs = mockValue[1] || [];
    const fake = new PrepareFake(path, fakeId, contentWindow, ...fakeArgs);
    fake.prepare();
    // @ts-ignore: error TS2339: Property 'FOREGROUND_FAKE' does not exist on
    // type 'typeof FakeType'.
    fake.replace(test.util.FakeType.FOREGROUND_FAKE, contentWindow);
  }
  return entries.length;
};

/**
 * Removes all fakes that were applied to the foreground page.
 * @param {Window} contentWindow Window to be tested.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.removeAllForegroundFakes = (contentWindow) => {
  const savedFakes =
      // @ts-ignore: error TS2339: Property 'appID' does not exist on type
      // 'Window'.
      Object.entries(test.util.foregroundReplacedObjects_[contentWindow.appID]);
  let removedCount = 0;
  // @ts-ignore: error TS6133: 'path' is declared but its value is never read.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.staticFakeCounter = (contentWindow, fakedApi) => {
  const fake =
      // @ts-ignore: error TS2339: Property 'appID' does not exist on type
      // 'Window'.
      test.util.foregroundReplacedObjects_[contentWindow.appID][fakedApi];
  return fake.callCounter_;
};

/**
 * Obtains the list of arguments with which the static fake api was called.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} fakedApi Path of the method that is faked.
 * @return {!Array<!Array<*>>} An array with all calls to this fake, each item
 *     is an array with all args passed in when the fake was called.
 */
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.staticFakeCalledArgs = (contentWindow, fakedApi) => {
  const fake =
      // @ts-ignore: error TS2339: Property 'appID' does not exist on type
      // 'Window'.
      test.util.foregroundReplacedObjects_[contentWindow.appID][fakedApi];
  return fake.calledArgs_;
  // @ts-ignore: error TS8024: JSDoc '@param' tag has name 'An', but there is no
  // parameter with that name.
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
// @ts-ignore: error TS2339: Property 'sync' does not exist on type 'typeof
// util'.
test.util.sync.sendProgressItem =
    // @ts-ignore: error TS2304: Cannot find name 'ProgressItemType'.
    (id, type, state, message, remainingTime, progressMax = 1,
     progressValue = 0, count = 1) => {
      // @ts-ignore: error TS2304: Cannot find name 'ProgressItemState'.
      const item = new ProgressCenterItem();
      item.id = id;
      item.type = type;
      item.state = state;
      item.message = message;
      item.remainingTime = remainingTime;
      item.progressMax = progressMax;
      item.progressValue = progressValue;
      item.itemCount = count;

      // @ts-ignore: error TS2339: Property 'background' does not exist on type
      // 'Window & typeof globalThis'.
      window.background.progressCenter.updateItem(item);
      return true;
    };

/**
 * Remote call API handler. This function handles messages coming from the test
 * harness to execute known functions and return results. This is a dummy
 * implementation that is replaced by a real one once the test harness is fully
 * loaded.
 * @type {function(*, function(*): void): void}
 */
// @ts-ignore: error TS6133: 'callback' is declared but its value is never read.
test.util.executeTestMessage = (request, callback) => {
  throw new Error('executeTestMessage not implemented');
};

/**
 * Handles a direct call from the integration test harness. We execute
 * swaTestMessageListener call directly from the FileManagerBrowserTest.
 * This method avoids enabling external callers to Files SWA. We forward
 * the response back to the caller, as a serialized JSON string.
// @ts-ignore: error TS7014: Function type, which lacks return-type annotation,
implicitly has an 'any' return type.
 * @param {!Object} request
 */
// @ts-ignore: error TS2339: Property 'swaTestMessageListener' does not exist on
// type 'typeof test'.
test.swaTestMessageListener = (request) => {
  // @ts-ignore: error TS2339: Property 'contentWindow' does not exist on type
  // 'Window & typeof globalThis'.
  request.contentWindow = window.contentWindow || window;
  return new Promise(resolve => {
    // @ts-ignore: error TS7006: Parameter 'response' implicitly has an 'any'
    // type.
    test.util.executeTestMessage(request, (response) => {
      response = response === undefined ? '@undefined@' : response;
      resolve(JSON.stringify(response));
    });
  });
};

// @ts-ignore: error TS7034: Variable 'testUtilsLoaded' implicitly has type
// 'any' in some locations where its type cannot be determined.
let testUtilsLoaded = null;

// @ts-ignore: error TS2339: Property 'swaLoadTestUtils' does not exist on type
// 'typeof test'.
test.swaLoadTestUtils = async () => {
  const scriptUrl = 'background/js/runtime_loaded_test_util.js';
  try {
    // @ts-ignore: error TS7005: Variable 'testUtilsLoaded' implicitly has an
    // 'any' type.
    if (!testUtilsLoaded) {
      console.log('Loading ' + scriptUrl);
      testUtilsLoaded = new ScriptLoader(scriptUrl, {type: 'module'}).load();
    }
    await testUtilsLoaded;
    console.log('Loaded ' + scriptUrl);
    return true;
  } catch (error) {
    testUtilsLoaded = null;
    return false;
  }
};

// @ts-ignore: error TS2339: Property 'getSwaAppId' does not exist on type
// 'typeof test'.
test.getSwaAppId = async () => {
  // @ts-ignore: error TS7005: Variable 'testUtilsLoaded' implicitly has an
  // 'any' type.
  if (!testUtilsLoaded) {
    // @ts-ignore: error TS2339: Property 'swaLoadTestUtils' does not exist on
    // type 'typeof test'.
    await test.swaLoadTestUtils();
  }

  // @ts-ignore: error TS2339: Property 'appID' does not exist on type 'Window &
  // typeof globalThis'.
  return String(window.appID);
};
