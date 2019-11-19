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
  if (!autostep) {
    autostep = true;
  }
  if (autostep && typeof window.step == 'function') {
    window.step();
  }
}

/**
 * Class to manipulate the window in the remote extension.
 *
 * @param {string} extensionId ID of extension to be manipulated.
 * @constructor
 */
function RemoteCall(extensionId) {
  this.extensionId_ = extensionId;

  /**
   * Tristate holding the cached result of isStepByStepEnabled_().
   * @type{?bool}
   */
  this.cachedStepByStepEnabled_ = null;
}

/**
 * Checks whether step by step tests are enabled or not.
 * @private
 * @return {Promise<bool>}
 */
RemoteCall.prototype.isStepByStepEnabled_ = async function() {
  if (this.cachedStepByStepEnabled_ === null) {
    this.cachedStepByStepEnabled_ = await new Promise((fulfill) => {
      chrome.commandLinePrivate.hasSwitch(
          'enable-file-manager-step-by-step-tests', fulfill);
    });
  }
  return this.cachedStepByStepEnabled_;
};

/**
 * Calls a remote test util in the Files app's extension. See:
 * registerRemoteTestUtils in test_util_base.js.
 *
 * @param {string} func Function name.
 * @param {?string} appId Target window's App ID or null for functions
 *     not requiring a window.
 * @param {Array<*>} args Array of arguments.
 * @param {function(*)=} opt_callback Callback handling the function's result.
 * @return {Promise} Promise to be fulfilled with the result of the remote
 *     utility.
 */
RemoteCall.prototype.callRemoteTestUtil =
    async function(func, appId, args, opt_callback) {
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
};

/**
 * Waits until a window having the given ID prefix appears.
 * @param {string} windowIdPrefix ID prefix of the requested window.
 * @return {Promise} promise Promise to be fulfilled with a found window's ID.
 */
RemoteCall.prototype.waitForWindow = function(windowIdPrefix) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const windows = await this.callRemoteTestUtil('getWindows', null, []);
    for (var id in windows) {
      if (id.indexOf(windowIdPrefix) === 0) {
        return id;
      }
    }
    return pending(
        caller, 'Window with the prefix %s is not found.', windowIdPrefix);
  });
};

/**
 * Closes a window and waits until the window is closed.
 *
 * @param {string} windowId ID of the window to close.
 * @return {Promise} promise Promise to be fulfilled with the result (true:
 *     success, false: failed).
 */
RemoteCall.prototype.closeWindowAndWait = async function(windowId) {
  var caller = getCaller();

  // Closes the window.
  if (!await this.callRemoteTestUtil('closeWindow', null, [windowId])) {
    // Returns false when the closing is failed.
    return false;
  }

  return repeatUntil(async () => {
    const windows = await this.callRemoteTestUtil('getWindows', null, []);
    for (var id in windows) {
      if (id === windowId) {
        // Window is still available. Continues waiting.
        return pending(
            caller, 'Window with the prefix %s is not found.', windowId);
      }
    }
    // Window is not available. Closing is done successfully.
    return true;
  });
};

/**
 * Waits until the window turns to the given size.
 * @param {string} windowId Target window ID.
 * @param {number} width Requested width in pixels.
 * @param {number} height Requested height in pixels.
 */
RemoteCall.prototype.waitForWindowGeometry = function(windowId, width, height) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const windows = await this.callRemoteTestUtil('getWindows', null, []);
    if (!windows[windowId]) {
      return pending(caller, 'Window %s is not found.', windowId);
    }
    if (windows[windowId].outerWidth !== width ||
        windows[windowId].outerHeight !== height) {
      return pending(
          caller, 'Expected window size is %j, but it is %j',
          {width: width, height: height}, windows[windowId]);
    }
  });
};

