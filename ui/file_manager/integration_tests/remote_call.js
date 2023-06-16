// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ElementObject} from './element_object.js';
import {KeyModifiers} from './key_modifiers.js';
import {getCaller, pending, repeatUntil, sendTestMessage} from './test_util.js';
import {VolumeManagerCommonVolumeType} from './volume_manager_common_volume_type.js';

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
  constructor(message) {
    super(message);
    this.name = 'ExecuteScriptError';
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
export class RemoteCall {
  /**
   * @param {string} origin ID of the app to be manipulated.
   */
  constructor(origin) {
    this.origin_ = origin;

    /**
     * Tristate holding the cached result of isStepByStepEnabled_().
     * @type {?boolean}
     */
    this.cachedStepByStepEnabled_ = null;
  }

  /**
   * Checks whether step by step tests are enabled or not.
   * @private
   * @return {!Promise<boolean>}
   */
  async isStepByStepEnabled_() {
    if (this.cachedStepByStepEnabled_ === null) {
      this.cachedStepByStepEnabled_ = await new Promise((fulfill) => {
        chrome.commandLinePrivate.hasSwitch(
            'enable-file-manager-step-by-step-tests', fulfill);
      });
    }
    return this.cachedStepByStepEnabled_;
  }

  /**
   * Sends a test |message| to the test code running in the File Manager.
   * @param {!Object} message
   * @return {!Promise<*>} A promise which when fulfilled returns the
   *     result of executing test code with the given message.
   */
  sendMessage(message) {
    return new Promise((fulfill) => {
      chrome.runtime.sendMessage(this.origin_, message, {}, fulfill);
    });
  }

  /**
   * Calls a remote test util in the Files app's extension. See:
   * registerRemoteTestUtils in test_util_base.js.
   *
   * @param {string} func Function name.
   * @param {?string} appId App window Id or null for functions not requiring a
   *     window.
   * @param {?Array<*>=} args Array of arguments.
   * @param {function(*)=} opt_callback Callback handling the function's result.
   * @return {!Promise} Promise to be fulfilled with the result of the remote
   *     utility.
   */
  async callRemoteTestUtil(func, appId, args, opt_callback) {
    const stepByStep = await this.isStepByStepEnabled_();
    let finishCurrentStep;
    if (stepByStep) {
      while (window.currentStep) {
        await window.currentStep;
      }
      window.currentStep = new Promise(resolve => {
        finishCurrentStep = () => {
          window.currentStep = null;
          resolve();
        };
      });
      console.info('Executing: ' + func + ' on ' + appId + ' with args: ');
      console.info(args);
      if (window.autostep !== true) {
        await new Promise((onFulfilled) => {
          console.info('Type step() to continue...');
          /** @type {?function()} */
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
      finishCurrentStep();
    }
    if (opt_callback) {
      opt_callback(response);
    }
    return response;
  }

  /**
   * Wait for a SWA window to be open.
   * @param {boolean=} debug Whether to debug the findSwaWindow.
   * @return {!Promise<string>}
   */
  async waitForWindow(debug = false) {
    const caller = getCaller();
    const appId = await repeatUntil(async () => {
      const msg = {name: 'findSwaWindow'};
      if (debug) {
        msg['debug'] = true;
      }
      const ret = await sendTestMessage(msg);
      if (ret === 'none') {
        return pending(caller, 'Wait for SWA window');
      }
      return ret;
    });

    return appId;
  }

  /**
   * Waits until the window turns to the given size.
   * @param {string} appId App window Id.
   * @param {number} width Requested width in pixels.
   * @param {number} height Requested height in pixels.
   */
  waitForWindowGeometry(appId, width, height) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      if (!windows[appId]) {
        return pending(caller, 'Window %s is not found.', appId);
      }
      if (windows[appId].outerWidth !== width ||
          windows[appId].outerHeight !== height) {
        return pending(
            caller, 'Expected window size is %j, but it is %j',
            {width: width, height: height}, windows[appId]);
      }
    });
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @return {Promise<ElementObject>} Promise to be fulfilled when the element
   *     appears.
   */
  waitForElement(appId, query) {
    return this.waitForElementStyles(appId, query, []);
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {!Array<string>} styleNames List of CSS property name to be
   *     obtained. NOTE: Causes element style re-calculation.
   * @return {Promise<ElementObject>} Promise to be fulfilled when the element
   *     appears.
   */
  waitForElementStyles(appId, query, styleNames) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements = await this.callRemoteTestUtil(
          'deepQueryAllElements', appId, [query, styleNames]);
      if (elements && elements.length > 0) {
        return /** @type {ElementObject} */ (elements[0]);
      }
      return pending(caller, 'Element %s is not found.', query);
    });
  }

  /**
   * Waits for a remote test function to return a specific result.
   *
   * @param {string} funcName Name of remote test function to be executed.
   * @param {?string} appId App window Id.
   * @param {function(Object):boolean|boolean|Object} expectedResult An value to
   *     be checked against the return value of |funcName| or a callback that
   *     receives the return value of |funcName| and returns true if the result
   *     is the expected value.
   * @param {?Array<*>=} args Arguments to be provided to |funcName| when
   *     executing it.
   * @return {Promise} Promise to be fulfilled when the |expectedResult| is
   *     returned from |funcName| execution.
   */
  waitFor(funcName, appId, expectedResult, args) {
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
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @return {Promise} Promise to be fulfilled when the element is lost.
   */
  waitForElementLost(appId, query) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements =
          await this.callRemoteTestUtil('deepQueryAllElements', appId, [query]);
      if (elements.length > 0) {
        return pending(caller, 'Elements %j still exists.', elements);
      }
      return true;
    });
  }

  /**
   * Wait for the |query| to match |count| elements.
   *
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {number} count The expected element match count.
   * @return {Promise} Promise to be fulfilled on success.
   */
  waitForElementsCount(appId, query, count) {
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
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {string} key DOM UI Events Key value.
   * @param {boolean} ctrlKey Control key flag.
   * @param {boolean} shiftKey Shift key flag.
   * @param {boolean} altKey Alt key flag.
   * @return {Promise} Promise to be fulfilled or rejected depending on the
   *     result.
   */
  async fakeKeyDown(appId, query, key, ctrlKey, shiftKey, altKey) {
    const result = await this.callRemoteTestUtil(
        'fakeKeyDown', appId, [query, key, ctrlKey, shiftKey, altKey]);
    if (result) {
      return true;
    } else {
      throw new Error('Fail to fake key down.');
    }
  }

  /**
   * Sets the given input text on the element identified by the query.
   * @param {string} appId App window ID.
   * @param {string|!Array<string>} selector The query selector to locate
   *     the element
   * @param {string} text The text to be set on the element.
   */
  async inputText(appId, selector, text) {
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('inputText', appId, [selector, text]));
  }

  /**
   * Gets file entries just under the volume.
   *
   * @param {VolumeManagerCommonVolumeType} volumeType Volume type.
   * @param {Array<string>} names File name list.
   * @return {Promise} Promise to be fulfilled with file entries or rejected
   *     depending on the result.
   */
  getFilesUnderVolume(volumeType, names) {
    return this.callRemoteTestUtil(
        'getFilesUnderVolume', null, [volumeType, names]);
  }

  /**
   * Waits for a single file.
   * @param {VolumeManagerCommonVolumeType} volumeType Volume type.
   * @param {string} name File name.
   * @return {!Promise} Promise to be fulfilled when the file had found.
   */
  waitForAFile(volumeType, name) {
    const caller = getCaller();
    return repeatUntil(async () => {
      if ((await this.getFilesUnderVolume(volumeType, [name])).length === 1) {
        return true;
      }
      return pending(caller, '"' + name + '" is not found.');
    });
  }

  /**
   * Shorthand for clicking an element.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {KeyModifiers=} opt_keyModifiers Object
   * @return {Promise} Promise to be fulfilled with the clicked element.
   */
  async waitAndClickElement(appId, query, opt_keyModifiers) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil(
        'fakeMouseClick', appId, [query, opt_keyModifiers]);
    chrome.test.assertTrue(result, 'mouse click failed.');
    return element;
  }

  /**
   * Shorthand for right-clicking an element.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {KeyModifiers=} opt_keyModifiers Object
   * @return {Promise} Promise to be fulfilled with the clicked element.
   */
  async waitAndRightClick(appId, query, opt_keyModifiers) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil(
        'fakeMouseRightClick', appId, [query, opt_keyModifiers]);
    chrome.test.assertTrue(result, 'mouse right-click failed.');
    return element;
  }

  /**
   * Shorthand for focusing an element.
   * @param {string} appId App window Id.
   * @param {!Array<string>} query Query to specify the element to be focused.
   * @return {Promise} Promise to be fulfilled with the focused element.
   */
  async focus(appId, query) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil('focus', appId, query);
    chrome.test.assertTrue(result, 'focus failed.');
    return element;
  }

  /**
   * Simulate Click in the UI in the middle of the element.
   * @param {string} appId App window ID contains the element. NOTE: The click
   *     is
   * simulated on most recent window in the window system.
   * @param {string|!Array<string>} query Query to the element to be clicked.
   * @param {boolean} leftClick If true, simulate left click. Otherwise simulate
   *     right click.
   * @return {!Promise} A promise fulfilled after the click event.
   */
  async simulateUiClick(appId, query, leftClick = true) {
    const element = /* @type {!Object} */ (
        await this.waitForElementStyles(appId, query, ['display']));
    chrome.test.assertTrue(!!element, 'element for simulateUiClick not found');

    // Find the middle of the element.
    const x =
        Math.floor(element['renderedLeft'] + (element['renderedWidth'] / 2));
    const y =
        Math.floor(element['renderedTop'] + (element['renderedHeight'] / 2));

    return sendTestMessage(
        {appId, name: 'simulateClick', 'clickX': x, 'clickY': y, leftClick});
  }

  /**
   * Simulate Right Click in blank/empty space of the file list element.
   * @param{string} appId App window ID contains the element. NOTE: The click is
   * simulated on most recent window in the window system.
   * @return {!Promise} A promise fulfilled after the click event.
   */
  async rightClickFileListBlankSpace(appId) {
    await this.simulateUiClick(
        appId, '#file-list .spacer.signals-overscroll', false);
  }

  /**
   * Selects the option given by the index in the menu given by the type. This
   * only works in V2 version of the search.
   *
   * @param {string} appId The ID that identifies the files app.
   * @param {string} type The search option type (location, recency, type).
   * @param {number} index The index of the button.
   * @return {Promise<boolean>} A promise that resolves to true if click was
   *     successful and false otherwise.
   */
  selectSearchOption(appId, type, index) {
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
   * Sends a test |message| to the test code running in the File Manager.
   * @param {!Object} message
   * @return {!Promise<*>}
   * @override
   */
  sendMessage(message) {
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
            fulfill(response == '' ? true : JSON.parse(response));
          } catch (e) {
            console.error(`Failed to parse "${response}" due to ${e}`);
            fulfill(false);
          }
        }
      });
    });
  }

  async getWindows() {
    return JSON.parse(await sendTestMessage({name: 'getWindows'}));
  }

  /**
   * Executes a script in the context of a <preview-tag> element contained in
   * the window.
   * For SWA: It's the first chrome-untrusted://file-manager <iframe>.
   * For legacy: It's the first elements based on the `query`.
   * Responds with its output.
   *
   * @param {string} appId App window Id.
   * @param {!Array<string>} query Query to the <preview-tag> element (this is
   *     ignored for SWA).
   * @param {string} statement Javascript statement to be executed within the
   *     <preview-tag>.
   * @return {!Promise<*>} resolved with the return value of the `statement`.
   */
  async executeJsInPreviewTag(appId, query, statement) {
    return this.executeJsInPreviewTagSwa_(statement);
  }

  /**
   * Inject javascript statemenent in the first chrome-untrusted://file-manager
   * page found and respond with its output.
   * @private
   * @param {string} statement
   * @return {!Promise}
   */
  async executeJsInPreviewTagSwa_(statement) {
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

    const response = await sendTestMessage(command);
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
   * @param {string} expectedUrl
   * @return {!Promise} Promise to be fulfilled when the expected URL is shown
   *     in a browser window.
   */
  async waitForLastOpenedBrowserTabUrl(expectedUrl) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const command = {name: 'getLastActiveTabURL'};
      const activeBrowserTabURL = await sendTestMessage(command);
      if (activeBrowserTabURL !== expectedUrl) {
        return pending(
            caller, 'waitForActiveBrowserTabUrl: expected %j actual %j.',
            expectedUrl, activeBrowserTabURL);
      }
    });
  }

  /**
   * Returns whether a window exists with the expected origin.
   * @param {string} expectedOrigin
   * @return {!Promise<boolean>} Promise resolved with true or false depending
   *     on whether such window exists.
   */
  async windowOriginExists(expectedOrigin) {
    const command = {name: 'expectWindowOrigin', expectedOrigin};
    const windowExists = await sendTestMessage(command);
    return windowExists == 'true';
  }

  /**
   * Waits for the file list turns to the given contents.
   * @param {string} appId App window Id.
   * @param {Array<Array<string>>} expected Expected contents of file list.
   * @param {{orderCheck:(?boolean|undefined), ignoreFileSize:
   *     (?boolean|undefined), ignoreLastModifiedTime:(?boolean|undefined)}=}
   *     opt_options Options of the comparison. If orderCheck is true, it also
   *     compares the order of files. If ignoreLastModifiedTime is true, it
   *     compares the file without its last modified time.
   * @return {Promise} Promise to be fulfilled when the file list turns to the
   *     given contents.
   */
  waitForFiles(appId, expected, opt_options) {
    const options = opt_options || {};
    const caller = getCaller();
    return repeatUntil(async () => {
      const files = await this.callRemoteTestUtil('getFileList', appId, []);
      if (!options.orderCheck) {
        files.sort();
        expected.sort();
      }
      for (let i = 0; i < Math.min(files.length, expected.length); i++) {
        // Change the value received from the UI to match when comparing.
        if (options.ignoreFileSize) {
          files[i][1] = expected[i][1];
        }
        if (options.ignoreLastModifiedTime) {
          if (expected[i].length < 4) {
            // expected sometimes doesn't include the modified time at all, so
            // just remove from the data from UI.
            files[i].splice(3, 1);
          } else {
            files[i][3] = expected[i][3];
          }
        }
      }
      if (!chrome.test.checkDeepEq(expected, files)) {
        return pending(
            caller, 'waitForFiles: expected: %j actual %j.', expected, files);
      }
    });
  }

  /**
   * Waits until the number of files in the file list is changed from the given
   * number.
   * TODO(hirono): Remove the function.
   *
   * @param {string} appId App window Id.
   * @param {number} lengthBefore Number of items visible before.
   * @return {Promise} Promise to be fulfilled with the contents of files.
   */
  waitForFileListChange(appId, lengthBefore) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const files = await this.callRemoteTestUtil('getFileList', appId, []);
      files.sort();

      const notReadyRows =
          files.filter((row) => row.filter((cell) => cell == '...').length);

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
   * @param {string} appId App window Id.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor Task to
   *     watch.
   * @param {!Array<string>} fileNames Name of files that should have been
   *     passed to the executeTasks().
   * @param {Array<Object>=} replyArgs arguments to reply to executed task.
   * @return {Promise} Promise to be fulfilled when the task appears in the
   *     executed task list.
   */
  waitUntilTaskExecutes(appId, descriptor, fileNames, replyArgs) {
    const caller = getCaller();
    return repeatUntil(async () => {
      if (!await this.callRemoteTestUtil(
              'taskWasExecuted', appId, [descriptor, fileNames])) {
        const tasks =
            await this.callRemoteTestUtil('getExecutedTasks', appId, []);
        const executedTasks = tasks.map((task) => {
          const {appId, taskType, actionId} = task.descriptor;
          const executedFileNames = task['fileNames'];
          return `${appId}|${taskType}|${actionId} for ${
              JSON.stringify(executedFileNames)}`;
        });
        return pending(caller, 'Executed task is %j', executedTasks);
      }
      if (replyArgs) {
        await this.callRemoteTestUtil(
            'replyExecutedTask', appId, [descriptor, replyArgs]);
      }
    });
  }

  /**
   * Check if the next tabforcus'd element has the given ID or not.
   * @param {string} appId App window Id.
   * @param {string} elementId String of 'id' attribute which the next
   *     tabfocus'd element should have.
   * @return {Promise} Promise to be fulfilled with the result.
   */
  async checkNextTabFocus(appId, elementId) {
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'Tab key dispatch failure');

    const caller = getCaller();
    return repeatUntil(async () => {
      let element =
          await this.callRemoteTestUtil('getActiveElement', appId, []);
      if (element && element.attributes['id'] === elementId) {
        return true;
      }
      // Try to check the shadow root.
      element =
          await this.callRemoteTestUtil('deepGetActiveElement', appId, []);
      if (element && element.attributes['id'] === elementId) {
        return true;
      }
      return pending(
          caller,
          'Waiting for active element with id: "' + elementId +
              '", but current is: "' + element.attributes['id'] + '"');
    });
  }

  /**
   * Returns a promise that repeatedly checks for a file with the given
   * name to be selected in the app window with the given ID. Typical
   * use
   *
   * await remoteCall.waitUntilSelected('file#0', 'hello.txt');
   * ... // either the test timed out or hello.txt is currently selected.
   *
   * @param {string} appId App window Id.
   * @param {string} fileName the name of the file to be selected.
   * @return {Promise<boolean>} Promise that indicates if selection was
   *     successful.
   */
  waitUntilSelected(appId, fileName) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const selected =
          await this.callRemoteTestUtil('selectFile', appId, [fileName]);
      if (!selected) {
        return pending(caller, `File ${fileName} not yet selected`);
      }
    });
  }

  /**
   * Waits until the current directory is changed.
   * @param {string} appId App window Id.
   * @param {string} expectedPath Path to be changed to.
   * @return {Promise} Promise to be fulfilled when the current directory is
   *     changed to expectedPath.
   */
  waitUntilCurrentDirectoryIsChanged(appId, expectedPath) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const path =
          await this.callRemoteTestUtil('getBreadcrumbPath', appId, []);
      if (path !== expectedPath) {
        return pending(
            caller, 'Expected path is %s got %s', expectedPath, path);
      }
    });
  }

  /**
   * Expands tree item.
   * @param {string} appId App window Id.
   * @param {string} query Query to the <tree-item> element.
   */
  async expandTreeItemInDirectoryTree(appId, query) {
    await this.waitForElement(appId, query);
    const elements = await this.callRemoteTestUtil(
        'queryAllElements', appId, [`${query}[expanded]`]);
    // If it's already expanded just set the focus on directory tree.
    if (elements.length > 0) {
      return this.callRemoteTestUtil('focus', appId, ['#directory-tree']);
    }

    // We must wait until <tree-item> has attribute [has-children=true]
    // otherwise it won't expand. We must also to account for the case
    // :not([expanded]) to ensure it has NOT been expanded by some async
    // operation since the [expanded] checks above.
    const expandIcon =
        query + ':not([expanded]) > .tree-row[has-children=true] .expand-icon';
    await this.waitAndClickElement(appId, expandIcon);
    // Wait for the expansion to finish.
    await this.waitForElement(appId, query + '[expanded]');
    // Force the focus on directory tree.
    await this.callRemoteTestUtil('focus', appId, ['#directory-tree']);
  }

  /**
   * Expands directory tree for specified path.
   */
  expandDirectoryTreeFor(appId, path, volumeType = 'downloads') {
    return this.expandDirectoryTreeForInternal_(
        appId, path.split('/'), 0, volumeType);
  }

  /**
   * Internal function for expanding directory tree for specified path.
   */
  async expandDirectoryTreeForInternal_(appId, components, index, volumeType) {
    if (index >= components.length - 1) {
      return;
    }

    // First time we should expand the root/volume first.
    if (index === 0) {
      await this.expandVolumeInDirectoryTree(appId, volumeType);
      return this.expandDirectoryTreeForInternal_(
          appId, components, index + 1, volumeType);
    }
    const path = '/' + components.slice(1, index + 1).join('/');
    await this.expandTreeItemInDirectoryTree(
        appId, `[full-path-for-testing="${path}"]`);
    await this.expandDirectoryTreeForInternal_(
        appId, components, index + 1, volumeType);
  }

  /**
   * Expands download volume in directory tree.
   */
  expandDownloadVolumeInDirectoryTree(appId) {
    return this.expandVolumeInDirectoryTree(appId, 'downloads');
  }

  /**
   * Expands download volume in directory tree.
   */
  expandVolumeInDirectoryTree(appId, volumeType) {
    return this.expandTreeItemInDirectoryTree(
        appId, `[volume-type-for-testing="${volumeType}"]`);
  }

  /**
   * Wait until the expected number of volumes is mounted.
   * @param {number} expectedVolumesCount Expected number of mounted volumes.
   * @return {Promise} promise Promise to be fulfilled.
   */
  async waitForVolumesCount(expectedVolumesCount) {
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
   * @param {string} appId App window Id
   * @param {string} bannerTagName Banner tag name in lowercase to isolate.
   */
  async isolateBannerForTesting(appId, bannerTagName) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'isolateBannerForTesting', appId, [bannerTagName]));
  }

  /**
   * Disables banners from attaching to the DOM.
   * @param {string} appId App window Id
   */
  async disableBannersForTesting(appId) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('disableBannersForTesting', appId, []));
  }

  /**
   * Sends text to the search box in the Files app.
   * @param {string} appId App window Id
   * @param {string} text The text to type in the search box.
   */
  async typeSearchText(appId, text) {
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
   * @param {string} appId
   * @return {!Promise<!Array<string>>} Array of the names in the auto complete
   *     list.
   */
  async waitForSearchAutoComplete(appId) {
    // Wait for the list to appear.
    await this.waitForElement(appId, '#autocomplete-list li');

    // Return the result.
    const elements = await this.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#autocomplete-list li']);
    return elements.map((element) => element.text);
  }

  /**
   * Disable nudges from expiring for testing.
   * @param {string} appId App window Id
   */
  async disableNudgeExpiry(appId) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('disableNudgeExpiry', appId, []));
  }

  /**
   * Selects the file and displays the context menu for the file.
   * @return {!Promise<void>} resolved when the context menu is visible.
   */
  async showContextMenuFor(appId, fileName) {
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
   * @param {string} appId App window Id.
   * @return {!Promise<void>}
   */
  async dismissMenu(appId) {
    await this.fakeKeyDown(appId, 'body', 'Escape', false, false, false);
  }

  /**
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to find the elements.
   * @return {!Promise<!Array<!ElementObject>>} Promise to be fulfilled with the
   *     elements.
   * @private
   */
  async queryElements_(appId, query) {
    if (typeof query === 'string') {
      query = [query];
    }
    return this.callRemoteTestUtil('deepQueryAllElements', appId, query);
  }

  /**
   * Returns the menu as ElementObject and its menu-items (including separators)
   * in the `items` property.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} menu The name of the menu.
   * @return {!Promise<undefined|!ElementObject>} Promise to be fulfilled with
   *     the menu.
   */
  async getMenu(appId, menu) {
    let menuId = '';
    // TODO: Implement for other menus.
    if (menu === 'context-menu') {
      menuId = '#file-context-menu';
    } else if (menu == 'tasks') {
      menuId = '#tasks-menu';
    }

    if (!menuId) {
      console.error(`Invalid menu '${menu}'`);
      return;
    }

    // Get the top level menu element.
    const menuElement = await this.waitForElement(appId, menuId);
    // Query all the menu items.
    menuElement.items = await this.queryElements_(appId, `${menuId} > *`);
    return menuElement;
  }

  /**
   * Displays the "tasks" menu from the "OPEN" button dropdown.
   * The caller code has to prepare the selection to have multiple tasks.
   * @param {string} appId App window Id.
   */
  async expandOpenDropdown(appId) {
    // Wait the OPEN button to have multiple tasks.
    await this.waitAndClickElement(appId, '#tasks[multiple]');
  }

  /**
   * Check if an item is pinned on drive or not.
   * @param {string} appId app window ID.
   * @param {string} path Path from the drive mount point, e.g. /root/test.txt
   * @param {boolean} status Pinned status to expect drive item to be.
   */
  async expectDriveItemPinnedStatus(appId, path, status) {
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
   * Send a delete event via the `OnFilesChanged` drivefs delegate method.
   * @param {string} appId app window ID.
   * @param {string} path Path from the drive mount point, e.g. /root/test.txt
   */
  async sendDriveCloudDeleteEvent(appId, path) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    await sendTestMessage({
      appId,
      name: 'sendDriveCloudDeleteEvent',
      path,
    });
  }

  /**
   * Whether the Jellybean UI is enabled.
   * @param {string} appId app window ID
   * @returns {Promise<boolean>}
   */
  async isJellybean(appId) {
    return await sendTestMessage({
             appId,
             name: 'isJellybean',
           }) === 'true';
  }

  /**
   * Wait for the nudge with the given text to be visible.
   *
   * @param {string} appId app window ID.
   * @param {string} expectedText Text that should be displayed in the Nudge.
   * @return {!Promise<boolean>}
   */
  async waitNudge(appId, expectedText) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const nudgeDot = await this.waitForElementStyles(
          appId, ['xf-nudge', '#dot'], ['left']);
      if (nudgeDot.renderedLeft < 0) {
        return pending(caller, 'Wait nudge to appear');
      }

      const actualText =
          await this.waitForElement(appId, ['xf-nudge', '#text']);
      console.log(actualText);
      chrome.test.assertEq(actualText.text, expectedText);

      return true;
    });
  }

  /**
   * Waits for the <xf-cloud-panel> element to be visible on the DOM.
   * @param {string} appId app window ID
   */
  async waitForCloudPanelVisible(appId) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const styles = await this.waitForElementStyles(
          appId, ['xf-cloud-panel', 'cr-action-menu', 'dialog'], ['left']);

      if (styles.renderedHeight > 0 && styles.renderedWidth > 0 &&
          styles.renderedTop > 0 && styles.renderedLeft > 0) {
        return true;
      }

      return pending(caller, `Waiting for xf-cloud-panel to appear.`);
    });
  }

  /**
   * Wait for the underlying bulk pinning manager to enter the specified stage.
   * @param {string} want The stage the bulk pinning is expected to be in. This
   *     is a string relating to the stage defined in the `PinManager`.
   */
  async waitForBulkPinningStage(want) {
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
   * Wait until the pin manager has the expected required space.
   * @param {number} want
   */
  async waitForBulkPinningRequiredSpace(want) {
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
   * Wait until the cloud panel has the specified item and percentage attributes
   * defined, if the `timeoutSeconds` is supplied it will only wait for the
   * specified time before timing out.
   * @param {string} appId app window ID
   * @param {number} items The items expected on the cloud panel.
   * @param {number} percentage The percentage integer expected on the cloud
   *     panel.
   * @param {number=} timeoutSeconds Whether to timeout when verifying the panel
   *     attributes.
   */
  async waitForCloudPanelState(appId, items, percentage, timeoutSeconds = 10) {
    const futureDate = new Date();
    futureDate.setSeconds(futureDate.getSeconds() + timeoutSeconds);
    const caller = getCaller();
    return repeatUntil(async () => {
      chrome.test.assertTrue(
          new Date() < futureDate,
          `Timed out waiting for items=${items} and percentage=${
              percentage} to appear on xf-cloud-panel`);
      const cloudPanel = await this.callRemoteTestUtil(
          'deepQueryAllElements', appId,
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
   * Clicks the enabled and visible move to trash button and ensures the delete
   * button is hidden.
   * @param {string} appId
   */
  async clickTrashButton(appId) {
    await this.waitForElement(appId, '#delete-button[hidden]');
    await this.waitAndClickElement(
        appId, '#move-to-trash-button:not([hidden]):not([disabled])');
  }

  /**
   * Fakes the response from spaced when it retrieves the free space.
   * @param {bigint} freeSpace
   */
  async setSpacedFreeSpace(freeSpace) {
    console.log(freeSpace);
    await sendTestMessage(
        {name: 'setSpacedFreeSpace', freeSpace: String(freeSpace)});
  }

  /**
   * Waits for the specified element appearing in the DOM. `query_jelly` or
   * `query_old` are used depending on the state of the migration to
   * cros_components.
   * @param  {string} appId App window Id.
   * @param {string|!Array<string>} query_jelly Used when cros_components are
   *     used. See `waitForElement` for details.
   * @param {string|!Array<string>} query_old Used when cros_components are not
   *     used. See `waitForElement` for details.
   * @returns {Promise<ElementObject>} Promise to be fulfilled when the
   *     element appears.
   */
  waitForElementJelly(appId, query_jelly, query_old) {
    return this.isJellybean(appId).then(
        isJellybean =>
            this.waitForElement(appId, isJellybean ? query_jelly : query_old));
  }

  /**
   * Shorthand for clicking the appropriate element, depending the state of the
   * Jellybean experiment.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query_jelly The query when using
   *     cros_components. See `waitAndClickElement` for details.
   * @param {string|!Array<string>} query_old The query when not using
   *     cros_components. See `waitAndClickElement` for details.
   * @param {KeyModifiers=} opt_keyModifiers Object
   * @return {Promise} Promise to be fulfilled with the clicked element.
   */
  async waitAndClickElementJelly(
      appId, query_jelly, query_old, opt_keyModifiers) {
    const isJellybean = await this.isJellybean(appId);
    return await this.waitAndClickElement(
        appId, isJellybean ? query_jelly : query_old, opt_keyModifiers);
  }
}
