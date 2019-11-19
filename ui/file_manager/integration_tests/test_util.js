// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sends a command to the controlling test harness, namely and usually, the
 * chrome FileManagerBrowserTest harness: it expects the command to contain
 * the 'name' of the command, and any required or optional arguments of the
 * command, e.g.,
 *
 *   await sendTestMessage({
 *     name: 'addEntries', // command with volume and entries arguments
 *     volume: volume,
 *     entries: entries
 *   });
 *
 * @param {Object} command Test command to send. The object is converted to
 *     a JSON string prior to sending.
 * @return {Promise} Promise to be fulfilled with the value returned by the
 *     chrome.test.sendMessage callback.
 */
function sendTestMessage(command) {
  if (typeof command.name === 'string') {
    return new Promise(function(fulfill) {
      chrome.test.sendMessage(JSON.stringify(command), fulfill);
    });
  } else {
    const error = 'sendTestMessage requires a command.name <string>';
    throw new Error(error);
  }
}

/**
 * Wait (aka pause, or sleep) for the given time in milliseconds.
 * @param {number} time Time in milliseconds.
 * @return {Promise} Promise that will resolve after Time in milliseconds
 *     has elapsed.
 */
function wait(time) {
  return new Promise(function(resolve) {
    setTimeout(resolve, time);
  });
}

/**
 * Verifies if there are no Javascript errors in the given app window by
 * asserting the count returned by the app.getErrorCount remote call.
 * @param {!RemoteCall} app RemoteCall interface to the app window.
 * @param {function()} callback Completion callback.
 * @return {Promise} Promise to be fulfilled on completion.
 */
async function checkIfNoErrorsOccuredOnApp(app, callback) {
  const count = await app.callRemoteTestUtil('getErrorCount', null, []);
  chrome.test.assertEq(0, count, 'The error count is not 0.');
  if (callback) {
    callback();
  }
}

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise to add the check to.
 * @param {Array<!RemoteCall>} apps An array of RemoteCall interfaces.
 */
async function testPromiseAndApps(promise, apps) {
  const finished = chrome.test.callbackPass(function() {
    // The callbackPass is necessary to avoid prematurely finishing tests.
    // Don't use chrome.test.succeed() here to avoid doubled success log.
  });
  try {
    await promise;
    await Promise.all(apps.map(app => checkIfNoErrorsOccuredOnApp(app)));
  } catch (error) {
    chrome.test.fail(error.stack || error);
    return;
  }
  finished();
}

/**
 * Interval milliseconds between checks of repeatUntil.
 * @type {number}
 * @const
 */
var REPEAT_UNTIL_INTERVAL = 200;

/**
 * Interval milliseconds between log output of repeatUntil.
 * @type {number}
 * @const
 */
var LOG_INTERVAL = 3000;

/**
 * Returns caller's file, function and line/column number from the call stack.
 * @return {string} String with the caller's file name and line/column number,
 *     as returned by exception stack trace. Example "at /a_file.js:1:1".
 */
function getCaller() {
  let error = new Error('For extracting error.stack');
  let ignoreStackLines = 3;
  let lines = error.stack.split('\n');
  if (ignoreStackLines < lines.length) {
    let caller = lines[ignoreStackLines];
    // Strip 'chrome-extension://oobinhbdbiehknkpbpejbbpdbkdjmoco' prefix.
    return caller.replace(/(chrome-extension:\/\/\w*)/gi, '').trim();
  }
  return '';
}


/**
 * Returns a pending marker. See also the repeatUntil function.
 * @param {string} name of test function that originated the operation,
 *     it's the return of getCaller() function.
 * @param {string} message Pending reason including %s, %d, or %j markers. %j
 *     format an object as JSON.
 * @param {Array<*>} var_args Values to be assigined to %x markers.
 * @return {Object} Object which returns true for the expression: obj instanceof
 *     pending.
 */
