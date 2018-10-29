// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// All testing functions in namespace 'test'.
var test = test || {};

// Update paths for testing.
constants.FILES_QUICK_VIEW_HTML = 'test/gen/foreground/elements/files_quick_view.html';
constants.DRIVE_WELCOME_CSS = FILE_MANAGER_ROOT + constants.DRIVE_WELCOME_CSS;

test.FILE_MANAGER_EXTENSION_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

// Stores Blobs loaded from src/chrome/test/data/chromeos/file_manager.
test.DATA = {
  'archive.zip': null,
  'image.png': null,
  'image2.png': null,
  'image3.jpg': null,
  'music.ogg': null,
  'package.deb': null,
  'random.bin': null,
  'text.txt': null,
  'video.ogv': null,
};

// Load DATA from local filesystem.
test.loadData = function() {
  return Promise.all(Object.keys(test.DATA).map(filename => {
    return new Promise(resolve => {
      var req = new XMLHttpRequest();
      req.responseType = 'blob';
      req.onload = () => {
        test.DATA[filename] = req.response;
        resolve();
      };
      req.open(
          'GET',
          FILE_MANAGER_ROOT +
              '../../../chrome/test/data/chromeos/file_manager/' + filename);
      req.send();
    });
  }));
};

/**
 * @enum {string}
 * @const
 */
test.EntryType = {
  FILE: 'file',
  DIRECTORY: 'directory'
};

/**
 * @enum {string}
 * @const
 */
test.SharedOption = {
  NONE: 'none',
  SHARED: 'shared'
};

/**
 * File system entry information for tests.
 *
 * @param {test.EntryType} type Entry type.
 * @param {string} sourceFileName Source file name that provides file contents.
 * @param {string} targetPath Path of entry on the test file system.
 * @param {string} mimeType Mime type.
 * @param {test.SharedOption} sharedOption Shared option.
 * @param {string} lastModifiedTime Last modified time as a text to be shown in
 *     the last modified column.
 * @param {string} nameText File name to be shown in the name column.
 * @param {string} sizeText Size text to be shown in the size column.
 * @param {string} typeText Type name to be shown in the type column.
 * @constructor
 */
test.TestEntryInfo = function(type,
                       sourceFileName,
                       targetPath,
                       mimeType,
                       sharedOption,
                       lastModifiedTime,
                       nameText,
                       sizeText,
                       typeText) {
  this.type = type;
  this.sourceFileName = sourceFileName || '';
  this.targetPath = targetPath;
  this.mimeType = mimeType || '';
  this.sharedOption = sharedOption;
  this.lastModifiedTime = lastModifiedTime;
  this.nameText = nameText;
  this.sizeText = sizeText;
  this.typeText = typeText;
  Object.freeze(this);
};

test.TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
};

/**
 * Returns 4-typle name, size, type, date as shown in file list.
 */
test.TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

test.TestEntryInfo.getMockFileSystemPopulateRows = function(entries, prefix) {
  return entries.map(function(entry) {
    return entry.getMockFileSystemPopulateRow(prefix);
  });
};

/**
 * Returns object {fullPath: ..., metadata: {...}, content: ...} as used in
 * MockFileSystem.populate.
 */
test.TestEntryInfo.prototype.getMockFileSystemPopulateRow = function(prefix) {
  var suffix = this.type == test.EntryType.DIRECTORY ? '/' : '';
  var content = test.DATA[this.sourceFileName];
  var size = content && content.size || 0;
  return {
    fullPath: prefix + this.nameText + suffix,
    metadata: {
      size: size,
      modificationTime: new Date(Date.parse(this.lastModifiedTime)),
      contentMimeType: this.mimeType,
      hosted: this.mimeType == 'application/vnd.google-apps.document',
    },
    content: content
  };
};

/**
 * Filesystem entries used by the test cases.
 * @type {Object<test.TestEntryInfo>}
 * @const
 */
