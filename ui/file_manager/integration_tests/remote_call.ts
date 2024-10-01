// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DirectoryTreePageObject} from './file_manager/page_objects/directory_tree.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './file_manager/test_data.js';
import type {ElementObject, FilesAppState, KeyModifiers, VolumeType} from './prod/file_manager/shared_types.js';
import {addEntries, getCaller, openEntryChoosingWindow, pending, pollForChosenEntry, repeatUntil, sendTestMessage, TestEntryInfo} from './test_util.js';

export type MenuObject = ElementObject&{
  items?: ElementObject[],
};
/**
 * When step by step tests are enabled, turns on automatic step() calls. Note
 * that if step() is defined at the time of this call, invoke it to start the
 * test auto-stepping ball rolling.
 */
window.autoStep = () => {
  window.autostep = window.autostep || false;
  if (!window.autostep) {
    window.autostep = true;
  }
  if (window.autostep && typeof window.step === 'function') {
    window.step();
  }
};

/**
 * This error type is thrown by executeJsInPreviewTagSwa_ if the script to
 * execute in the untrusted context produces an error.
 */
export class ExecuteScriptError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ExecuteScriptError';
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
export class RemoteCall {
  /**
   * Tristate holding the cached result of isStepByStepEnabled_().
   */
  private cachedStepByStepEnabled_: boolean|null = null;

  /**
   * @param origin ID of the app to be manipulated.
   */
  constructor(protected origin_: string) {}

  /**
   * Checks whether step by step tests are enabled or not.
   */
  private async isStepByStepEnabled_(): Promise<boolean> {
    if (this.cachedStepByStepEnabled_ === null) {
      this.cachedStepByStepEnabled_ = await new Promise(
          fulfill => chrome.commandLinePrivate.hasSwitch(
              'enable-file-manager-step-by-step-tests', fulfill));
    }
    return this.cachedStepByStepEnabled_!;
  }

  /**
   * Sends a test `message` to the test code running in the File Manager.
   * @return A promise which when fulfilled returns the result of executing test
   *     code with the given message.
   */
  sendMessage(message: Object): Promise<unknown> {
    return new Promise((fulfill) => {
      chrome.runtime.sendMessage(this.origin_, message, {}, fulfill);
    });
  }

  /**
   * Calls a remote test util in the Files app's extension. See:
   * registerRemoteTestUtils in test_util_base.js.
   *
   * @param func Function name.
   * @param appId App window Id or null for functions not requiring a window.
   * @param args Array of arguments.
   * @param callback Callback handling the function's result.
   * @return Promise to be fulfilled with the result of the remote utility.
   */
  async callRemoteTestUtil<T>(
      func: string, appId: null|string, args?: null|readonly unknown[],
      callback?: (r: T) => void): Promise<T> {
    const stepByStep = await this.isStepByStepEnabled_();
    let finishCurrentStep;
    if (stepByStep) {
      while (window.currentStep) {
        await window.currentStep;
      }
      window.currentStep = new Promise(resolve => {
        finishCurrentStep = () => {
          // eslint-disable-next-line no-console
          console.groupEnd();
          window.currentStep = null;
          resolve();
        };
      });
      // eslint-disable-next-line no-console
      console.group('Executing: ' + func + ' on ' + appId + ' with args: ');
      console.info(args);
      if (window.autostep !== true) {
        await new Promise<void>((onFulfilled) => {
          console.info('Type step() to continue...');
          window.step = function() {
            window.step = null;
            onFulfilled();
          };
        });
      } else {
        console.info('Auto calling step() ...');
      }
    }
    const response = await this.sendMessage({func, appId, args});

    if (stepByStep) {
      console.info('Returned value:');
      console.info(JSON.stringify(response));
      finishCurrentStep!();
    }
    if (callback) {
      callback(response as T);
    }
    return response as T;
  }

  /**
   * Waits for a SWA window to be open.
   * @param debug Whether to debug the findSwaWindow.
   */
  async waitForWindow(debug: boolean = false): Promise<string> {
    const caller = getCaller();
    const appId = await repeatUntil(async () => {
      const msg = {
        name: 'findSwaWindow',
        debug: debug ?? undefined,
      };
      const ret = await sendTestMessage(msg);
      if (ret === 'none') {
        return pending(caller, 'Wait for SWA window');
      }
      return ret;
    });

    return appId;
  }