function pending(caller, message, var_args) {
  // |index| is used to ignore caller and message arguments subsisting markers
  // (%s, %d and %j) within message with the remaining |arguments|.
  var index = 2;
  var args = arguments;
  message = String(message);
  var formattedMessage = message.replace(/%[sdj]/g, function(pattern) {
    var arg = args[index++];
    switch (pattern) {
      case '%s':
        return String(arg);
      case '%d':
        return Number(arg);
      case '%j':
        return JSON.stringify(arg);
      default:
        return pattern;
    }
  });
  var pendingMarker = Object.create(pending.prototype);
  pendingMarker.message = caller + ': ' + formattedMessage;
  return pendingMarker;
}

/**
 * Waits until the checkFunction returns a value but a pending marker.
 * @param {function():*} checkFunction Function to check a condition. It can
 *     return a pending marker created by a pending function.
 * @return {Promise} Promise to be fulfilled with the return value of
 *     checkFunction when the checkFunction reutrns a value but a pending
 *     marker.
 */
async function repeatUntil(checkFunction) {
  var logTime = Date.now() + LOG_INTERVAL;
  while (true) {
    const result = await checkFunction();
    if (!(result instanceof pending)) {
      return result;
    }
    if (Date.now() > logTime) {
      console.warn(result.message);
      logTime += LOG_INTERVAL;
    }
    await wait(REPEAT_UNTIL_INTERVAL);
  }
}

/**
 * Sends the test |command| to the browser test harness and awaits a 'string'
 * result. Calls |callback| with that result.
 * @param {Object} command Test command to send. Refer to sendTestMessage()
 *    above for the expected format of a test |command| object.
 * @param {function(string)} callback Completion callback.
 * @param {Object=} opt_debug If truthy, log the result.
 */
async function sendBrowserTestCommand(command, callback, opt_debug) {
  const caller = getCaller();
  if (typeof command.name !== 'string') {
    chrome.test.fail('Invalid test command: ' + JSON.stringify(command));
  }
  const result = await repeatUntil(async () => {
    const tryAgain = pending(caller, 'Sent BrowserTest ' + command.name);
    try {
      const result = await sendTestMessage(command);
      if (typeof result !== 'string') {
        return tryAgain;
      }
      return result;
    } catch (error) {
      console.log(error.stack || error);
      return tryAgain;
    }
  });
  if (opt_debug) {
    console.log('BrowserTest ' + command.name + ': ' + result);
  }
  callback(result);
}

/**
 * Waits for an app window with the URL |windowUrl|.
 * @param {string} windowUrl URL of the app window to wait for.
 * @return {Promise} Promise to be fulfilled with the window ID of the
 *     app window.
 */
function waitForAppWindow(windowUrl) {
  const caller = getCaller();
  const command = {'name': 'getAppWindowId', 'windowUrl': windowUrl};
  return repeatUntil(async () => {
    const result = await sendTestMessage(command);
    if (result == 'none') {
      return pending(caller, 'getAppWindowId ' + windowUrl);
    }
    return result;
  });
}

/**
 * Wait for the count of windows for app |appId| to equal |expectedCount|.
 * @param{string} appId ID of the app to count windows for.
 * @param{number} expectedCount Number of app windows to wait for.
 * @return {Promise} Promise to be fulfilled when the number of app windows
 *     equals |expectedCount|.
 */
function waitForAppWindowCount(appId, expectedCount) {
  const caller = getCaller();
  const command = {'name': 'countAppWindows', 'appId': appId};
  return repeatUntil(async () => {
    if (await sendTestMessage(command) != expectedCount) {
      return pending(caller, 'waitForAppWindowCount ' + appId + ' ' + result);
    }
    return true;
  });
}

/**
 * Get all the browser windows.
 * @return {Object} Object returned from chrome.windows.getAll().
 */