test.ENTRIES = {
  hello: new test.TestEntryInfo(
      test.EntryType.FILE, 'text.txt', 'hello.txt', 'text/plain',
      test.SharedOption.NONE, 'Sep 4, 1998, 12:34 PM', 'hello.txt', '51 bytes',
      'Plain text'),

  world: new test.TestEntryInfo(
      test.EntryType.FILE, 'video.ogv', 'world.ogv', 'video/ogg',
      test.SharedOption.NONE, 'Jul 4, 2012, 10:35 AM', 'world.ogv', '59 KB',
      'OGG video'),

  unsupported: new test.TestEntryInfo(
      test.EntryType.FILE, 'random.bin', 'unsupported.foo', 'application/x-foo',
      test.SharedOption.NONE, 'Jul 4, 2012, 10:36 AM', 'unsupported.foo',
      '8 KB', 'FOO file'),

  desktop: new test.TestEntryInfo(
      test.EntryType.FILE, 'image.png', 'My Desktop Background.png',
      'image/png', test.SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'My Desktop Background.png', '272 bytes', 'PNG image'),

  // An image file without an extension, to confirm that file type detection
  // using mime types works fine.
  image2: new test.TestEntryInfo(
      test.EntryType.FILE, 'image2.png', 'image2', 'image/png',
      test.SharedOption.NONE, 'Jan 18, 2038, 1:02 AM', 'image2', '4 KB',
      'PNG image'),

  image3: new test.TestEntryInfo(
      test.EntryType.FILE, 'image3.jpg', 'image3.jpg', 'image/jpeg',
      test.SharedOption.NONE, 'Jan 18, 2038, 1:02 AM', 'image3.jpg', '3 KB',
      'JPEG image'),

  // An ogg file without a mime type, to confirm that file type detection using
  // file extensions works fine.
  beautiful: new test.TestEntryInfo(
      test.EntryType.FILE, 'music.ogg', 'Beautiful Song.ogg', '',
      test.SharedOption.NONE, 'Nov 12, 2086, 12:00 PM', 'Beautiful Song.ogg',
      '14 KB', 'OGG audio'),

  photos: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'photos', '', test.SharedOption.NONE,
      'Jan 1, 1980, 11:59 PM', 'photos', '--', 'Folder'),

  testDocument: new test.TestEntryInfo(
      test.EntryType.FILE, '', 'Test Document',
      'application/vnd.google-apps.document', test.SharedOption.NONE,
      'Apr 10, 2013, 4:20 PM', 'Test Document.gdoc', '--', 'Google document'),

  testSharedDocument: new test.TestEntryInfo(
      test.EntryType.FILE, '', 'Test Shared Document',
      'application/vnd.google-apps.document', test.SharedOption.SHARED,
      'Mar 20, 2013, 10:40 PM', 'Test Shared Document.gdoc', '--',
      'Google document'),

  newlyAdded: new test.TestEntryInfo(
      test.EntryType.FILE, 'music.ogg', 'newly added file.ogg', 'audio/ogg',
      test.SharedOption.NONE, 'Sep 4, 1998, 12:00 AM', 'newly added file.ogg',
      '14 KB', 'OGG audio'),

  directoryA: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'A', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'A', '--', 'Folder'),

  directoryB: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'A/B', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'B', '--', 'Folder'),

  directoryC: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'A/B/C', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'C', '--', 'Folder'),

  directoryD: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'D', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'D', '--', 'Folder'),

  directoryE: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'D/E', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'E', '--', 'Folder'),

  directoryF: new test.TestEntryInfo(
      test.EntryType.DIRECTORY, '', 'D/E/F', '', test.SharedOption.NONE,
      'Jan 1, 2000, 1:00 AM', 'F', '--', 'Folder'),

  zipArchive: new test.TestEntryInfo(
      test.EntryType.FILE, 'archive.zip', 'archive.zip', 'application/x-zip',
      test.SharedOption.NONE, 'Jan 1, 2014, 1:00 AM', 'archive.zip',
      '533 bytes', 'Zip archive'),

  debPackage: new test.TestEntryInfo(
      test.EntryType.FILE, 'package.deb', 'package.deb',
      'application/vnd.debian.binary-package', test.SharedOption.NONE,
      'Jan 1, 2014, 1:00 AM', 'package.deb', '724 bytes', 'DEB file'),

  hiddenFile: new test.TestEntryInfo(
      test.EntryType.FILE, 'text.txt', '.hiddenfile.txt', 'text/plain',
      test.SharedOption.NONE, 'Sep 30, 2014, 3:30 PM', '.hiddenfile.txt',
      '51 bytes', 'Plain text'),

  mhtml: new test.TestEntryInfo(
      test.EntryType.FILE, 'text.txt', 'hello.mhtml', 'text/html',
      test.SharedOption.NONE, 'Sep 4, 1998, 12:34 PM', 'hello.mhtml',
      '51 bytes', 'HTML document'),

  helloInA: new test.TestEntryInfo(
      test.EntryType.FILE, 'text.txt', 'hello.txt', 'text/plain',
      test.SharedOption.NONE, 'Sep 4, 1998, 12:34 PM', 'A/hello.txt',
      '51 bytes', 'Plain text'),
};