/**
 * Waits for the specified element appearing in the DOM.
 * @param {string} windowId Target window ID.
 * @param {string|!Array<string>} query Query to specify the element.
 *     If query is an array, |query[0]| specifies the first
 *     element(s), |query[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
RemoteCall.prototype.waitForElement = function(windowId, query) {
  return this.waitForElementStyles(windowId, query, []);
};

/**
 * Waits for the specified element appearing in the DOM.
 * @param {string} windowId Target window ID.
 * @param {string|!Array<string>} query Query to specify the element.
 *     If query is an array, |query[0]| specifies the first
 *     element(s), |query[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {!Array<string>} styleNames List of CSS property name to be
 *     obtained. NOTE: Causes element style re-calculation.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
RemoteCall.prototype.waitForElementStyles = function(
    windowId, query, styleNames) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const elements = await this.callRemoteTestUtil(
        'deepQueryAllElements', windowId, [query, styleNames]);
    if (elements.length > 0) {
      return elements[0];
    }
    return pending(caller, 'Element %s is not found.', query);
  });
};

/**
 * Waits for a remote test function to return a specific result.
 *
 * @param {string} funcName Name of remote test function to be executed.
 * @param {string} windowId Target window ID.
 * @param {function(Object): boolean|Object} expectedResult An value to be
 *     checked against the return value of |funcName| or a callabck that
 *     receives the return value of |funcName| and returns true if the result
 *     is the expected value.
 * @param {?Array<*>} args Arguments to be provided to |funcName| when executing
 *     it.
 * @return {Promise} Promise to be fulfilled when the |expectedResult| is
 *     returned from |funcName| execution.
 */
RemoteCall.prototype.waitFor = function(
    funcName, windowId, expectedResult, args) {
  const caller = getCaller();
  args = args || [];
  return repeatUntil(async () => {
    const result = await this.callRemoteTestUtil(funcName, windowId, args);
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
};

/**
 * Waits for the specified element leaving from the DOM.
 * @param {string} windowId Target window ID.
 * @param {string|!Array<string>} query Query to specify the element.
 *     If query is an array, |query[0]| specifies the first
 *     element(s), |query[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @return {Promise} Promise to be fulfilled when the element is lost.
 */
RemoteCall.prototype.waitForElementLost = function(windowId, query) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const elements = await this.callRemoteTestUtil(
        'deepQueryAllElements', windowId, [query]);
    if (elements.length > 0) {
      return pending(caller, 'Elements %j is still exists.', elements);
    }
    return true;
  });
};

/**
 * Sends a fake key down event.
 * @param {string} windowId Window ID.
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
RemoteCall.prototype.fakeKeyDown =
    async function(windowId, query, key, ctrlKey, shiftKey, altKey) {
  const result = await this.callRemoteTestUtil(
      'fakeKeyDown', windowId, [query, key, ctrlKey, shiftKey, altKey]);
  if (result) {
    return true;
  } else {
    throw new Error('Fail to fake key down.');
  }
};

/**
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<string>} names File name list.
 * @return {Promise} Promise to be fulfilled with file entries or rejected
 *     depending on the result.
 */
RemoteCall.prototype.getFilesUnderVolume = function(volumeType, names) {
  return this.callRemoteTestUtil(
      'getFilesUnderVolume', null, [volumeType, names]);
};

/**
 * Waits for a single file.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {string} name File name.
 * @return {!Promise} Promise to be fulfilled when the file had found.
 */
RemoteCall.prototype.waitForAFile = function(volumeType, name) {
  var caller = getCaller();
  return repeatUntil(async () => {
    if ((await this.getFilesUnderVolume(volumeType, [name])).length === 1) {
      return true;
    }
    return pending(caller, '"' + name + '" is not found.');
  });
};

/**
 * Shorthand for clicking an element.
 * @param {AppWindow} appWindow Application window.
 * @param {string|!Array<string>} query Query to specify the element.
 *     If query is an array, |query[0]| specifies the first
 *     element(s), |query[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object
 * @param {Promise} Promise to be fulfilled with the clicked element.
 */