async function getBrowserWindows() {
  const caller = getCaller();
  return repeatUntil(async () => {
    const result = await new Promise(function(fulfill) {
      chrome.windows.getAll({'populate': true}, fulfill);
    });
    if (result.length == 0) {
      return pending(caller, 'getBrowserWindows ' + result.length);
    }
    return result;
  });
}

/**
 * Adds the given entries to the target volume(s).
 * @param {Array<string>} volumeNames Names of target volumes.
 * @param {Array<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)=} opt_callback Callback function to be passed the
 *     result of function. The argument is true on success.
 * @return {Promise} Promise to be fulfilled when the entries are added.
 */
async function addEntries(volumeNames, entries, opt_callback) {
  if (volumeNames.length == 0) {
    callback(true);
    return;
  }
  var volumeResultPromises = volumeNames.map(function(volume) {
    return sendTestMessage({
      name: 'addEntries',
      volume: volume,
      entries: entries,
    });
  });
  if (!opt_callback) {
    return volumeResultPromises;
  }
  try {
    await Promise.all(volumeResultPromises);
  } catch (error) {
    opt_callback(false);
    throw error;
  }
  opt_callback(true);
}

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory',
  LINK: 'link',
  SHARED_DRIVE: 'team_drive',
  COMPUTER: 'Computer'
});

/**
 * @enum {string}
 * @const
 */
var SharedOption = Object.freeze({
  NONE: 'none',
  SHARED: 'shared',
  SHARED_WITH_ME: 'sharedWithMe',
  NESTED_SHARED_WITH_ME: 'nestedSharedWithMe',
});

/**
 * @enum {string}
 */
var RootPath = Object.seal({
  DOWNLOADS: '/must-be-filled-in-test-setup',
  DRIVE: '/must-be-filled-in-test-setup',
  ANDROID_FILES: '/must-be-filled-in-test-setup',
});


/**
 * The capabilities (permissions) for the Test Entry. Structure should match
 * TestEntryCapabilities in file_manager_browsertest_base.cc. All capabilities
 * default to true if not specified.
 *
 * @record
 * @struct
 */
function TestEntryCapabilities() {}

/**
 * @type {boolean|undefined}
 */
TestEntryCapabilities.prototype.canCopy = true;

/**
 * @type {boolean|undefined}
 */
TestEntryCapabilities.prototype.canDelete = true;

/**
 * @type {boolean|undefined}
 */
TestEntryCapabilities.prototype.canRename = true;

/**
 * @type {boolean|undefined}
 */
TestEntryCapabilities.prototype.canAddChildren = true;

/**
 * @type {boolean|undefined}
 */
TestEntryCapabilities.prototype.canShare = true;

/**
 * The folder features for the test entry. Structure should match
 * TestEntryFolderFeature in file_manager_browsertest_base.cc. All features
 * default to false is not specified.
 *
 * @record
 * @struct
 */
function TestEntryFolderFeature() {}

/**
 * @type {boolean|undefined}
 */
TestEntryFolderFeature.prototype.isMachineRoot = false;

/**
 * @type {boolean|undefined}
 */
TestEntryFolderFeature.prototype.isArbitrarySyncFolder = false;

/**
 * @type {boolean|undefined}
 */
TestEntryFolderFeature.prototype.isExternalMedia = false;

/**
 * Parameters to creat a Test Entry in the file manager. Structure should match
 * TestEntryInfo in file_manager_browsertest_base.cc.
 *
 * @record
 * @struct
 */
function TestEntryInfoOptions() {}

/**
 * @type {EntryType} Entry type.
 */
TestEntryInfoOptions.prototype.type;
/**
 * @type {string|undefined} Source file name that provides file contents
 *     (file location relative to /chrome/test/data/chromeos/file_manager/).
 */
TestEntryInfoOptions.prototype.sourceFileName;
/**
 * @type {string} Name of entry on the test file system. Used to determine the
 *     actual name of the file.
 */