/**
 * Basic entry set for the local volume.
 * @type {!Array<!test.TestEntryInfo>}
 * @const
 */
test.BASIC_LOCAL_ENTRY_SET = [
  test.ENTRIES.hello,
  test.ENTRIES.world,
  test.ENTRIES.desktop,
  test.ENTRIES.beautiful,
  test.ENTRIES.photos
];

/**
 * Basic entry set for the drive volume.
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 *
 * @type {!Array<!test.TestEntryInfo>}
 * @const
 */
test.BASIC_DRIVE_ENTRY_SET = [
  test.ENTRIES.hello,
  test.ENTRIES.world,
  test.ENTRIES.desktop,
  test.ENTRIES.beautiful,
  test.ENTRIES.photos,
  test.ENTRIES.unsupported,
  test.ENTRIES.testDocument,
  test.ENTRIES.testSharedDocument
];

/**
 * Basic entry set for the local crostini volume.
 * @type {!Array<!test.TestEntryInfo>}
 * @const
 */
test.BASIC_CROSTINI_ENTRY_SET = [
  test.ENTRIES.directoryA,
  test.ENTRIES.hello,
  test.ENTRIES.world,
  test.ENTRIES.desktop,
];

/**
 * Number of times to repeat immediately before waiting REPEAT_UNTIL_INTERVAL.
 * @type {number}
 * @const
 */
test.REPEAT_UNTIL_IMMEDIATE_COUNT = 3;

/**
 * Interval (ms) between checks of repeatUntil.
 * @type {number}
 * @const
 */
test.REPEAT_UNTIL_INTERVAL = 100;

/**
 * Interval (ms) between log output of repeatUntil.
 * @type {number}
 * @const
 */
test.REPEAT_UNTIL_LOG_INTERVAL = 3000;

/**
 * Returns a pending marker. See also the repeatUntil function.
 * @param {string} message Pending reason including %s, %d, or %j markers. %j
 *     format an object as JSON.
 * @param {...*} var_args Values to be assigined to %x markers.
 * @return {Object} Object which returns true for the expression: obj instanceof
 *     pending.
 */
test.pending = function(message, var_args) {
  var index = 1;
  var args = arguments;
  var formattedMessage = message.replace(/%[sdj]/g, function(pattern) {
    var arg = args[index++];
    switch(pattern) {
      case '%s': return String(arg);
      case '%d': return Number(arg);
      case '%j': return JSON.stringify(arg);
      default: return pattern;
    }
  });
  var pendingMarker = Object.create(test.pending.prototype);
  pendingMarker.message = formattedMessage;
  return pendingMarker;
};

/**
 * Waits until the checkFunction returns a value which is not a pending marker.
 * @param {function():*} checkFunction Function to check a condition. It can
 *     return a pending marker created by a pending function.
 * @return {Promise} Promise to be fulfilled with the return value of
 *     checkFunction when the checkFunction reutrns a value but a pending
 *     marker.
 */