  /**
   * Waits for the dialog window and waits for it to fully load.
   * @return dialog's id.
   */
  async waitForDialog(): Promise<string> {
    const dialog = await this.waitForWindow();

    // Wait for Files app to finish loading.
    await this.waitFor('isFileManagerLoaded', dialog, true);

    return dialog;
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @return  Promise to be fulfilled when the element appears.
   */
  async waitForElement(appId: string, query: string|string[]):
      Promise<ElementObject> {
    return this.waitForElementStyles(appId, query, []);
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @param styleNames List of CSS property name to be obtained. NOTE: Causes
   *     element style re-calculation.
   * @return Promise to be fulfilled when the element appears.
   */
  async waitForElementStyles(
      appId: string, query: string|string[],
      styleNames: string[]): Promise<ElementObject> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements = await this.callRemoteTestUtil<ElementObject[]>(
          'deepQueryAllElements', appId, [query, styleNames]);
      if (elements && elements.length > 0) {
        return elements[0];
      }
      return pending(caller, 'Element %s is not found.', query);
    });
  }

  /**
   * Waits for a remote test function to return a specific result.
   *
   * @param funcName Name of remote test function to be executed.
   * @param appId App window Id.
   * @param expectedResult An value to be checked against the return value of
   *     `funcName` or a callback that receives the return value of `funcName`
   *     and returns true if the result is the expected value.
   * @param args Arguments to be provided to `funcName` when executing it.
   * @return Promise to be fulfilled when the `expectedResult` is returned from
   *     `funcName` execution.
   */
  waitFor(
      funcName: string, appId: null|string,
      expectedResult: boolean|((r: unknown) => boolean),
      args?: unknown[]|null): Promise<void> {
    const caller = getCaller();
    args = args || [];
    return repeatUntil(async () => {
      const result = await this.callRemoteTestUtil(funcName, appId, args);
      if (typeof expectedResult === 'function' && expectedResult(result)) {
        return result;
      }
      if (expectedResult === result) {
        return result;
      }
      const msg = 'waitFor: Waiting for ' +
          `${funcName} to return ${expectedResult}, ` +
          `but got ${JSON.stringify(result)}.`;
      return pending(caller, msg);
    });
  }

  /**
   * Waits for the specified element leaving from the DOM.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @return Promise to be fulfilled when the element is lost.
   */
  waitForElementLost(appId: string, query: string|string[]): Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements = await this.queryElements(appId, query);
      if (elements.length > 0) {
        return pending(caller, 'Elements %j still exists.', elements);
      }
      return true;
    });
  }

  async queryElements(
      appId: string, query: string|string[],
      styleNames?: string[]): Promise<ElementObject[]> {
    const args: Array<string|string[]> = [query];
    if (styleNames) {
      args.push(styleNames);
    }
    return this.callRemoteTestUtil<ElementObject[]>(
        'deepQueryAllElements', appId, args);
  }

  /**
   * Waits for the `query` to match `count` elements.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @param count The expected element match count.
   * @return Promise to be fulfilled on success.
   */
  async waitForElementsCount(
      appId: string, query: string|string[], count: number): Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const expect = `Waiting for [${query}] to match ${count} elements`;
      const result =
          await this.callRemoteTestUtil('countElements', appId, [query, count]);
      return !result ? pending(caller, expect) : true;
    });
  }

  /**
   * Sends a fake key down event.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param key DOM UI Events Key value.
   * @param ctrlKey Control key flag.
   * @param shiftKey Shift key flag.
   * @param altKey Alt key flag.
   * @return Promise to be fulfilled or rejected depending on
   *     the result.
   */
  async fakeKeyDown(
      appId: string, query: string|string[], key: string, ctrlKey: boolean,
      shiftKey: boolean, altKey: boolean): Promise<boolean> {
    const result = await this.callRemoteTestUtil(
        'fakeKeyDown', appId, [query, key, ctrlKey, shiftKey, altKey]);
    if (result) {
      return true;
    }
    throw new Error('Fail to fake key down.');
  }

  /**
   * Sets the given input text on the element identified by the query.
   * @param appId App window ID.
   * @param selector The query selector to locate the element
   * @param text The text to be set on the element.
   */
  async inputText(appId: string, selector: string|string[], text: string) {
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('inputText', appId, [selector, text]));
  }

  /**
   * Gets file entries just under the volume.
   * @param volumeType Volume type.
   * @param names File name list.
   * @return Promise to be fulfilled with file urls for entries or rejected
   *     depending on the result.
   */
  getFilesUnderVolume(volumeType: VolumeType, names: string[]):
      Promise<string[]> {
    return this.callRemoteTestUtil<string[]>(
        'getFilesUnderVolume', null, [volumeType, names]);
  }

  /**
   * Waits for a single file.
   * @param volumeType Volume type.
   * @param name File name.
   * @return Promise to be fulfilled when the file had found.
   */
  waitForFile(volumeType: VolumeType, name: string): Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      if ((await this.getFilesUnderVolume(volumeType, [name])).length === 1) {
        return true;
      }
      return pending(caller, `"${name}" is not found.`);
    });
  }

  /**
   * Shorthand for clicking an element.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @return Promise to be fulfilled with the clicked element.
   */
  async waitAndClickElement(
      appId: string, query: string|string[],
      keyModifiers?: KeyModifiers): Promise<ElementObject> {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil<boolean>(
        'fakeMouseClick', appId, [query, keyModifiers]);
    chrome.test.assertTrue(result, 'mouse click failed.');
    return element;
  }

  /**
   * Shorthand for right-clicking an element.
   * @param appId App window Id.
   * @param query Query to specify the element.
   *     If query is an array, `query[0]` specifies the first element(s),
   * `query[1]` specifies elements inside the shadow DOM of the first element,
   * and so on.
   * @return Promise to be fulfilled with the clicked element.
   */
  async waitAndRightClick(
      appId: string, query: string|string[],
      keyModifiers?: KeyModifiers): Promise<ElementObject> {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil<boolean>(
        'fakeMouseRightClick', appId, [query, keyModifiers]);
    chrome.test.assertTrue(result, 'mouse right-click failed.');
    return element;
  }

  /**
   * Shorthand for focusing an element.
   * @param appId App window Id.
   * @param query Query to specify the element to be focused.
   * @return Promise to be fulfilled with the focused element.
   */
  async focus(appId: string, query: string[]): Promise<null|ElementObject> {
    const element = await this.waitForElement(appId, query);
    const result =
        await this.callRemoteTestUtil<boolean>('focus', appId, query);
    chrome.test.assertTrue(result, 'focus failed.');
    return element;
  }

  /**
   * Simulates Click in the UI in the middle of the element.
   * @param appId App window ID contains the element. NOTE: The click is
   *     simulated on most recent window in the window system.
   * @param query Query to the element to be clicked.
   * @param leftClick If true, simulate left click. Otherwise simulate right
   *     click.
   * @return A promise fulfilled after the click event.
   */
  async simulateUiClick(
      appId: string, query: string|string[],
      leftClick: boolean = true): Promise<unknown> {
    const element = await this.waitForElementStyles(appId, query, ['display']);
    chrome.test.assertTrue(!!element, 'element for simulateUiClick not found');

    // Find the middle of the element.
    const left = element.renderedLeft ?? 0;
    const top = element.renderedTop ?? 0;
    const width = element.renderedWidth ?? 0;
    const height = element.renderedHeight ?? 0;
    const x = Math.floor(left + (width / 2));
    const y = Math.floor(top + (height / 2));

    return sendTestMessage(
        {appId, name: 'simulateClick', 'clickX': x, 'clickY': y, leftClick});
  }

  /**
   * Simulates Right Click in blank/empty space of the file list element.
   * @param appId App window ID contains the element. NOTE: The click is
   *     simulated on most recent window in the window system.
   * @return A promise fulfilled after the click event.
   */
  async rightClickFileListBlankSpace(appId: string): Promise<void> {
    await this.simulateUiClick(
        appId, '#file-list .spacer.signals-overscroll', false);
  }

  /**
   * Selects the option given by the index in the menu given by the type. This
   * only works in V2 version of the search.
   * @param appId The ID that identifies the files app.
   * @param type The search option type (location, recency, type).
   * @param index The index of the button.
   * @return A promise that resolves to true if click was successful and false
   *     otherwise.
   */
  selectSearchOption(appId: string, type: string, index: number):
      Promise<boolean> {
    return this.callRemoteTestUtil('fakeMouseClick', appId, [
      [
        'xf-search-options',
        `xf-select#${type}-selector`,
        `cr-action-menu cr-button:nth-of-type(${index})`,
      ],
    ]);
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
export class RemoteCallFilesApp extends RemoteCall {
  /**
   * Sends a test `message` to the test code running in the File Manager.
   */
  override sendMessage(message: {appId: string}): Promise<unknown> {
    const command = {
      name: 'callSwaTestMessageListener',
      appId: message.appId,
      data: JSON.stringify(message),
    };

    return new Promise((fulfill) => {
      chrome.test.sendMessage(JSON.stringify(command), (response) => {
        if (response === '"@undefined@"') {
          fulfill(undefined);
        } else {
          try {
            fulfill(response === '' ? true : JSON.parse(response));
          } catch (e) {
            console.error(`Failed to parse "${response}" due to ${e}`);
            fulfill(false);
          }
        }
      });
    });
  }

  async getWindows() {
    return JSON.parse(await sendTestMessage({name: 'getWindows'}) as string);
  }

  /**
   * Executes a script in the context of a <preview-tag> element contained in
   * the window.
   * For SWA: It's the first chrome-untrusted://file-manager <iframe>.
   * For legacy: It's the first elements based on the `query`.
   * Responds with its output.
   *
   * @param appId App window Id.
   * @param query Query to the <preview-tag> element (this is ignored for SWA).
   * @param statement Javascript statement to be executed within the
   *     <preview-tag>.
   * @return resolved with the return value of the `statement`.
   */
  async executeJsInPreviewTag<T>(
      _appId: string, _query: string[],
      statement: string): Promise<T|undefined> {
    return this.executeJsInPreviewTagSwa_(statement);
  }

  /**
   * Injects javascript statemenent in the first chrome-untrusted://file-manager
   * page found and respond with its output.
   */
  private async executeJsInPreviewTagSwa_<T>(statement: string):
      Promise<T|undefined> {
    const script = `try {
          let result = ${statement};
          result = result === undefined ? '@undefined@' : [result];
          window.domAutomationController.send(JSON.stringify(result));
        } catch (error) {
          const errorInfo = {'@error@':  error.message, '@stack@': error.stack};
          window.domAutomationController.send(JSON.stringify(errorInfo));
        }`;

    const command = {
      name: 'executeScriptInChromeUntrusted',
      data: script,
    };

    const response = await sendTestMessage(command) as string;
    if (response === '"@undefined@"') {
      return undefined;
    }
    const output = JSON.parse(response);
    if ('@error@' in output) {
      console.error(output['@error@']);
      console.error('Original StackTrace:\n' + output['@stack@']);
      throw new ExecuteScriptError(
          'Error executing JS in Preview: ' + output['@error@']);
    } else {
      return output;
    }
  }

  /**
   * Waits until the expected URL shows in the last opened browser tab.
   * @return Promise to be fulfilled when the expected URL is shown in a browser
   *     window.
   */
  async waitForLastOpenedBrowserTabUrl(expectedUrl: string): Promise<string> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const command = {name: 'getLastActiveTabURL'};
      const activeBrowserTabURL = await sendTestMessage(command);
      if (activeBrowserTabURL !== expectedUrl) {
        return pending(
            caller, 'waitForActiveBrowserTabUrl: expected %j actual %j.',
            expectedUrl, activeBrowserTabURL);
      }
      return undefined;
    });
  }

  /**
   * Returns whether a window exists with the expected origin.
   * @return Promise resolved with true or false depending on whether such
   *     window exists.
   */
  async windowOriginExists(expectedOrigin: string): Promise<boolean> {
    const command = {name: 'expectWindowOrigin', expectedOrigin};
    const windowExists = await sendTestMessage(command);
    return windowExists === 'true';
  }

  /**
   * Waits for the file list turns to the given contents.
   * @param appId App window Id.
   * @param expected Expected contents of file list.
   * @param options Options of the comparison. If orderCheck is true, it also
   *     compares the order of files. If ignoreLastModifiedTime is true, it
   * compares the file without its last modified time.
   * @return Promise to be fulfilled when the file list turns to the given
   *     contents.
   */
  waitForFiles(appId: string, expected: string[][], options?: {
    orderCheck?: boolean,
    ignoreFileSize?: boolean,
    ignoreLastModifiedTime?: boolean,
  }): Promise<void> {
    options = options || {};
    const caller = getCaller();
    return repeatUntil(async () => {
      const files =
          await this.callRemoteTestUtil<string[][]>('getFileList', appId, []);
      if (!options!.orderCheck) {
        files.sort();
        expected.sort();
      }
      for (let i = 0; i < Math.min(files.length, expected.length); i++) {
        // Change the value received from the UI to match when comparing.
        if (options!.ignoreFileSize) {
          files[i]![1] = expected[i]![1]!;
        }
        if (options!.ignoreLastModifiedTime) {
          if (expected[i]!.length < 4) {
            // Expected sometimes doesn't include the modified time at all, so
            // just remove from the data from UI.
            files[i]!.splice(3, 1);
          } else {
            files[i]![3]! = expected[i]![3]!;
          }
        }
      }
      if (!chrome.test.checkDeepEq(expected, files)) {
        return pending(
            caller, 'waitForFiles: expected: %j actual %j.', expected, files);
      }
      return undefined;
    });
  }

  /**
   * Waits until the number of files in the file list is changed from the
   * given number.
   * @param appId App window Id.
   * @param lengthBefore Number of items visible before.
   * @return Promise to be fulfilled with the contents of files.
   */
  waitForFileListChange(appId: string, lengthBefore: number): Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const files =
          await this.callRemoteTestUtil<string[][]>('getFileList', appId, []);
      files.sort();

      const notReadyRows =
          files.filter((row) => row.filter(cell => cell === '...').length);

      if (notReadyRows.length === 0 && files.length !== lengthBefore &&
          files.length !== 0) {
        return files;
      } else {
        return pending(
            caller, 'The number of file is %d. Not changed.', lengthBefore);
      }
    });
  }

  /**
   * Waits until the given taskId appears in the executed task list.
   * @param appId App window Id.
   * @param descriptor Task to watch.
   * @param fileNames Name of files that should have been passed to the
   *     executeTasks().
   * @param replyArgs arguments to reply to executed task.
   * @return Promise to be fulfilled when the task appears in the executed task
   *     list.
   */
  waitUntilTaskExecutes(
      appId: string, descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
      fileNames: string[], replyArgs?: Object[]): Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      if (!await this.callRemoteTestUtil(
              'taskWasExecuted', appId, [descriptor, fileNames])) {
        const tasks = await this.callRemoteTestUtil<Array<{
          descriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
          fileNames: string[],
        }>>('getExecutedTasks', appId, []);
        const executedTasks = tasks.map((task) => {
          const {appId, taskType, actionId} = task.descriptor;
          const executedFileNames = task.fileNames;
          return `${appId}|${taskType}|${actionId} for ${
              JSON.stringify(executedFileNames)}`;
        });
        return pending(caller, 'Executed task is %j', executedTasks);
      }
      if (replyArgs) {
        await this.callRemoteTestUtil(
            'replyExecutedTask', appId, [descriptor, replyArgs]);
      }
      return undefined;
    });
  }

  /**
   * Checks if the next tabforcus'd element has the given ID or not.
   * @param appId App window Id.
   * @param elementId String of `id` attribute which the next tabfocus'd element
   *     should have.
   */
  async checkNextTabFocus(appId: string, elementId: string): Promise<boolean> {
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'Tab key dispatch failure');

    const caller = getCaller();
    return repeatUntil(async () => {
      const element = await this.callRemoteTestUtil<ElementObject|null>(
          'getActiveElement', appId, []);
      // For directory tree implementation, directory tree itself
      // ("#directory-tree") is not focusable, the underlying tree item will be
      // focused, for directory tree related focus check, the `elementId` format
      // will be "directory-tree#<tree item label>", so we need to check the
      // label here for the tree.
      if (elementId.startsWith('directory-tree#')) {
        const treeItemLabel = elementId.split('#')[1];
        if (element && element.attributes['label'] === treeItemLabel) {
          return true;
        }
      }
      if (element && element.attributes['id'] === elementId) {
        return true;
      }
      // Try to check the shadow root.
      const activeElements = await this.callRemoteTestUtil<ElementObject[]>(
          'deepGetActivePath', appId, []);
      const matches =
          activeElements.filter(el => el.attributes['id'] === elementId);
      if (matches.length === 1) {
        return true;
      }
      if (matches.length > 1) {
        console.error(`Found ${
            matches.length} active elements with the same id: ${elementId}`);
      }

      return pending(
          caller,
          `Waiting for active element with id: "${
              elementId}", but current is: "${element!.attributes['id']}"`);
    });
  }

  /**
   * Returns a promise that repeatedly checks for a file with the given name to
   * be selected in the app window with the given ID. Typical use
   *
   * await remoteCall.waitUntilSelected('file#0', 'hello.txt');
   * ... // either the test timed out or hello.txt is currently selected.
   *
   * @param appId App window Id.
   * @param fileName the name of the file to be selected.
   * @return Promise that indicates if selection was successful.
   */
  async waitUntilSelected(appId: string, fileName: string): Promise<boolean> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const selected =
          await this.callRemoteTestUtil('selectFile', appId, [fileName]);
      if (!selected) {
        return pending(caller, `File ${fileName} not yet selected`);
      }
      return undefined;
    });
  }

  /**
   * Waits until the current directory is changed.
   * @param appId App window Id.
   * @param expectedPath Path to be changed to.
   * @return Promise to be fulfilled when the current directory is changed to
   *     expectedPath.
   */
  async waitUntilCurrentDirectoryIsChanged(appId: string, expectedPath: string):
      Promise<void> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const path =
          await this.callRemoteTestUtil('getBreadcrumbPath', appId, []);
      if (path !== expectedPath) {
        return pending(
            caller, 'Expected path is %s got %s', expectedPath, path);
      }
      return undefined;
    });
  }

  /**
   * Waits until the expected number of volumes is mounted.
   * @param expectedVolumesCount Expected number of mounted volumes.
   * @return promise Promise to be fulfilled.
   */
  async waitForVolumesCount(expectedVolumesCount: number): Promise<unknown> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const volumesCount = await sendTestMessage({name: 'getVolumesCount'});
      if (volumesCount === expectedVolumesCount.toString()) {
        return;
      }
      const msg =
          'Expected number of mounted volumes: ' + expectedVolumesCount +
          '. Actual: ' + volumesCount;
      return pending(caller, msg);
    });
  }

  /**
   * Isolates the specified banner to test. The banner is still checked against
   * it's filters, but is now the top priority banner.
   * @param appId App window Id
   * @param bannerTagName Banner tag name in lowercase to isolate.
   */
  async isolateBannerForTesting(appId: string, bannerTagName: string) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'isolateBannerForTesting', appId, [bannerTagName]));
  }

  /**
   * Disables banners from attaching to the DOM.
   * @param appId App window Id
   */
  async disableBannersForTesting(appId: string) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('disableBannersForTesting', appId, []));
  }

  /**
   * Sends text to the search box in the Files app.
   * @param appId App window Id
   * @param text The text to type in the search box.
   */
  async typeSearchText(appId: string, text: string) {
    const searchBoxInput = ['#search-box cr-input'];

    // Focus the search box.
    await this.waitAndClickElement(appId, '#search-button');

    // Wait for search to fully open.
    await this.waitForElementLost(appId, '#search-wrapper[collapsed]');

    // Input the text.
    await this.inputText(appId, searchBoxInput, text);

    // Notify the element of the input.
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box cr-input', 'input']));
  }

  /**
   * Waits for the search box auto complete list to appear.
   * @return Array of the names in the auto complete list.
   */
  async waitForSearchAutoComplete(appId: string): Promise<string[]> {
    // Wait for the list to appear.
    await this.waitForElement(appId, '#autocomplete-list li');

    // Return the result.
    const elements = await this.queryElements(appId, ['#autocomplete-list li']);
    return elements.map(element => element.text ?? '');
  }

  /**
   * Disables nudges from expiring for testing.
   * @param appId App window Id
   */
  async disableNudgeExpiry(appId: string) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('disableNudgeExpiry', appId, []));
  }

  /**
   * Selects the file and displays the context menu for the file.
   * @return resolved when the context menu is visible.
   */
  async showContextMenuFor(appId: string, fileName: string): Promise<void> {
    // Select the file.
    await this.waitUntilSelected(appId, fileName);

    // Right-click to display the context menu.
    await this.waitAndRightClick(appId, '.table-row[selected]');

    // Wait for the context menu to appear.
    await this.waitForElement(appId, '#file-context-menu:not([hidden])');

    // Wait for the tasks to be fully fetched.
    await this.waitForElement(appId, '#tasks[get-tasks-completed]');
  }

  /**
   * @param appId App window Id.
   */
  async dismissMenu(appId: string): Promise<void> {
    await this.fakeKeyDown(appId, 'body', 'Escape', false, false, false);
  }

  /**
   * Returns the menu as `ElementObject` and its menu-items (including
   * separators) in the `items` property.
   * @param appId App window Id.
   * @param menu The name of the menu.
   * @return Promise to be fulfilled with the menu.
   */
  async getMenu(appId: string, menu: string): Promise<undefined|MenuObject> {
    const menuId = this.getMenuId(menu);
    if (!menuId) {
      return;
    }

    // Get the top level menu element.
    const menuElement: MenuObject = await this.waitForElement(appId, menuId);
    // Query all the menu items.
    menuElement.items = await this.queryElements(appId, `${menuId} > *`);
    return menuElement;
  }

  getMenuId(menu: string) {
    // TODO: Implement for other menus.
    if (menu === 'context-menu') {
      return '#file-context-menu';
    } else if (menu === 'tasks') {
      return '#tasks-menu';
    }

    console.error(`Invalid menu '${menu}'`);
    return '';
  }

  async waitForMenuItem(appId: string, menu: string, commandId: string):
      Promise<undefined|MenuObject> {
    const menuId = this.getMenuId(menu);
    if (!menuId) {
      return;
    }

    const visibleMenu = `${menuId}:not([hidden])`;
    const visibleMenuItem = `[command="${commandId}"]:not([hidden])`;

    await this.waitForElement(appId, visibleMenu);
    await this.waitForElement(appId, visibleMenuItem);

    return this.getMenu(appId, menu);
  }

  async waitAndClickMenuItem(appId: string, menu: string, commandId: string) {
    await this.waitForMenuItem(appId, menu, commandId);
    const visibleMenuItem =
        `[command="${commandId}"]:not([hidden]):not([disabled])`;
    await this.waitAndClickElement(appId, visibleMenuItem);
  }

  /**
   * Displays the "tasks" menu from the "OPEN" button dropdown.
   * The caller code has to prepare the selection to have multiple tasks.
   * @param appId App window Id.
   */
  async expandOpenDropdown(appId: string) {
    // Wait the OPEN button to have multiple tasks.
    await this.waitAndClickElement(appId, '#tasks[multiple]');
  }

  /**
   * Checks if an item is pinned on drive or not.
   * @param appId app window ID.
   * @param path Path from the drive mount point, e.g. /root/test.txt
   * @param status Pinned status to expect drive item to be.
   */
  async expectDriveItemPinnedStatus(
      appId: string, path: string, status: boolean) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertEq(
        await sendTestMessage({
          appId,
          name: 'isItemPinned',
          path,
        }),
        String(status));
  }

  /**
   * Sends a delete event via the `OnFilesChanged` drivefs delegate method.
   * @param appId app window ID.
   * @param path Path from the drive mount point, e.g. /root/test.txt
   */
  async sendDriveCloudDeleteEvent(appId: string, path: string) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    await sendTestMessage({
      appId,
      name: 'sendDriveCloudDeleteEvent',
      path,
    });
  }

  /**
   * Whether the Jellybean UI is enabled.
   * @param appId app window ID
   */
  async isCrosComponents(appId: string): Promise<boolean> {
    return await sendTestMessage({
             appId,
             name: 'isCrosComponents',
           }) === 'true';
  }

  /**
   * Waits for the nudge with the given text to be visible.
   * @param appId app window ID.
   * @param expectedText Text that should be displayed in the Nudge.
   */
  async waitNudge(appId: string, expectedText: string): Promise<boolean> {
    const caller = getCaller();
    return repeatUntil(async () => {
      const nudgeDot = await this.waitForElementStyles(
          appId, ['xf-nudge', '#dot'], ['left']);
      if ((nudgeDot.renderedLeft ?? 0) < 0) {
        return pending(caller, 'Wait nudge to appear');
      }

      const actualText =
          await this.waitForElement(appId, ['xf-nudge', '#text']);
      console.info(actualText);
      chrome.test.assertEq(actualText.text, expectedText);

      return true;
    });
  }

  /**
   * Waits for the <xf-cloud-panel> element to be visible on the DOM.
   * @param appId app window ID
   */
  async waitForCloudPanelVisible(appId: string) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const styles = await this.waitForElementStyles(
          appId, ['xf-cloud-panel', 'cr-action-menu', 'dialog'], ['left']);

      if ((styles.renderedHeight ?? 0) > 0 && (styles.renderedWidth ?? 0) > 0 &&
          (styles.renderedTop ?? 0) > 0 && (styles.renderedLeft ?? 0) > 0) {
        return true;
      }

      return pending(caller, `Waiting for xf-cloud-panel to appear.`);
    });
  }

  /**
   * Waits for the underlying bulk pinning manager to enter the specified stage.
   * @param want The stage the bulk pinning is expected to be in. This is a
   *     string relating to the stage defined in the `PinningManager`.
   */
  async waitForBulkPinningStage(want: string) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const currentStage = await sendTestMessage({name: 'getBulkPinningStage'});
      if (currentStage === want) {
        return true;
      }
      return pending(caller, `Still waiting for syncing stage: ${want}`);
    });
  }

  /**
   * Waits until the pin manager has the expected required space.
   */
  async waitForBulkPinningRequiredSpace(want: number) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const actualRequiredSpace =
          await sendTestMessage({name: 'getBulkPinningRequiredSpace'});
      const parsedSpace = parseInt(actualRequiredSpace, 10);
      if (parsedSpace === want) {
        return true;
      }
      return pending(caller, `Still waiting for required space to be ${want}`);
    });
  }

  /**
   * Waits until the cloud panel has the specified item and percentage
   * attributes defined, if the `timeoutSeconds` is supplied it will only wait
   * for the specified time before timing out.
   * @param appId app window ID
   * @param items The items expected on the cloud panel.
   * @param percentage The percentage integer expected on the cloud panel.
   * @param timeoutSeconds Whether to timeout when verifying the panel
   *     attributes.
   */
  async waitForCloudPanelState(
      appId: string, items: number, percentage: number,
      timeoutSeconds: number = 10) {
    const futureDate = new Date();
    futureDate.setSeconds(futureDate.getSeconds() + timeoutSeconds);
    const caller = getCaller();
    return repeatUntil(async () => {
      chrome.test.assertTrue(
          new Date() < futureDate,
          `Timed out waiting for items=${items} and percentage=${
              percentage} to appear on xf-cloud-panel`);
      const cloudPanel = await this.queryElements(
          appId,
          [`xf-cloud-panel[percentage="${percentage}"][items="${items}"]`]);
      if (cloudPanel && cloudPanel.length === 1) {
        return true;
      }
      return pending(
          caller,
          `Still waiting for xf-cloud-panel to have items=${
              items} and percentage=${percentage}`);
    });
  }

  /**
   * Waits for the feedback panel to show an item with the provided messages.
   * @param appId app window ID
   * @param expectedPrimaryMessageRegex The expected primary-text of the item.
   * @param expectedSecondaryMessageRegex The expected secondary-text of the
   *     item.
   */
  async waitForFeedbackPanelItem(
      appId: string, expectedPrimaryMessageRegex: RegExp,
      expectedSecondaryMessageRegex: RegExp) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const element = await this.waitForElement(
          appId, ['#progress-panel', 'xf-panel-item']);

      const actualPrimaryText = element.attributes['primary-text'] ?? '';
      const actualSecondaryText = element.attributes['secondary-text'] ?? '';

      if (expectedPrimaryMessageRegex.test(actualPrimaryText) &&
          expectedSecondaryMessageRegex.test(actualSecondaryText)) {
        return;
      }
      return pending(
          caller,
          `Expected feedback panel item with primary-text regex:"${
              expectedPrimaryMessageRegex}" and secondary-text regex:"${
              expectedSecondaryMessageRegex}", got item with primary-text "${
              actualPrimaryText}" and secondary-text "${actualSecondaryText}"`);
    });
  }

  /**
   * Clicks the enabled and visible move to trash button and ensures the delete
   * button is hidden.
   */
  async clickTrashButton(appId: string) {
    await this.waitForElement(appId, '#delete-button[hidden]');
    await this.waitAndClickElement(
        appId, '#move-to-trash-button:not([hidden]):not([disabled])');
  }

  /** Fakes the response from spaced when it retrieves the free space. */
  async setSpacedFreeSpace(freeSpace: bigint) {
    console.info(freeSpace);
    await sendTestMessage(
        {name: 'setSpacedFreeSpace', freeSpace: String(freeSpace)});
  }

  /**
   * Waits for the specified element appearing in the DOM. `query_jelly` or
   * `query_old` are used depending on the state of the migration to
   * cros_components.
   * @param appId App window Id.
   * @param queryJelly Used when cros_components are used. See `waitForElement`
   *     for details.
   * @param queryOld Used when cros_components are not used. See
   *     `waitForElement` for details.
   */
  async waitForElementJelly(
      appId: string, queryJelly: string|string[],
      queryOld: string|string[]): Promise<ElementObject> {
    const isJellybean = await this.isCrosComponents(appId);
    return this.waitForElement(appId, isJellybean ? queryJelly : queryOld);
  }

  /**
   * Shorthand for clicking the appropriate element, depending the state of
   * the Jellybean experiment.
   * @param appId App window Id.
   * @param queryJelly The query when using cros_components. See
   *     `waitAndClickElement` for details.
   * @param queryOld The query when not using cros_components. See
   *     `waitAndClickElement` for details.
   */
  async waitAndClickElementJelly(
      appId: string, queryJelly: string|string[], queryOld: string|string[],
      keyModifiers?: KeyModifiers): Promise<ElementObject> {
    const isJellybean = await this.isCrosComponents(appId);
    return await this.waitAndClickElement(
        appId, isJellybean ? queryJelly : queryOld, keyModifiers);
  }

  /** Sets the pooled storage quota on Drive volume. */
  async setPooledStorageQuotaUsage(
      usedUserBytes: number, totalUserBytes: number,
      organizationLimitExceeded: boolean) {
    return sendTestMessage({
      name: 'setPooledStorageQuotaUsage',
      usedUserBytes,
      totalUserBytes,
      organizationLimitExceeded,
    });
  }

  /**
   * Opens a Files app's main window.
   * @param initialRoot Root path to be used as a default current directory
   *     during initialization. Can be null, for no default path.
   * @param appState App state to be passed with on opening the Files app.
   * @return Promise to be fulfilled after window creating.
   */
  async openNewWindow(initialRoot: null|string, appState?: null|FilesAppState):
      Promise<string> {
    appState = appState ?? {};
    if (initialRoot) {
      const tail = `external${initialRoot}`;
      appState.currentDirectoryURL = `filesystem:${this.origin_}/${tail}`;
    }

    const launchDir = appState ? appState.currentDirectoryURL : undefined;
    const type = appState ? appState.type : undefined;
    const volumeFilter = appState ? appState.volumeFilter : undefined;
    const searchQuery = appState ? appState.searchQuery : undefined;
    const appId = await sendTestMessage({
      name: 'launchFileManager',
      launchDir,
      type,
      volumeFilter,
      searchQuery,
    });

    return appId;
  }

  /**
   * Opens a file dialog and waits for closing it.
   * @param dialogParams Dialog parameters to be passed to
   *     openEntryChoosingWindow() function.
   * @param volumeType Volume icon type passed to the directory page object's
   *     selectItemByType function.
   * @param expectedSet Expected set of the entries.
   * @param closeDialog Function to close the dialog.
   * @param useBrowserOpen Whether to launch the select file dialog via a
   *     browser OpenFile() call.
   * @param debug Whether to debug the waitForWindow().
   * @return Promise to be fulfilled with the result entry of the dialog.
   */
  async openAndWaitForClosingDialog(
      dialogParams: chrome.fileSystem.ChooseEntryOptions, volumeType: string,
      expectedSet: TestEntryInfo[], closeDialog: (a: string) => Promise<void>,
      useBrowserOpen: boolean = false,
      debug: boolean = false): Promise<unknown> {
    const caller = getCaller();
    let resultPromise;
    if (useBrowserOpen) {
      await sendTestMessage({name: 'runSelectFileDialog'});
      resultPromise = async () => {
        return await sendTestMessage(
            {name: 'waitForSelectFileDialogNavigation'});
      };
    } else {
      await openEntryChoosingWindow(dialogParams);
      resultPromise = () => {
        return pollForChosenEntry(caller);
      };
    }

    const appId = await this.waitForWindow(debug);
    await this.waitForElement(appId, '#file-list');
    await this.waitFor('isFileManagerLoaded', appId, true);
    const directoryTree = await DirectoryTreePageObject.create(appId);
    await directoryTree.selectItemByType(volumeType);
    await this.waitForFiles(appId, TestEntryInfo.getExpectedRows(expectedSet));
    await closeDialog(appId);
    await repeatUntil(async () => {
      const windows = await this.getWindows();
      if (windows[appId] !== appId) {
        return;
      }
      return pending(caller, 'Waiting for Window %s to hide.', appId);
    });
    return await resultPromise();
  }

  /**
   * Opens a Files app's main window and waits until it is initialized. Fills
   * the window with initial files. Should be called for the first window only.
   * @param initialRoot Root path to be used as a default current directory
   *     during initialization. Can be null, for no default path.
   * @param initialLocalEntries List of initial entries to load in Downloads
   *     (defaults to a basic entry set).
   * @param initialDriveEntries List of initial entries to load in Google Drive
   *     (defaults to a basic entry set).
   * @param appState App state to be passed with on opening the Files app.
   * @return Promise to be fulfilled with the window ID.
   */
  async setupAndWaitUntilReady(
      initialRoot: null|string,
      initialLocalEntries: TestEntryInfo[] = BASIC_LOCAL_ENTRY_SET,
      initialDriveEntries: TestEntryInfo[] = BASIC_DRIVE_ENTRY_SET,
      appState?: FilesAppState): Promise<string> {
    const localEntriesPromise = addEntries(['local'], initialLocalEntries);
    const driveEntriesPromise = addEntries(['drive'], initialDriveEntries);

    const appId = await this.openNewWindow(initialRoot, appState);
    await this.waitForElement(appId, '#detail-table');

    // Wait until the elements are loaded in the table.
    await Promise.all([
      this.waitForFileListChange(appId, 0),
      localEntriesPromise,
      driveEntriesPromise,
    ]);
    await this.waitFor('isFileManagerLoaded', appId, true);
    return appId;
  }

  /**
   * Creates a folder shortcut to |directoryName| using the context menu. Note
   * the current directory must be a parent of the given |directoryName|.
   *
   * @param appId Files app windowId.
   * @param directoryName Directory of shortcut to be created.
   * @return Promise fulfilled on success.
   */
  async createShortcut(appId: string, directoryName: string): Promise<void> {
    await this.waitUntilSelected(appId, directoryName);

    await this.waitForElement(appId, ['.table-row[selected]']);
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'fakeMouseRightClick', appId, ['.table-row[selected]']));

    await this.waitForElement(appId, '#file-context-menu:not([hidden])');
    await this.waitForElement(
        appId, '[command="#pin-folder"]:not([hidden]):not([disabled])');
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'fakeMouseClick', appId,
        ['[command="#pin-folder"]:not([hidden]):not([disabled])']));

    const directoryTree = await DirectoryTreePageObject.create(appId);
    await directoryTree.waitForShortcutItemByLabel(directoryName);
  }

  /**
   * Mounts crostini volume by clicking on the fake crostini root.
   * @param appId Files app windowId.
   * @param initialEntries List of initial entries to load in Crostini (defaults
   *     to a basic entry set).
   */
  async mountCrostini(
      appId: string,
      initialEntries: TestEntryInfo[] = BASIC_CROSTINI_ENTRY_SET) {
    const directoryTree = await DirectoryTreePageObject.create(appId);

    // Add entries to crostini volume, but do not mount.
    await addEntries(['crostini'], initialEntries);

    // Linux files fake root is shown.
    await directoryTree.waitForPlaceholderItemByType('crostini');

    // Mount crostini, and ensure real root and files are shown.
    await directoryTree.selectPlaceholderItemByType('crostini');
    await directoryTree.waitForItemByType('crostini');
    const files = TestEntryInfo.getExpectedRows(initialEntries);
    await this.waitForFiles(appId, files);
  }

  /**
   * Registers a GuestOS, mounts the volume, and populates it with tbe specified
   * entries.
   * @param appId Files app windowId.
   * @param initialEntries List of initial entries to load in the volume.
   */
  async mountGuestOs(appId: string, initialEntries: TestEntryInfo[]) {
    await sendTestMessage({
      name: 'registerMountableGuest',
      displayName: 'Bluejohn',
      canMount: true,
      vmType: 'bruschetta',
    });
    const directoryTree = await DirectoryTreePageObject.create(appId);

    // Wait for the GuestOS fake root then click it.
    await directoryTree.selectPlaceholderItemByType('bruschetta');

    // Wait for the volume to get mounted.
    await directoryTree.waitForItemByType('bruschetta');

    // Add entries to GuestOS volume
    await addEntries(['guest_os_0'], initialEntries);

    // Ensure real root and files are shown.
    const files = TestEntryInfo.getExpectedRows(initialEntries);
    await this.waitForFiles(appId, files);
  }

  /**
   * Returns true if the SinglePartitionFormat flag is on.
   * @param appId Files app windowId.
   */
  async isSinglePartitionFormat(appId: string) {
    const dialog =
        await this.waitForElement(appId, ['files-format-dialog', 'cr-dialog']);
    const flag = dialog.attributes['single-partition-format'] || '';
    return !!flag;
  }

  /**
   * Shows hidden files to facilitate tests again the .Trash directory.
   */
  async showHiddenFiles(appId: string, check: boolean = true) {
    // Open the gear menu by clicking the gear button.
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#gear-button']));

    // Wait for menu to not be hidden.
    await this.waitForElement(appId, '#gear-menu:not([hidden])');

    // Wait for menu item to appear.
    await this.waitForElement(
        appId,
        `#gear-menu-toggle-hidden-files:not([disabled])${
            check ? ':not([checked])' : '[checked]'}`);

    // Click the menu item.
    await this.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files']);
  }

  /**
   * Sets the local user files policies to enable migration to `provider`.
   * @param provider Where local files should be moved. One of
   *     microsoft_onedrive, google_drive.
   */
  async setLocalFilesMigrationDestination(provider: string) {
    // Disable local storage - migration destination is ignored otherwise.
    await sendTestMessage({name: 'setLocalFilesEnabled', enabled: false});
    // Set the destination.
    await sendTestMessage({
      name: 'setLocalFilesMigrationDestination',
      provider: provider,
    });
  }
}