TestEntryInfoOptions.prototype.targetPath;
/**
 * @type {string} Name of the team drive this entry is in. Defaults to a blank
 *     string (no team drive). Team Drive names must be unique.
 */
TestEntryInfoOptions.prototype.teamDriveName;
/**
 * @type {string} Name of the computer this entry is in. Defaults to a blank
 *     string (no computer). Computer names must be unique.
 */
TestEntryInfoOptions.prototype.computerName;
/**
 * @type {string|undefined} Mime type.
 */
TestEntryInfoOptions.prototype.mimeType;
/**
 * @type {SharedOption|undefined} Shared option. Defaults to NONE (not shared).
 */
TestEntryInfoOptions.prototype.sharedOption;
/**
 * @type {string} Last modified time as a text to be shown in the last modified
 *     column.
 */
TestEntryInfoOptions.prototype.lastModifiedTime;
/**
 * @type {string} File name to be shown in the name column.
 */
TestEntryInfoOptions.prototype.nameText;
/**
 * @type {string} Size text to be shown in the size column.
 */
TestEntryInfoOptions.prototype.sizeText;
/**
 * @type {string} Type name to be shown in the type column.
 */
TestEntryInfoOptions.prototype.typeText;
/**
 * @type {TestEntryCapabilities|undefined} Capabilities of this file. Defaults
 *     to all capabilities available (read-write access).
 */
TestEntryInfoOptions.prototype.capabilities;

/**
 * @type {TestEntryFolderFeature|undefined} Foder features of this file.
 *     Defaults to all features disabled.
 */
TestEntryInfoOptions.prototype.folderFeature;

/**
 * File system entry information for tests. Structure should match TestEntryInfo
 * in file_manager_browsertest_base.cc
 * TODO(sashab): Remove this, rename TestEntryInfoOptions to TestEntryInfo and
 * set the defaults in the record definition above.
 *
 * @param {TestEntryInfoOptions} options Parameters to create the TestEntryInfo.
 */
function TestEntryInfo(options) {
  this.type = options.type;
  this.sourceFileName = options.sourceFileName || '';
  this.targetPath = options.targetPath;
  this.teamDriveName = options.teamDriveName || '';
  this.computerName = options.computerName || '';
  this.mimeType = options.mimeType || '';
  this.sharedOption = options.sharedOption || SharedOption.NONE;
  this.lastModifiedTime = options.lastModifiedTime;
  this.nameText = options.nameText;
  this.sizeText = options.sizeText;
  this.typeText = options.typeText;
  this.capabilities = options.capabilities;
  this.folderFeature = options.folderFeature;
  this.pinned = !!options.pinned;
  Object.freeze(this);
}

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) {
    return entry.getExpectedRow();
  });
};

/**
 * Obtains a expected row contents of the file in the file list.
 */
TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

/**
 * Filesystem entries used by the test cases.
 * TODO(sashab): Rename 'nameText', 'sizeText' and 'typeText' to
 * 'expectedNameText', 'expectedSizeText' and 'expectedTypeText' to reflect that
 * they are the expected values for those columns in the file manager.
 *
 * @type {Object<TestEntryInfo>}
 * @const
 */