test.repeatUntil = function(checkFunction) {
  var logTime = Date.now() + test.REPEAT_UNTIL_LOG_INTERVAL;
  var loopCount = 0;
  var step = function() {
    loopCount++;
    return Promise.resolve(checkFunction()).then(function(result) {
      if (!(result instanceof test.pending)) {
        return result;
      }
      if (Date.now() > logTime) {
        console.warn(result.message);
        logTime += test.REPEAT_UNTIL_LOG_INTERVAL;
      }
      // Repeat immediately for the first few, then wait between repeats.
      var interval = loopCount <= test.REPEAT_UNTIL_IMMEDIATE_COUNT ?
          0 :
          test.REPEAT_UNTIL_INTERVAL;
      return new Promise(resolve => {
               setTimeout(resolve, interval);
             })
          .then(step);
    });
  };
  return step();
};

/**
 * Waits for the specified element appearing in the DOM.
 * @param {string} query Query string for the element.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
test.waitForElement = function(query) {
  return test.repeatUntil(() => {
    let element = document.querySelector(query);
    if (element)
      return element;
    return test.pending('Element %s is not found.', query);
  });
};

/**
 * Waits for the specified element leaving from the DOM.
 * @param {string} query Query string for the element.
 * @return {Promise} Promise to be fulfilled when the element is lost.
 */
test.waitForElementLost = function(query) {
  return test.repeatUntil(() => {
    var element = document.querySelector(query);
    if (element)
      return test.pending('Elements %s still exists.', query);
    return true;
  });
};

/**
 * Adds specified TestEntryInfos to downloads and drive.
 *
 * @param {!Array<!test.TestEntryInfo>} downloads Entries for downloads.
 * @param {!Array<!test.TestEntryInfo>} drive Entries for drive.
 * @param {!Array<!test.TestEntryInfo>} crostini Entries for crostini.
 */
test.addEntries = function(downloads, drive, crostini) {
  var fsDownloads = /** @type {MockFileSystem} */ (
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem);
  fsDownloads.populate(
      test.TestEntryInfo.getMockFileSystemPopulateRows(downloads, '/'), true);

  var fsDrive = /** @type {MockFileSystem} */ (
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE)
          .fileSystem);
  fsDrive.populate(
      test.TestEntryInfo.getMockFileSystemPopulateRows(drive, '/root/'), true);
  fsDrive.populate(['/team_drives/']);

  var fsCrostini = /** @type {MockFileSystem} */ (
      mockVolumeManager
          .createVolumeInfo(
              VolumeManagerCommon.VolumeType.CROSTINI, 'crostini',
              str('LINUX_FILES_ROOT_LABEL'))
          .fileSystem);
  fsCrostini.populate(
      test.TestEntryInfo.getMockFileSystemPopulateRows(crostini, '/'), true);
};

/**
 * Sends mount event for crostini volume.
 */
test.mountCrostini = function() {
  chrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    status: 'success',
    eventType: 'mount',
    volumeMetadata: {
      volumeType: VolumeManagerCommon.VolumeType.CROSTINI,
      volumeId: 'crostini',
      isReadOnly: false,
      iconSet: {},
      profile: {isCurrentProfile: true, displayName: ''},
      mountContext: 'user',
    },
  });
};

/**
 * Waits for the file list turns to the given contents.
 * @param {!Array<!Array<string>>} expected Expected contents of file list.
 * @param {{orderCheck:boolean, ignoreName:boolean, ignoreSize:boolean,
 *     ignoreType:boolean, ignoreDate:boolean}=} opt_options
 *     Options of the comparison. If orderCheck is true, it also compares the
 *     order of files. If ignore[Name|Size|Type|Date] is true, it compares
 *     the file without considering that field.
 * @return {Promise} Promise to be fulfilled when the file list turns to the
 *     given contents.
 */