RemoteCall.prototype.waitAndClickElement =
    async function(windowId, query, opt_keyModifiers) {
  const element = await this.waitForElement(windowId, query);
  const result = await this.callRemoteTestUtil(
      'fakeMouseClick', windowId, [query, opt_keyModifiers]);
  chrome.test.assertTrue(result, 'mouse click failed.');
  return element;
};

/**
 * Shorthand for right-clicking an element.
 * @param {AppWindow} appWindow Application window.
 * @param {string|!Array<string>} query Query to specify the element.
 *     If query is an array, |query[0]| specifies the first
 *     element(s), |query[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object
 * @param {Promise} Promise to be fulfilled with the clicked element.
 */
RemoteCall.prototype.waitAndRightClick =
    async function(windowId, query, opt_keyModifiers) {
  const element = await this.waitForElement(windowId, query);
  const result = await this.callRemoteTestUtil(
      'fakeMouseRightClick', windowId, [query, opt_keyModifiers]);
  chrome.test.assertTrue(result, 'mouse right-click failed.');
  return element;
};

/**
 * Shorthand for focusing an element.
 * @param {AppWindow} appWindow Application window.
 * @param {!Array<string>} query Query to specify the element to be focused.
 * @param {Promise} Promise to be fulfilled with the focused element.
 */
RemoteCall.prototype.focus = async function(windowId, query) {
  const element = await this.waitForElement(windowId, query);
  const result = await this.callRemoteTestUtil('focus', windowId, query);
  chrome.test.assertTrue(result, 'focus failed.');
  return element;
};

/**
 * Class to manipulate the window in the remote extension.
 *
 * @param {string} extensionId ID of extension to be manipulated.
 * @extends {RemoteCall}
 * @constructor
 */
function RemoteCallFilesApp() {
  RemoteCall.apply(this, arguments);
}

RemoteCallFilesApp.prototype.__proto__ = RemoteCall.prototype;

/**
 * Waits for the file list turns to the given contents.
 * @param {string} windowId Target window ID.
 * @param {Array<Array<string>>} expected Expected contents of file list.
 * @param {{orderCheck:boolean=, ignoreLastModifiedTime:boolean=}=} opt_options
 *     Options of the comparison. If orderCheck is true, it also compares the
 *     order of files. If ignoreLastModifiedTime is true, it compares the file
 *     without its last modified time.
 * @return {Promise} Promise to be fulfilled when the file list turns to the
 *     given contents.
 */
RemoteCallFilesApp.prototype.waitForFiles = function(
    windowId, expected, opt_options) {
  var options = opt_options || {};
  var caller = getCaller();
  return repeatUntil(async () => {
    const files = await this.callRemoteTestUtil('getFileList', windowId, []);
    if (!options.orderCheck) {
      files.sort();
      expected.sort();
    }
    for (var i = 0; i < Math.min(files.length, expected.length); i++) {
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
};

/**
 * Waits until the number of files in the file list is changed from the given
 * number.
 * TODO(hirono): Remove the function.
 *
 * @param {string} windowId Target window ID.
 * @param {number} lengthBefore Number of items visible before.
 * @return {Promise} Promise to be fulfilled with the contents of files.
 */
RemoteCallFilesApp.prototype.waitForFileListChange = function(
    windowId, lengthBefore) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const files = await this.callRemoteTestUtil('getFileList', windowId, []);
    files.sort();

    var notReadyRows =
        files.filter((row) => row.filter((cell) => cell == '...').length);

    if (notReadyRows.length === 0 && files.length !== lengthBefore &&
        files.length !== 0) {
      return files;
    } else {
      return pending(
          caller, 'The number of file is %d. Not changed.', lengthBefore);
    }
  });
};

/**
 * Waits until the given taskId appears in the executed task list.
 * @param {string} windowId Target window ID.
 * @param {string} taskId Task ID to watch.
 * @return {Promise} Promise to be fulfilled when the task appears in the
 *     executed task list.
 */