var ENTRIES = {
  hello: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'hello.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'hello.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text'
  }),

  world: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.ogv',
    targetPath: 'world.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'world.ogv',
    sizeText: '59 KB',
    typeText: 'OGG video'
  }),

  webm: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'world.webm',
    targetPath: 'world.webm',
    mimeType: 'video/webm',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'world.webm',
    sizeText: '17 KB',
    typeText: 'WebM video'
  }),

  video: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video_long.ogv',
    targetPath: 'video_long.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jan 14, 2019, 16:01 PM',
    nameText: 'video_long.ogv',
    sizeText: '166 KB',
    typeText: 'OGG video'
  }),

  subtitle: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.vtt',
    targetPath: 'world.vtt',
    mimeType: 'text/vtt',
    lastModifiedTime: 'Feb 7, 2019, 15:03 PM',
    nameText: 'world.vtt',
    sizeText: '46 bytes',
    typeText: 'VTT text'
  }),

  unsupported: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'random.bin',
    targetPath: 'unsupported.foo',
    mimeType: 'application/x-foo',
    lastModifiedTime: 'Jul 4, 2012, 10:36 AM',
    nameText: 'unsupported.foo',
    sizeText: '8 KB',
    typeText: 'FOO file'
  }),

  desktop: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image.png',
    targetPath: 'My Desktop Background.png',
    mimeType: 'image/png',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'My Desktop Background.png',
    sizeText: '272 bytes',
    typeText: 'PNG image'
  }),

  image2: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image2.png',
    // No file extension.
    targetPath: 'image2',
    mimeType: 'image/png',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'image2',
    sizeText: '4 KB',
    typeText: 'PNG image'
  }),

  image3: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image3.jpg',
    targetPath: 'image3.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'image3.jpg',
    sizeText: '3 KB',
    typeText: 'JPEG image'
  }),

  smallJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'small.jpg',
    targetPath: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'small.jpg',
    sizeText: '1 KB',
    typeText: 'JPEG image'
  }),

  exifImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'exif.jpg',
    // No mime type.
    targetPath: 'exif.jpg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'exif.jpg',
    sizeText: '31 KB',
    typeText: 'JPEG image'
  }),

  rawImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'raw.orf',
    // No mime type.
    targetPath: 'raw.orf',
    lastModifiedTime: 'May 20, 2019, 10:10 AM',
    nameText: 'raw.orf',
    sizeText: '214 KB',
    typeText: 'ORF image'
  }),

  beautiful: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
    // No mime type.
    targetPath: 'Beautiful Song.ogg',
    lastModifiedTime: 'Nov 12, 2086, 12:00 PM',
    nameText: 'Beautiful Song.ogg',
    sizeText: '14 KB',
    typeText: 'OGG audio'
  }),

  photos: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'photos',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: 'photos',
    sizeText: '--',
    typeText: 'Folder'
  }),

  testDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Test Document',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'Test Document.gdoc',
    sizeText: '--',
    typeText: 'Google document'
  }),

  testSharedDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Test Shared Document',
    mimeType: 'application/vnd.google-apps.document',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Test Shared Document.gdoc',
    sizeText: '--',
    typeText: 'Google document'
  }),

  testSharedFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'test.txt',
    mimeType: 'text/plain',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Mar 20, 2012, 11:40 PM',
    nameText: 'test.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    pinned: true
  }),

  newlyAdded: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
    targetPath: 'newly added file.ogg',
    mimeType: 'audio/ogg',
    lastModifiedTime: 'Sep 4, 1998, 12:00 AM',
    nameText: 'newly added file.ogg',
    sizeText: '14 KB',
    typeText: 'OGG audio'
  }),

  tallText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.txt',
    targetPath: 'tall.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.txt',
    sizeText: '546 bytes',
    typeText: 'Plain text',
  }),

  plainText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'plaintext',
    // No mime type, no file extension.
    targetPath: 'plaintext',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'plaintext',
    sizeText: '32 bytes',
    typeText: 'Plain text',
  }),

  tallHtml: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.html',
    targetPath: 'tall.html',
    mimeType: 'text/html',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.html',
    sizeText: '589 bytes',
    typeText: 'HTML document',
  }),

  tallPdf: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.pdf',
    targetPath: 'tall.pdf',
    mimeType: 'application/pdf',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.pdf',
    sizeText: '15 KB',
    typeText: 'PDF document',
  }),

  imgPdf: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'img.pdf',
    targetPath: 'imgpdf',
    mimeType: 'application/pdf',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'imgpdf',
    sizeText: '1608 bytes',
    typeText: 'PDF document'
  }),

  pinned: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'pinned.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'pinned.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    pinned: true,
  }),

  directoryA: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder'
  }),

  directoryB: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'B',
    sizeText: '--',
    typeText: 'Folder'
  }),

  directoryC: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'C',
    sizeText: '--',
    typeText: 'Folder'
  }),

  directoryD: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'D',
    sizeText: '--',
    typeText: 'Folder'
  }),

  directoryE: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D/E',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'E',
    sizeText: '--',
    typeText: 'Folder'
  }),

  directoryF: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D/E/F',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'F',
    sizeText: '--',
    typeText: 'Folder'
  }),

  deeplyBurriedSmallJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B/C/deep.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'deep.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image'
  }),

  linkGtoB: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'G',
    sourceFileName: 'A/B',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'G',
    sizeText: '--',
    typeText: 'Folder'
  }),

  linkHtoFile: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'H.jpg',
    sourceFileName: 'A/B/C/deep.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'H.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image'
  }),

  linkTtoTransitiveDirectory: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'T',
    sourceFileName: 'G/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'T',
    sizeText: '--',
    typeText: 'Folder'
  }),

  zipArchive: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.zip',
    targetPath: 'archive.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'archive.zip',
    sizeText: '743 bytes',
    typeText: 'Zip archive'
  }),

  zipArchiveSJIS: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive_sjis.zip',
    targetPath: 'archive_sjis.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Dec 21, 2018, 12:21 PM',
    nameText: 'archive_sjis.zip',
    sizeText: '160 bytes',
    typeText: 'Zip archive'
  }),

  zipArchiveMacOs: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive_macos.zip',
    targetPath: 'archive_macos.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Dec 21, 2018, 12:21 PM',
    nameText: 'archive_macos.zip',
    sizeText: '190 bytes',
    typeText: 'Zip archive'
  }),

  zipArchiveWithAbsolutePaths: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'absolute_paths.zip',
    targetPath: 'absolute_paths.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'absolute_paths.zip',
    sizeText: '400 bytes',
    typeText: 'Zip archive'
  }),

  zipArchiveEncrypted: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'encrypted.zip',
    targetPath: 'encrypted.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'encrypted.zip',
    sizeText: '589 bytes',
    typeText: 'Zip archive'
  }),

  debPackage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'package.deb',
    targetPath: 'package.deb',
    mimeType: 'application/vnd.debian.binary-package',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'package.deb',
    sizeText: '724 bytes',
    typeText: 'DEB file'
  }),

  tiniFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.tar.gz',
    targetPath: 'test.tini',
    mimeType: 'application/gzip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'test.tini',
    sizeText: '439 bytes',
    typeText: 'Crostini image file'
  }),

  hiddenFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: '.hiddenfile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 30, 2014, 3:30 PM',
    nameText: '.hiddenfile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text'
  }),

  // Team-drive entries.
  teamDriveA: new TestEntryInfo({
    type: EntryType.SHARED_DRIVE,
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: true,
      canShare: true,
    },
  }),

  teamDriveAFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'teamDriveAFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveAFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveADirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'teamDriveADirectory',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'teamDriveADirectory',
    sizeText: '--',
    typeText: 'Folder',
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: true,
      canShare: false,
    },
  }),

  teamDriveAHostedFile: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'teamDriveAHostedDoc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'teamDriveAHostedDoc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    teamDriveName: 'Team Drive A',
  }),

  teamDriveB: new TestEntryInfo({
    type: EntryType.SHARED_DRIVE,
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveBFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'teamDriveBFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveBFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveBDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'teamDriveBDirectory',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveBDirectory',
    sizeText: '--',
    typeText: 'Folder',
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  // Computer entries.
  computerA: new TestEntryInfo({
    type: EntryType.COMPUTER,
    computerName: 'Computer A',
    folderFeature: {
      isMachineRoot: true,
    },
  }),

  computerAFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'computerAFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'computerAFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    computerName: 'Computer A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: false,
      canShare: true,
    },
  }),

  computerAdirectoryA: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    computerName: 'Computer A',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder'
  }),

  // Read-only and write-restricted entries.
  // TODO(sashab): Generate all combinations of capabilities inside the test, to
  // ensure maximum coverage.

  // A folder that can't be renamed or deleted or have children added, but can
  // be copied and shared.
  readOnlyFolder: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Read-Only Folder',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Read-Only Folder',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  // A google doc file that can't be renamed or deleted, but can be copied and
  // shared.
  readOnlyDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Read-Only Doc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Read-Only Doc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  // A google doc file that can't be renamed, deleted, copied or shared.
  readOnlyStrictDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Read-Only (Strict) Doc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Read-Only (Strict) Doc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    capabilities: {
      canCopy: false,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: false
    },
  }),

  // A regular file that can't be renamed or deleted, but can be copied and
  // shared.
  readOnlyFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image4.jpg',
    targetPath: 'Read-Only File.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'Read-Only File.jpg',
    sizeText: '9 KB',
    typeText: 'JPEG image',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  // A regular file that can't be renamed, but can be deleted.
  deletableFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Deletable File.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'Deletable File.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: true
    },
  }),

  // A regular file that can't be deleted, but can be renamed.
  renamableFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Renamable File.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'Renamable File.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: true,
      canDelete: false
    },
  }),

  // Default Android directories.
  directoryDocuments: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Documents',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Documents',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  directoryMovies: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Movies',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Movies',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  directoryMusic: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Music',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Music',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  directoryPictures: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Pictures',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Pictures',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true
    },
  }),

  documentsText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Documents/android.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'android.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  neverSync: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'never-sync.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'never-sync.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text'
  }),

  sharedDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Shared Directory',
    sharedOption: SharedOption.SHARED_WITH_ME,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Shared Directory',
    sizeText: '--',
    typeText: 'Folder'
  }),

  sharedDirectoryFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Shared Directory/file.txt',
    mimeType: 'text/plain',
    sharedOption: SharedOption.NESTED_SHARED_WITH_ME,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'file.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text'
  }),

  crdownload: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'hello.crdownload',
    mimeType: 'application/octet-stream',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'hello.crdownload',
    sizeText: '51 bytes',
    typeText: 'CRDOWNLOAD file'
  }),
};