test.waitForFiles = function(expected, opt_options) {
  var options = opt_options || {};
  var nextLog = Date.now() + test.REPEAT_UNTIL_LOG_INTERVAL;
  return test.repeatUntil(function() {
    var files = test.getFileList();
    if (Date.now() > nextLog) {
      console.debug('waitForFiles', expected, files);
      nextLog = Date.now() + test.REPEAT_UNTIL_LOG_INTERVAL;
    }
    if (!options.orderCheck) {
      files.sort();
      expected.sort();
    }

    if (((a, b) => {
          if (a.length != b.length)
            return false;
          for (var i = 0; i < files.length; i++) {
            // Each row is [name, size, type, date].
            if ((!options.ignoreName && a[i][0] != b[i][0]) ||
                (!options.ignoreSize && a[i][1] != b[i][1]) ||
                (!options.ignoreType && a[i][2] != b[i][2]) ||
                (!options.ignoreDate && a[i][3] != b[i][3]))
              return false;
          }
          return true;
        })(expected, files)) {
      return true;
    } else {
      return test.pending(
          'waitForFiles: expected: %j actual %j.', expected, files);
    }
  });
};

/**
 * Opens a Files app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 *
 * @param {Array<!test.TestEntryInfo>=} opt_downloads Entries for downloads.
 * @param {Array<!test.TestEntryInfo>=} opt_drive Entries for drive.
 * @param {Array<!test.TestEntryInfo>=} opt_crostini Entries for crostini.
 * @return {Promise} Promise to be fulfilled with the result object, which
 *     contains the file list.
 */
test.setupAndWaitUntilReady = function(opt_downloads, opt_drive, opt_crostini) {
  const entriesDownloads = opt_downloads || test.BASIC_LOCAL_ENTRY_SET;
  const entriesDrive = opt_drive || test.BASIC_DRIVE_ENTRY_SET;
  const entriesCrostini = opt_crostini || test.BASIC_CROSTINI_ENTRY_SET;

  // Copy some functions from test.util.sync and bind to main window.
  test.fakeMouseClick = test.util.sync.fakeMouseClick.bind(null, window);
  test.fakeMouseDoubleClick =
      test.util.sync.fakeMouseDoubleClick.bind(null, window);
  test.fakeMouseRightClick =
      test.util.sync.fakeMouseRightClick.bind(null, window);
  test.fakeKeyDown = test.util.sync.fakeKeyDown.bind(null, window);
  test.sendEvent = test.util.sync.sendEvent.bind(null, window);
  test.getFileList = test.util.sync.getFileList.bind(null, window);
  test.inputText = test.util.sync.inputText.bind(null, window);
  test.selectFile = test.util.sync.selectFile.bind(null, window);

  return test.loadData()
      .then(() => {
        test.addEntries(entriesDownloads, entriesDrive, entriesCrostini);
        return test.waitForElement(
            '#directory-tree [volume-type-icon="downloads"]');
      })
      .then((downloadsIcon) => {
        // Click Downloads, or refresh button if already on Downloads.
        assertTrue(test.fakeMouseClick(
            downloadsIcon.parentElement.hasAttribute('selected') ?
                '#refresh-button' :
                '#directory-tree [volume-type-icon="downloads"]'));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(entriesDownloads));
      });
};

/**
 * Shortcut for endTests with success.
 * @param {boolean=} opt_failed True indicates failure.
 */
test.done = function(opt_failed) {
  window.endTests(!opt_failed);
};

/**
 * @return {number} Maximum listitem-? id from #file-list.
 */
test.maxListItemId = function() {
  var listItems = document.querySelectorAll('#file-list .table-row');
  if (!listItems)
    return 0;
  return Math.max(...Array.from(listItems).map(e => {
    return e.id.replace('listitem-', '');
  }));
};

/**
 * @return {number} Minium listitem-? id from #file-list.
 */
test.minListItemId = function() {
  var listItems = document.querySelectorAll('#file-list .table-row');
  if (!listItems)
    return 0;
  return Math.min(...Array.from(listItems).map(e => {
    return e.id.replace('listitem-', '');
  }));
};
