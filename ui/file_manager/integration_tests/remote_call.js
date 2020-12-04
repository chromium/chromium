// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * When step by step tests are enabled, turns on automatic step() calls. Note
 * that if step() is defined at the time of this call, invoke it to start the
 * test auto-stepping ball rolling.
 */
function autoStep() {
  window.autostep = window.autostep || false;
  if (!window.autostep) {
    window.autostep = true;
  }
  if (window.autostep && typeof window.step == 'function') {
    window.step();
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
class RemoteCall {
  /**
   * @param {string} extensionId ID of extension to be manipulated.
   */
  constructor(extensionId) {
    this.extensionId_ = extensionId;

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
    const response = await new Promise((onFulfilled) => {
      chrome.runtime.sendMessage(
          this.extensionId_, {func: func, appId: appId, args: args}, {},
          onFulfilled);
    });

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
   * Waits until a window having the given ID prefix appears.
   * @param {string} windowIdPrefix ID prefix of the requested window.
   * @return {Promise} promise Promise to be fulfilled with a found window's ID.
   */
  waitForWindow(windowIdPrefix) {
    const caller = getCaller();
    const windowIdRegex = new RegExp(windowIdPrefix);
    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      for (const id in windows) {
        if (id.indexOf(windowIdPrefix) === 0 || windowIdRegex.test(id)) {
          return id;
        }
      }
      return pending(
          caller, 'Window with the prefix %s is not found.', windowIdPrefix);
    });
  }

  /**
   * Closes a window and waits until the window is closed.
   *
   * @param {string} appId App window Id.
   * @return {Promise} promise Promise to be fulfilled with the result (true:
   *     success, false: failed).
   */
  async closeWindowAndWait(appId) {
    const caller = getCaller();

    // Closes the window.
    if (!await this.callRemoteTestUtil('closeWindow', null, [appId])) {
      // Returns false when the closing is failed.
      return false;
    }

    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      for (const id in windows) {
        if (id === appId) {
          // Window is still available. Continues waiting.
          return pending(
              caller, 'Window with the prefix %s is not found.', appId);
        }
      }
      // Window is not available. Closing is done successfully.
      return true;
    });
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
      if (elements.length > 0) {
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
   * Gets file entries just under the volume.
   *
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
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
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
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
   * @param{string} appId App window ID contains the element. NOTE: The click is
   * simulated on most recent window in the window system.
   * @param {string|!Array<string>} query Query to the element to be clicked.
   * @return {!Promise} A promise fulfilled after the click event.
   */
  async simulateUiClick(appId, query) {
    const element = /* @type {!Object} */ (
        await this.waitForElementStyles(appId, query, ['display']));
    chrome.test.assertTrue(!!element, 'element for simulateUiClick not found');

    // Find the middle of the element.
    const x =
        Math.floor(element['renderedLeft'] + (element['renderedWidth'] / 2));
    const y =
        Math.floor(element['renderedTop'] + (element['renderedHeight'] / 2));

    return sendTestMessage({name: 'simulateClick', 'clickX': x, 'clickY': y});
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
class RemoteCallFilesApp extends RemoteCall {
  /**
   * Waits for the file list turns to the given contents.
   * @param {string} appId App window Id.
   * @param {Array<Array<string>>} expected Expected contents of file list.
   * @param {{orderCheck:(?boolean|undefined),
   *     ignoreLastModifiedTime:(?boolean|undefined)}=} opt_options Options of
   *     the comparison. If orderCheck is true, it also compares the order of
   *     files. If ignoreLastModifiedTime is true, it compares the file without
   *     its last modified time.
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
   * @param {string} taskId Task ID to watch.
   * @param {Array<Object>=} opt_replyArgs arguments to reply to executed task.
   * @return {Promise} Promise to be fulfilled when the task appears in the
   *     executed task list.
   */
  waitUntilTaskExecutes(appId, taskId, opt_replyArgs) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const executedTasks =
          await this.callRemoteTestUtil('getExecutedTasks', appId, []);
      if (executedTasks.indexOf(taskId) === -1) {
        return pending(caller, 'Executed task is %j', executedTasks);
      }
      if (opt_replyArgs) {
        await this.callRemoteTestUtil(
            'replyExecutedTask', appId, [taskId, opt_replyArgs]);
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
   * Navigates to specified directory on the specified volume by using directory
   * tree.
   * DEPRECATED: Use background.js:navigateWithDirectoryTree instead
   * crbug.com/996626.
   */
  async navigateWithDirectoryTree(
      appId, path, rootLabel, volumeType = 'downloads') {
    await this.expandDirectoryTreeFor(appId, path, volumeType);

    // Select target path.
    await this.callRemoteTestUtil(
        'fakeMouseClick', appId, [`[full-path-for-testing="${path}"]`]);

    // Entries within Drive starts with /root/ but it isn't displayed in the
    // breadcrubms used by waitUntilCurrentDirectoryIsChanged.
    path = path.replace(/^\/root/, '')
               .replace(/^\/team_drives/, '')
               .replace(/^\/Computers/, '');

    // TODO(lucmult): Remove this once MyFilesVolume is rolled out.
    // Remove /Downloads duplication when MyFilesVolume is enabled.
    if (volumeType == 'downloads' && path.startsWith('/Downloads') &&
        rootLabel.endsWith('/Downloads')) {
      rootLabel = rootLabel.replace('/Downloads', '');
    }

    // Wait until the Files app is navigated to the path.
    return this.waitUntilCurrentDirectoryIsChanged(
        appId, `/${rootLabel}${path}`);
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
class RemoteCallGallery extends RemoteCall {
  /**
   * Waits until the expected image is shown.
   *
   * @param {string} appId App window Id.
   * @param {number} width Expected width of the image.
   * @param {number} height Expected height of the image.
   * @param {string|null} name Expected name of the image.
   * @return {Promise} Promsie to be fulfilled when the check is passed.
   */
  waitForSlideImage(appId, width, height, name) {
    const expected = {};
    if (width) {
      expected.width = width;
    }
    if (height) {
      expected.height = height;
    }
    if (name) {
      expected.name = name;
    }

    const caller = getCaller();
    return repeatUntil(async () => {
      const query = '.gallery[mode="slide"] .image-container > .image';
      const [nameBox, image] = await Promise.all([
        this.waitForElement(appId, '#rename-input'),
        this.waitForElementStyles(appId, query, ['any'])
      ]);
      const actual = {};
      if (width && image) {
        actual.width = image.imageWidth;
      }
      if (height && image) {
        actual.height = image.imageHeight;
      }
      if (name && nameBox) {
        actual.name = nameBox.value;
      }

      if (!chrome.test.checkDeepEq(expected, actual)) {
        return pending(
            caller, 'Slide mode state, expected is %j, actual is %j.', expected,
            actual);
      }
      return actual;
    });
  }

  async changeNameAndWait(appId, newName) {
    await this.callRemoteTestUtil('changeName', appId, [newName]);
    return this.waitForSlideImage(appId, 0, 0, newName);
  }

  /**
   * Waits for the "Press Enter" message.
   *
   * @param {string} appId App window Id.
   * @return {Promise} Promise to be fulfilled when the element appears.
   */
  async waitForPressEnterMessage(appId) {
    const element = await this.waitForElement(appId, '.prompt-wrapper .prompt');
    chrome.test.assertEq('Press Enter when done', element.text.trim());
  }

  /**
   * Shorthand for selecting an image in thumbnail mode.
   * @param {string} appId App window Id.
   * @param {string} name File name to be selected.
   * @return {!Promise<boolean>} A promise which will be resolved with true if
   *     the thumbnail has clicked. This method does not guarantee whether the
   *     thumbnail has actually selected or not.
   */
  selectImageInThumbnailMode(appId, name) {
    return this.callRemoteTestUtil(
        'fakeMouseClick', appId,
        ['.thumbnail-view > ul > li[title="' + name + '"] > .selection.frame']);
  }
}