/**
 * Returns the count for |value| for the histogram |name|.
 * @param {string} name The histogram to be queried.
 * @param {number} value The value within that histogram to query.
 * @return {!Promise<number>} A promise fulfilled with the count.
 */
async function getHistogramCount(name, value) {
  return JSON.parse(await sendTestMessage({
    'name': 'getHistogramCount',
    'histogramName': name,
    'value': value,
  }));
}

/**
 * Returns the count for the user action |name|.
 * @param {string} name The user action to be queried.
 * @return {!Promise<number>} A promise fulfilled with the count.
 */
async function getUserActionCount(name) {
  return JSON.parse(await sendTestMessage({
    'name': 'getUserActionCount',
    'userActionName': name,
  }));
}

/**
 * Simulate Click in the UI in the middle of the element.
 * @param{string} appId ID of the app that contains the element. NOTE: The click
 *     is simulated on most recent window in the window system.
 * @param {string|!Array<string>} query Query to the element to be clicked.
 * @return {!Promise} A promise fulfilled after the click event.
 */
async function simulateUiClick(appId, query) {
  const element =
      await remoteCall.waitForElementStyles(appId, query, ['display']);
  chrome.test.assertTrue(!!element, 'element for simulateUiClick not found');

  // Find the middle of the element.
  const x = Math.floor(element.renderedLeft + (element.renderedWidth / 2));
  const y = Math.floor(element.renderedTop + (element.renderedHeight / 2));

  return sendTestMessage({name: 'simulateClick', 'clickX': x, 'clickY': y});
}
