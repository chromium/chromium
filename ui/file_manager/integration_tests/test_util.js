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
 *   sendTestMessage({
 *     name: 'addEntries', // command with volume and entries arguments
 *     volume: volume,
 *     entries: entries
 *   }).then(...);
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
 */
function checkIfNoErrorsOccuredOnApp(app, callback) {
  var countPromise = app.callRemoteTestUtil('getErrorCount', null, []);
  countPromise.then(function(count) {
    chrome.test.assertEq(0, count, 'The error count is not 0.');
    callback();
  });
}

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise to add the check to.
 * @param {Array<!RemoteCall>} apps An array of RemoteCall interfaces.
 */
function testPromiseAndApps(promise, apps) {
  promise.then(function() {
    return Promise.all(
        apps.map(function(app) {
          return new Promise(checkIfNoErrorsOccuredOnApp.bind(null, app));
        }));
  }).then(chrome.test.callbackPass(function() {
    // The callbackPass is necessary to avoid prematurely finishing tests.
    // Don't use chrome.test.succeed() here to avoid doubled success log.
  }), function(error) {
    chrome.test.fail(error.stack || error);
  });
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
    switch(pattern) {
      case '%s': return String(arg);
      case '%d': return Number(arg);
      case '%j': return JSON.stringify(arg);
      default: return pattern;
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
function repeatUntil(checkFunction) {
  var logTime = Date.now() + LOG_INTERVAL;
  var step = function() {
    return Promise.resolve(checkFunction()).then(function(result) {
      if (result instanceof pending) {
        if (Date.now() > logTime) {
          console.warn(result.message);
          logTime += LOG_INTERVAL;
        }
        return wait(REPEAT_UNTIL_INTERVAL).then(step);
      } else {
        return result;
      }
    });
  };
  return step();
}

/**
 * Sends the test |command| to the browser test harness and awaits a 'string'
 * result. Calls |callback| with that result.
 * @param {Object} command Test command to send. Refer to sendTestMessage()
 *    above for the expected format of a test |command| object.
 * @param {function(string)} callback Completion callback.
 * @param {Object=} opt_debug If truthy, log the result.
 */
function sendBrowserTestCommand(command, callback, opt_debug) {
  const caller = getCaller();
  if (typeof command.name !== 'string')
    chrome.test.fail('Invalid test command: ' + JSON.stringify(command));
  repeatUntil(function sendTestCommand() {
    const tryAgain = pending(caller, 'Sent BrowserTest ' + command.name);
    return sendTestMessage(command).then((result) => {
      if (typeof result !== 'string')
        return tryAgain;
      if (opt_debug)
        console.log('BrowserTest ' + command.name + ': ' + result);
      callback(result);
    }).catch((error) => {
      console.log(error.stack || error);
      return tryAgain;
    });
  });
}

/**
 * Adds the givin entries to the target volume(s).
 * @param {Array<string>} volumeNames Names of target volumes.
 * @param {Array<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)=} opt_callback Callback function to be passed the
 *     result of function. The argument is true on success.
 * @return {Promise} Promise to be fulfilled when the entries are added.
 */
function addEntries(volumeNames, entries, opt_callback) {
  if (volumeNames.length == 0) {
    callback(true);
    return;
  }
  var volumeResultPromises = volumeNames.map(function(volume) {
    return sendTestMessage({
      name: 'addEntries',
      volume: volume,
      entries: entries
    });
  });
  var resultPromise = Promise.all(volumeResultPromises);
  if (opt_callback) {
    resultPromise.then(opt_callback.bind(null, true),
                       opt_callback.bind(null, false));
  }
  return resultPromise;
}

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory',
  TEAM_DRIVE: 'team_drive',
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
  this.mimeType = options.mimeType || '';
  this.sharedOption = options.sharedOption || SharedOption.NONE;
  this.lastModifiedTime = options.lastModifiedTime;
  this.nameText = options.nameText;
  this.sizeText = options.sizeText;
  this.typeText = options.typeText;
  this.capabilities = options.capabilities;
  this.pinned = !!options.pinned;
  Object.freeze(this);
}

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
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

  // An image file without an extension, to confirm that file type detection
  // using mime types works fine.
  image2: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image2.png',
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

  // An ogg file without a mime type, to confirm that file type detection using
  // file extensions works fine.
  beautiful: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
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

  zipArchive: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.zip',
    targetPath: 'archive.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'archive.zip',
    sizeText: '533 bytes',
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
    type: EntryType.TEAM_DRIVE,
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
    type: EntryType.TEAM_DRIVE,
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
};
