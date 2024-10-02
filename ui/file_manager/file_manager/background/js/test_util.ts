// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {ProgressCenterItem} from '../../common/js/progress_center_common.js';
import {ScriptLoader} from '../../common/js/script_loader.js';
import {descriptorEqual} from '../../common/js/util.js';
import type {XfBreadcrumb} from '../../widgets/xf_breadcrumb.js';

import type {RemoteRequest} from './runtime_loaded_test_util.js';
import {test} from './test_util_base.js';

export {test};

// Shorter alias for the task descriptor type.
type FileTaskDescriptor = chrome.fileManagerPrivate.FileTaskDescriptor;

// This is the shape of the data managed by overrideTasks().
interface ExecutedTask {
  descriptor: FileTaskDescriptor;
  entries: Entry[];
  callback: Function;
}

// A function that has been replaced by a Fake.
type FakedFunction = (...args: any[]) => (void|Promise<unknown>);

/**
 * Sanitizes the formatted date. Replaces unusual space with normal space.
 * @param strDate the date already in the string format.
 */
export function sanitizeDate(strDate: string): string {
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
 * @param contentWindow Window to be tested.
 * @return Details for each visible file row.
 */
test.util.sync.getFileList = (contentWindow: Window): string[][] => {
  const table =
      contentWindow.document.querySelector<HTMLElement>('#detail-table')!;
  const rows = table.querySelectorAll('li');
  const fileList = [];
  for (const row of rows) {
    fileList.push([
      row.querySelector('.filename-label')?.textContent ?? '',
      row.querySelector('.size')?.textContent ?? '',
      row.querySelector('.type')?.textContent ?? '',
      sanitizeDate(row.querySelector('.date')?.textContent || ''),
    ]);
  }
  return fileList;
};

/**
 * Returns the name of the files currently selected in the file list. Note the
 * routine has the same 'visible files' limitation as getFileList() above.
 *
 * @param contentWindow Window to be tested.
 * @return Selected file names.
 */
test.util.sync.getSelectedFiles = (contentWindow: Window): string[] => {
  const table = contentWindow.document.querySelector('#detail-table')!;
  const rows = table.querySelectorAll('li');
  const selected = [];
  for (const row of rows) {
    if (row.hasAttribute('selected')) {
      selected.push(row.querySelector('.filename-label')?.textContent ?? '');
    }
  }
  return selected;
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param contentWindow Window to be tested.
 * @param filename Name of the file to be selected.
 * @return True if file got selected, false otherwise.
 */
test.util.sync.selectFile =
    (contentWindow: Window, filename: string): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param filename Name of the file to be opened.
 * @return True if file got selected and a double click message is
 *     sent, false otherwise.
 */
test.util.sync.openFile =
    (contentWindow: Window, filename: string): boolean => {
      const query = '#file-list li.table-row[selected] .filename-label span';
      return test.util.sync.selectFile(contentWindow, filename) &&
          test.util.sync.fakeMouseDoubleClick(contentWindow, query);
    };

/**
 * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
 *
 * @param contentWindow The window where visitURL() was called.
 * @return The URL of the last URL visited.
 */
test.util.sync.getLastVisitedURL = (contentWindow: Window): string => {
  return contentWindow.fileManager.getLastVisitedUrl();
};

/**
 * Returns a string translation from its translation ID.
 * @param id The id of the translated string.
 */
test.util.sync.getTranslatedString =
    (contentWindow: Window, id: string): string => {
      return contentWindow.fileManager.getTranslatedString(id);
    };

/**
 * Executes Javascript code on a webview and returns the result.
 *
 * @param contentWindow Window to be tested.
 * @param webViewQuery Selector for the web view.
 * @param code Javascript code to be executed within the web view.
 * @param callback Callback function with results returned by the script.
 */
test.util.async.executeScriptInWebView =
    (contentWindow: Window, webViewQuery: string, code: string,
     callback: (a: unknown) => void) => {
      const webView =
          contentWindow.document.querySelector<chrome.webviewTag.WebView>(
              webViewQuery)!;
      webView.executeScript({code: code}, callback);
    };

/**
 * Selects |filename| and fakes pressing Ctrl+C, Ctrl+V (copy, paste).
 *
 * @param contentWindow Window to be tested.
 * @param filename Name of the file to be copied.
 * @return True if copying got simulated successfully. It does not
 *     say if the file got copied, or not.
 */
test.util.sync.copyFile =
    (contentWindow: Window, filename: string): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param filename Name of the file to be deleted.
 * @return True if deleting got simulated successfully. It does not
 *     say if the file got deleted, or not.
 */
test.util.sync.deleteFile =
    (contentWindow: Window, filename: string): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param command Command name.
 * @return True if the command is executed successfully.
 */
test.util.sync.execCommand =
    (contentWindow: Window, command: string): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param taskList List of tasks to be returned in
 *     fileManagerPrivate.getFileTasks().
 * @param isPolicyDefault Whether the default is set by policy.
 * @return Always return true.
 */
test.util.sync.overrideTasks =
    (contentWindow: Window, taskList: any[],
     isPolicyDefault: boolean = false): boolean => {
      const getFileTasks =
          (_entries: Entry[], _sourceUrls: string[],
           onTasks: (a: {tasks: any[], policyDefaultHandlerStatus: any}) =>
               void) => {
            // Call onTask asynchronously (same with original getFileTasks).
            setTimeout(() => {
              const policyDefaultHandlerStatus = isPolicyDefault ?
                  chrome.fileManagerPrivate.PolicyDefaultHandlerStatus
                      .DEFAULT_HANDLER_ASSIGNED_BY_POLICY :
                  undefined;

              onTasks({tasks: taskList, policyDefaultHandlerStatus});
            }, 0);
          };

      const executeTask =
          (descriptor: FileTaskDescriptor, entries: Entry[],
           callback: Function) => {
            executedTasks!.push({descriptor, entries, callback});
          };

      const setDefaultTask = (descriptor: FileTaskDescriptor) => {
        for (const task of taskList) {
          task.isDefault = descriptorEqual(task.descriptor, descriptor);
        }
      };


      executedTasks = [];
      contentWindow.chrome.fileManagerPrivate.getFileTasks = getFileTasks;
      contentWindow.chrome.fileManagerPrivate.executeTask = executeTask;
      contentWindow.chrome.fileManagerPrivate.setDefaultTask = setDefaultTask;
      return true;
    };

/**
 * Obtains the list of executed tasks.
 */
test.util.sync.getExecutedTasks = (_contentWindow: Window): null|
    Array<{descriptor: FileTaskDescriptor, fileNames: string[]}> => {
      if (!executedTasks) {
        console.error('Please call overrideTasks() first.');
        return null;
      }

      return executedTasks.map((task: ExecutedTask) => {
        return {
          descriptor: task.descriptor,
          fileNames: task.entries.map(e => e.name),
        };
      });
    };

/**
 * Obtains the list of executed tasks.
 * @param _contentWindow Window to be tested.
 * @param descriptor the task to *     check.
 * @param fileNames Name of files that should have been passed to the
 *     executeTasks().
 * @return True if the task was executed.
 */
test.util.sync.taskWasExecuted =
    (_contentWindow: Window,
     descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
     fileNames: string[]): boolean|null => {
      if (!executedTasks) {
        console.error('Please call overrideTasks() first.');
        return null;
      }
      const fileNamesStr = JSON.stringify(fileNames);
      const task = executedTasks.find(
          (task: ExecutedTask) =>
              descriptorEqual(task.descriptor, descriptor) &&
              fileNamesStr === JSON.stringify(task.entries.map(e => e.name)));
      return task !== undefined;
    };


let executedTasks: ExecutedTask[]|null = null;

/**
 * Invokes an executed task with |responseArgs|.
 * @param _contentWindow Window to be tested.
 * @param descriptor the task to be replied to.
 * @param responseArgs the arguments to invoke the callback with.
 */
test.util.sync.replyExecutedTask =
    (_contentWindow: Window, descriptor: FileTaskDescriptor,
     responseArgs: any[]) => {
      if (!executedTasks) {
        console.error('Please call overrideTasks() first.');
        return false;
      }
      const found = executedTasks.find(
          (task: ExecutedTask) => descriptorEqual(task.descriptor, descriptor));
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
 * @param contentWindow Window to be tested.
 */
test.util.sync.unload = (contentWindow: Window) => {
  contentWindow.fileManager.onUnloadForTest();
};

/**
 * Returns the path shown in the breadcrumb.
 *
 * @param contentWindow Window to be tested.
 * @return The breadcrumb path.
 */
test.util.sync.getBreadcrumbPath = (contentWindow: Window): string => {
  const doc = contentWindow.document;
  const breadcrumb =
      doc.querySelector<XfBreadcrumb>('#location-breadcrumbs xf-breadcrumb');

  if (!breadcrumb) {
    return '';
  }

  return '/' + breadcrumb.path;
};

/**
 * Obtains the preferences.
 * @param callback Callback function with results returned by the script.
 */
test.util.async.getPreferences = (callback: (a: any) => void) => {
  chrome.fileManagerPrivate.getPreferences(callback);
};

/**
 * Stubs out the formatVolume() function in fileManagerPrivate.
 *
 * @param contentWindow Window to be affected.
 */
test.util.sync.overrideFormat = (contentWindow: Window) => {
  contentWindow.chrome.fileManagerPrivate.formatVolume =
      (_volumeId: string, _filesystem: string, _volumeLabel: string) => {};
  return true;
};

/**
 * Run a contentWindow.requestAnimationFrame() cycle and resolve the
 * callback when that requestAnimationFrame completes.
 * @param contentWindow Window to be tested.
 * @param callback Completion callback.
 */
test.util.async.requestAnimationFrame =
    (contentWindow: Window, callback: (a: boolean) => void) => {
      contentWindow.requestAnimationFrame(() => {
        callback(true);
      });
    };

/**
 * Set the window text direction to RTL and wait for the window to redraw.
 * @param contentWindow Window to be tested.
 * @param callback Completion callback.
 */
test.util.async.renderWindowTextDirectionRTL =
    (contentWindow: Window, callback: (a: boolean) => void) => {
      contentWindow.document.documentElement.setAttribute('dir', 'rtl');
      contentWindow.document.body.setAttribute('dir', 'rtl');
      contentWindow.requestAnimationFrame(() => {
        callback(true);
      });
    };

/**
 * Map the appId to a map of all fakes applied in the foreground window e.g.:
 *  {'files#0': {'chrome.bla.api': FAKE}
 */
const foregroundReplacedObjects:
    Record<string, Record<string, PrepareFake>> = {};

/**
 * A factory that returns a fake (aka function) that returns a static value.
 * Used to force a callback-based API to return always the same value.
 */
function staticFakeFactory(
    attrName: string, staticValue: unknown): FakedFunction {
  return (...args: unknown[]) => {
    // This code is executed when the production code calls the function that
    // has been replaced by the test.
    // `args` is the arguments provided by the production code.
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
}

/**
 * A factory that returns an async function (aka a Promise) that returns a
 * static value. Used to force a promise-based API to return always the same
 * value.
 */
function staticPromiseFakeFactory(
    attrName: string, staticValue: unknown): FakedFunction {
  return async (..._args: unknown[]) => {
    // This code is executed when the production code calls the function that
    // has been replaced by the test.
    // `args` is the arguments provided by the production code.
    console.warn(
        `staticPromiseFake for "${attrName}" returning value: ${staticValue}`);
    return staticValue;
  };
}

/**
 * Registry of available fakes, it maps the an string ID to a factory
 * function which returns the actual fake used to replace an implementation.
 *
 */
const fakes = {
  'static_fake': staticFakeFactory,
  'static_promise_fake': staticPromiseFakeFactory,
};
type FakeId = keyof typeof fakes;

/**
 * Class holds the information for applying and restoring fakes.
 */
class PrepareFake {
  /**
   * The instance of the fake to be used, ready to be used.
   */
  private fake_: FakedFunction|null = null;

  /**
   * After traversing |context_| the object that holds the attribute to be
   * replaced by the fake.
   */
  private parentObject_: Record<string, any>|null = null;

  /**
   * After traversing |context_| the attribute name in |parentObject_| that
   * will be replaced by the fake.
   */
  private leafAttrName_: string = '';

  /**
   * Original object that was replaced by the fake.
   */
  private original_: any|null = null;

  /**
   * If this fake object has been constructed and everything initialized.
   */
  private prepared_: boolean = false;

  /**
   * Counter to record the number of times the static fake is called.
   */
  callCounter: number = 0;

  /**
   * Additional data provided from integration tests to the fake constructor.
   */
  private args_: unknown[];

  /**
   * List to record the arguments provided to the static fake calls.
   */
  calledArgs: unknown[][] = [];

  /**
   * @param attrName Name of the attribute to be replaced by the fake
   *   e.g.: "chrome.app.window.create".
   * @param fakeId The name of the fake to be used from `fakes_`.
   * @param context The context where the attribute will be traversed from,
   *   e.g.: Window object.
   * @param args Additional args provided from the integration test to the fake,
   *     e.g.: static return value.
   */
  constructor(
      private attrName_: string, private fakeId_: keyof typeof fakes,
      private context_: Object, ...args: unknown[]) {
    this.args_ = args;
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
   * @param contentWindow Window to be tested.
   */
  replace(contentWindow: Window) {
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

    this.saveOriginal_(contentWindow);
    this.parentObject_[this.leafAttrName_] = async (...args: unknown[]) => {
      const result = await this.fake_!(...args);
      this.callCounter++;
      this.calledArgs.push([...args]);
      return result;
    };
  }

  /**
   * Restores the original implementation that had been previously replaced by
   * the fake.
   */
  restore() {
    if (!this.original_) {
      return;
    }
    this.parentObject_![this.leafAttrName_] = this.original_;
    this.original_ = null;
  }

  /**
   * Saves the original implementation to be able restore it later.
   * @param contentWindow Window to be tested.
   */
  private saveOriginal_(contentWindow: Window) {
    const windowFakes = foregroundReplacedObjects[contentWindow.appID] || {};
    foregroundReplacedObjects[contentWindow.appID] = windowFakes;

    // Only save once, otherwise it can save an object that is already fake.
    if (!windowFakes[this.attrName_]) {
      if (!this.parentObject_) {
        console.error(`Failed to find the fake context: ${this.attrName_}`);
        return;
      }
      const original = this.parentObject_[this.leafAttrName_];
      this.original_ = original;
      windowFakes[this.attrName_] = this;
    }
    return;
  }

  /**
   * Constructs the fake.
   */
  private buildFake_() {
    const factory: (...args: any[]) => FakedFunction = fakes[this.fakeId_];
    if (!factory) {
      throw new Error(`Failed to find the fake factory for ${this.fakeId_}`);
    }

    this.fake_ = factory(this.attrName_, ...this.args_);
  }

  /**
   * Finds the parent and the object to be replaced by fake.
   */
  private traverseContext_() {
    let target = this.context_ as any;
    let parentObj: Object|null = null;
    let attr = '';

    for (const a of this.attrName_.split('.')) {
      attr = a;
      parentObj = target;
      target = target[a] as Object;

      if (target === undefined) {
        throw new Error(`Couldn't find "${0}" from "${this.attrName_}"`);
      }
    }

    this.parentObject_ = parentObj;
    this.leafAttrName_ = attr;
  }
}

/**
 * Replaces implementations in the foreground page with fakes.
 *
 * @param contentWindow Window to be tested.
 * @param fakeData An object mapping the path to the
 * object to be replaced and the value is the Array with fake id and
 * additional arguments for the fake constructor, e.g.: fakeData = {
 *     'chrome.app.window.create' : [
 *       'static_fake',
 *       ['some static value', 'other arg'],
 *     ]
 *   }
 *
 *  This will replace the API 'chrome.app.window.create' with a static fake,
 *  providing the additional data to static fake: ['some static value',
 * 'other value'].
 */
test.util.sync.foregroundFake =
    (contentWindow: Window, fakeData: Record<string, [FakeId, unknown[]]>) => {
      const entries = Object.entries(fakeData);
      for (const [path, mockValue] of entries) {
        const fakeId = mockValue[0];
        const fakeArgs = mockValue[1] || [];
        const fake = new PrepareFake(path, fakeId, contentWindow, ...fakeArgs);
        fake.prepare();
        fake.replace(contentWindow);
      }
      return entries.length;
    };

/**
 * Removes all fakes that were applied to the foreground page.
 * @param contentWindow Window to be tested.
 */
test.util.sync.removeAllForegroundFakes = (contentWindow: Window) => {
  const windowFakes = foregroundReplacedObjects[contentWindow.appID];
  if (!windowFakes) {
    console.error(`Failed to find the fakes for window ${contentWindow.appID}`);
    return 0;
  }

  const savedFakes = Object.entries(windowFakes);
  let removedCount = 0;
  for (const [_path, fake] of savedFakes) {
    fake.restore();
    removedCount++;
  }

  return removedCount;
};

/**
 * Obtains the number of times the static fake api is called.
 * @param contentWindow Window to be tested.
 * @param fakedApi Path of the method that is faked.
 * @return Number of times the fake api called.
 */
test.util.sync.staticFakeCounter =
    (contentWindow: Window, fakedApi: string): number => {
      const windowFakes = foregroundReplacedObjects[contentWindow.appID];
      if (!windowFakes) {
        console.error(
            `Failed to find the fakes for window ${contentWindow.appID}`);
        return -1;
      }
      const fake = windowFakes[fakedApi];
      return fake?.callCounter ?? -1;
    };

/**
 * Obtains the list of arguments with which the static fake api was called.
 * @param contentWindow Window to be tested.
 * @param fakedApi Path of the method that is faked.
 * @return An array with all calls to this fake, each item
 *     is an array with all args passed in when the fake was called.
 */
test.util.sync.staticFakeCalledArgs =
    (contentWindow: Window, fakedApi: string): unknown[][] => {
      const fake = foregroundReplacedObjects[contentWindow.appID]![fakedApi]!;
      return fake.calledArgs;
    };

/**
 * Send progress item to Foreground page to display.
 * @param id Progress item id.
 * @param type Type of progress item.
 * @param state State of the progress item.
 * @param message Message of the progress item.
 * @param remainingTime The remaining time of the progress in second.
 * @param progressMax Max value of the progress.
 * @param progressValue Current value of the progress.
 * @param count Number of items being processed.
 */
test.util.sync.sendProgressItem =
    (id: string, type: ProgressItemType, state: ProgressItemState,
     message: string, remainingTime: number, progressMax: number = 1,
     progressValue: number = 0, count: number = 1) => {
      const item = new ProgressCenterItem();
      item.id = id;
      item.type = type;
      item.state = state;
      item.message = message;
      item.remainingTime = remainingTime;
      item.progressMax = progressMax;
      item.progressValue = progressValue;
      item.itemCount = count;

      window.background.progressCenter.updateItem(item);
      return true;
    };

/**
 * Remote call API handler. This function handles messages coming from the
 * test harness to execute known functions and return results. This is a
 * dummy implementation that is replaced by a real one once the test harness
 * is fully loaded.
 */
test.util.executeTestMessage =
    (_request: RemoteRequest, _callback: (...a: unknown[]) => void) => {
      throw new Error('executeTestMessage not implemented');
    };

/**
 * Handles a direct call from the integration test harness. We execute
 * swaTestMessageListener call directly from the FileManagerBrowserTest.
 * This method avoids enabling external callers to Files SWA. We forward
 * the response back to the caller, as a serialized JSON string.
 */
test.swaTestMessageListener = (request: any) => {
  request.contentWindow = window;
  return new Promise(resolve => {
    test.util.executeTestMessage(request, (response: unknown) => {
      response = response === undefined ? '@undefined@' : response;
      resolve(JSON.stringify(response));
    });
  });
};

let testUtilsLoaded: null|Promise<string> = null;

test.swaLoadTestUtils = async () => {
  const scriptUrl = 'background/js/runtime_loaded_test_util.js';
  try {
    if (!testUtilsLoaded) {
      console.info('Loading ' + scriptUrl);
      testUtilsLoaded = new ScriptLoader(scriptUrl, {type: 'module'}).load();
    }
    await testUtilsLoaded;
    console.info('Loaded ' + scriptUrl);
    return true;
  } catch (error) {
    testUtilsLoaded = null;
    return false;
  }
};

test.getSwaAppId = async () => {
  if (!testUtilsLoaded) {
    await test.swaLoadTestUtils();
  }

  return String(window.appID);
};
