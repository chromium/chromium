// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {importer} from '../../common/js/importer_common.js';
import {installMockChrome, MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {duplicateFinderInterfaces} from '../../externs/background/duplicate_finder.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {duplicateFinder} from './duplicate_finder.js';
import {MockVolumeManager} from './mock_volume_manager.js';
import {importerTestHistory} from './test_import_history.js';

/** @type {!duplicateFinder.DriveDuplicateFinder} */
let duplicateFinderTest;

/** @type {VolumeInfo} */
let drive;

/**
 * Map of file URL to hash code.
 * @type {!Object<string>}
 */
const hashes = {};

/**
 * Map of hash code to file URL.
 * @type {!Object<string>}
 */
const fileUrls = {};

/** @type {!MockFileSystem} */
let fileSystem;

/** @type {!importerTestHistory.TestImportHistory} */
let testHistory;

/** @type {duplicateFinderInterfaces.DispositionChecker.CheckerFunction} */
let getDisposition;

window.metrics = {
  recordTime: function() {},
};

export function setUp() {
  window.loadTimeData.getString = id => id;
  const mockChrome = {
    fileManagerPrivate: {
      /**
       * @param {!Entry} entry
       * @param {function(?string)} callback
       */
      computeChecksum: function(entry, callback) {
        callback(hashes[entry.toURL()] || null);
      },
      /**
       * @param {string} volumeId
       * @param {!Array<string>} hashes
       * @param {function(!Object<Array<string>>)} callback
       */
      searchFilesByHashes: function(volumeId, hashes, callback) {
        const result = {};
        hashes.forEach(
            /** @param {string} hash */
            hash => {
              result[hash] = fileUrls[hash] || [];
            });
        callback(result);
      },
    },
    runtime: {lastError: null},
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();
  fileSystem = new MockFileSystem('fake-filesystem');

  const volumeManager = new MockVolumeManager();
  drive = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  assertTrue(drive != null);

  MockVolumeManager.installMockSingleton(volumeManager);

  testHistory = new importerTestHistory.TestImportHistory();
  duplicateFinderTest = new duplicateFinder.DriveDuplicateFinder();
  getDisposition =
      duplicateFinder.DispositionCheckerImpl.createChecker(testHistory);
}

// Verifies the correct result when a duplicate exists.
export function testCheckDuplicateTrue(callback) {
  const filePaths = ['/foo.txt'];
  const fileHashes = ['abc123'];
  const files = setupHashes(filePaths, fileHashes);

  reportPromise(
      duplicateFinderTest.isDuplicate(files[0]).then(isDuplicate => {
        assertTrue(isDuplicate);
      }),
      callback);
}

// Verifies the correct result when a duplicate doesn't exist.
export function testCheckDuplicateFalse(callback) {
  const filePaths = ['/foo.txt'];
  const fileHashes = ['abc123'];
  const files = setupHashes(filePaths, fileHashes);

  // Make another file.
  const newFilePath = '/bar.txt';
  fileSystem.populate([newFilePath]);
  const newFile = /** @type {!FileEntry} */ (fileSystem.entries[newFilePath]);

  reportPromise(
      duplicateFinderTest.isDuplicate(newFile).then(isDuplicate => {
        assertFalse(isDuplicate);
      }),
      callback);
}

export function testDispositionChecker_ContentDupe(callback) {
  const filePaths = ['/foo.txt'];
  const fileHashes = ['abc123'];
  const files = setupHashes(filePaths, fileHashes);

  reportPromise(
      getDisposition(
          files[0], importer.Destination.GOOGLE_DRIVE,
          importer.ScanMode.CONTENT)
          .then(disposition => {
            assertEquals(importer.Disposition.CONTENT_DUPLICATE, disposition);
          }),
      callback);
}

export function testDispositionChecker_HistoryDupe(callback) {
  const filePaths = ['/foo.txt'];
  const fileHashes = ['abc123'];
  const files = setupHashes(filePaths, fileHashes);

  testHistory.importedPaths['/foo.txt'] = [importer.Destination.GOOGLE_DRIVE];

  reportPromise(
      getDisposition(
          files[0], importer.Destination.GOOGLE_DRIVE,
          importer.ScanMode.CONTENT)
          .then(disposition => {
            assertEquals(importer.Disposition.HISTORY_DUPLICATE, disposition);
          }),
      callback);
}

export function testDispositionChecker_Original(callback) {
  const filePaths = ['/foo.txt'];
  const fileHashes = ['abc123'];
  const files = setupHashes(filePaths, fileHashes);

  const newFilePath = '/bar.txt';
  fileSystem.populate([newFilePath]);
  const newFile = /** @type {!FileEntry} */ (fileSystem.entries[newFilePath]);

  reportPromise(
      getDisposition(
          newFile, importer.Destination.GOOGLE_DRIVE, importer.ScanMode.CONTENT)
          .then(disposition => {
            assertEquals(importer.Disposition.ORIGINAL, disposition);
          }),
      callback);
}

/**
 * @param {!Array<string>} filePaths
 * @param {!Array<string>} fileHashes
 * @return {!Array<!FileEntry>} Created files.
 */
function setupHashes(filePaths, fileHashes) {
  // Set up a filesystem with some files.
  fileSystem.populate(filePaths);

  const files = filePaths.map(filename => {
    return fileSystem.entries[filename];
  });

  files.forEach((file, index) => {
    hashes[file.toURL()] = fileHashes[index];
    fileUrls[fileHashes[index]] = file.toURL();
  });

  return files;
}