RemoteCallFilesApp.prototype.waitUntilTaskExecutes = function(
    windowId, taskId) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const executedTasks =
        await this.callRemoteTestUtil('getExecutedTasks', windowId, []);
    if (executedTasks.indexOf(taskId) === -1) {
      return pending(caller, 'Executed task is %j', executedTasks);
    }
  });
};

/**
 * Check if the next tabforcus'd element has the given ID or not.
 * @param {string} windowId Target window ID.
 * @param {string} elementId String of 'id' attribute which the next tabfocus'd
 *     element should have.
 * @return {Promise} Promise to be fulfilled with the result.
 */
RemoteCallFilesApp.prototype.checkNextTabFocus =
    async function(windowId, elementId) {
  const result = await sendTestMessage({name: 'dispatchTabKey'});
  chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failure');

  var caller = getCaller();
  return repeatUntil(async () => {
    var element =
        await remoteCall.callRemoteTestUtil('getActiveElement', windowId, []);
    if (element && element.attributes['id'] === elementId) {
      return true;
    }
    return pending(
        caller,
        'Waiting for active element with id: "' + elementId +
            '", but current is: "' + element.attributes['id'] + '"');
  });
};

/**
 * Waits until the current directory is changed.
 * @param {string} windowId Target window ID.
 * @param {string} expectedPath Path to be changed to.
 * @return {Promise} Promise to be fulfilled when the current directory is
 *     changed to expectedPath.
 */
RemoteCallFilesApp.prototype.waitUntilCurrentDirectoryIsChanged = function(
    windowId, expectedPath) {
  var caller = getCaller();
  return repeatUntil(async () => {
    const path =
        await this.callRemoteTestUtil('getBreadcrumbPath', windowId, []);
    if (path !== expectedPath) {
      return pending(caller, 'Expected path is %s got %s', expectedPath, path);
    }
  });
};

/**
 * Expands tree item.
 * @param {string} windowId Target window ID.
 * @param {string} query Query to the <tree-item> element.
 */
RemoteCallFilesApp.prototype.expandTreeItemInDirectoryTree =
    async function(windowId, query) {
  await this.waitForElement(windowId, query);
  const elements = await this.callRemoteTestUtil(
      'queryAllElements', windowId, [`${query}[expanded]`]);
  // If it's already expanded just set the focus on directory tree.
  if (elements.length > 0) {
    return this.callRemoteTestUtil('focus', windowId, ['#directory-tree']);
  }

  // We must wait until <tree-item> has attribute [has-children=true]
  // otherwise it won't expand. We must also to account for the case
  // :not([expanded]) to ensure it has NOT been expanded by some async
  // operation since the [expanded] checks above.
  const expandIcon =
      query + ':not([expanded]) > .tree-row[has-children=true] > .expand-icon';
  await this.waitAndClickElement(windowId, expandIcon);
  // Wait for the expansion to finish.
  await this.waitForElement(windowId, query + '[expanded]');
  // Force the focus on directory tree.
  await this.callRemoteTestUtil('focus', windowId, ['#directory-tree']);
};

/**
 * Expands directory tree for specified path.
 */
RemoteCallFilesApp.prototype.expandDirectoryTreeFor = function(
    windowId, path, volumeType = 'downloads') {
  return this.expandDirectoryTreeForInternal_(
      windowId, path.split('/'), 0, volumeType);
};

/**
 * Internal function for expanding directory tree for specified path.
 */
RemoteCallFilesApp.prototype.expandDirectoryTreeForInternal_ =
    async function(windowId, components, index, volumeType) {
  if (index >= components.length - 1) {
    return;
  }

  // First time we should expand the root/volume first.
  if (index === 0) {
    await this.expandVolumeInDirectoryTree(windowId, volumeType);
    return this.expandDirectoryTreeForInternal_(
        windowId, components, index + 1, volumeType);
  }
  const path = '/' + components.slice(1, index + 1).join('/');
  await this.expandTreeItemInDirectoryTree(
      windowId, `[full-path-for-testing="${path}"]`);
  await this.expandDirectoryTreeForInternal_(
      windowId, components, index + 1, volumeType);
};

/**
 * Expands download volume in directory tree.
 */
RemoteCallFilesApp.prototype.expandDownloadVolumeInDirectoryTree = function(
    windowId) {
  return this.expandVolumeInDirectoryTree(windowId, 'downloads');
};

/**
 * Expands download volume in directory tree.
 */
RemoteCallFilesApp.prototype.expandVolumeInDirectoryTree = function(
    windowId, volumeType) {
  return this.expandTreeItemInDirectoryTree(
      windowId, `[volume-type-for-testing="${volumeType}"]`);
};

/**
 * Navigates to specified directory on the specified volume by using directory
 * tree.
 * DEPRECATED: Use background.js:navigateWithDirectoryTree instead
 * crbug.com/996626.
 */
RemoteCallFilesApp.prototype.navigateWithDirectoryTree =
    async function(windowId, path, rootLabel, volumeType = 'downloads') {
  await this.expandDirectoryTreeFor(windowId, path, volumeType);

  // Select target path.
  await this.callRemoteTestUtil(
      'fakeMouseClick', windowId, [`[full-path-for-testing="${path}"]`]);

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
      windowId, `/${rootLabel}${path}`);
};

/**
 * Class to manipulate the window in the remote extension.
 *
 * @param {string} extensionId ID of extension to be manipulated.
 * @extends {RemoteCall}
 * @constructor
 */
function RemoteCallGallery() {
  RemoteCall.apply(this, arguments);
}

RemoteCallGallery.prototype.__proto__ = RemoteCall.prototype;

/**
 * Waits until the expected image is shown.
 *
 * @param {document} document Document.
 * @param {number} width Expected width of the image.
 * @param {number} height Expected height of the image.
 * @param {string|null} name Expected name of the image.
 * @return {Promise} Promsie to be fulfilled when the check is passed.
 */
RemoteCallGallery.prototype.waitForSlideImage = function(
    windowId, width, height, name) {
  var expected = {};
  if (width) {
    expected.width = width;
  }
  if (height) {
    expected.height = height;
  }
  if (name) {
    expected.name = name;
  }
  var caller = getCaller();

  return repeatUntil(async () => {
    var query = '.gallery[mode="slide"] .image-container > .image';
    const [nameBox, image] = await Promise.all([
      this.waitForElement(windowId, '#rename-input'),
      this.waitForElementStyles(windowId, query, ['any'])
    ]);
    var actual = {};
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
};

RemoteCallGallery.prototype.changeNameAndWait =
    async function(windowId, newName) {
  await this.callRemoteTestUtil('changeName', windowId, [newName]);
  return this.waitForSlideImage(windowId, 0, 0, newName);
};

/**
 * Waits for the "Press Enter" message.
 *
 * @param {AppWindow} appWindow App window.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
RemoteCallGallery.prototype.waitForPressEnterMessage = async function(appId) {
  const element = await this.waitForElement(appId, '.prompt-wrapper .prompt');
  chrome.test.assertEq('Press Enter when done', element.text.trim());
};

/**
 * Shorthand for selecting an image in thumbnail mode.
 * @param {string} appId App id.
 * @param {string} name File name to be selected.
 * @return {!Promise<boolean>} A promise which will be resolved with true if the
 *     thumbnail has clicked. This method does not guarantee whether the
 *     thumbnail has actually selected or not.
 */
RemoteCallGallery.prototype.selectImageInThumbnailMode = function(appId, name) {
  return this.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['.thumbnail-view > ul > li[title="' + name + '"] > .selection.frame']);
};
