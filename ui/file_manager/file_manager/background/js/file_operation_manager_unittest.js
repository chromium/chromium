// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {EntryLocation} from '../../externs/entry_location.js';

import {FileOperationManagerImpl} from './file_operation_manager.js';
import {fileOperationUtil} from './file_operation_util.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * Fake volume manager.
 */
class FakeVolumeManager {
  /**
   * Returns fake volume info.
   * @param {!Entry} entry
   * @return {!Object}
   */
  getVolumeInfo(entry) {
    return {volumeId: entry.filesystem.name};
  }
  /**
   * Return fake location info.
   * @param {!Entry} entry
   * @return {?EntryLocation}
   */
  getLocationInfo(entry) {
    return /** @type {!EntryLocation} */ ({
      rootType: 'downloads',
      volumeInfo:
          {volumeType: 'downloads', label: 'Downloads', remoteMountPath: ''},
    });
  }
}

/**
 * Returns file system of the url.
 * @param {!Array<!MockFileSystem>} fileSystems
 * @param {string} url
 * @return {!MockFileSystem}
 */
function getFileSystemForURL(fileSystems, url) {
  for (let i = 0; i < fileSystems.length; i++) {
    if (new RegExp('^filesystem:' + fileSystems[i].name + '/').test(url)) {
      return fileSystems[i];
    }
  }

  throw new Error('Unexpected url: ' + url);
}

/**
 * Size of directory.
 * @type {number}
 * @const
 */
const DIRECTORY_SIZE = -1;

/**
 * Creates test file system.
 * @param {string} id File system Id.
 * @param {Object<number>} entries Map of entry paths and their size.
 *     If the entry size is DIRECTORY_SIZE, the entry is a directory.
 * @return {!MockFileSystem}
 */
function createTestFileSystem(id, entries) {
  const fileSystem = new MockFileSystem(id, 'filesystem:' + id);
  for (const path in entries) {
    if (entries[path] === DIRECTORY_SIZE) {
      fileSystem.entries[path] = MockDirectoryEntry.create(fileSystem, path);
    } else {
      const metadata = /** @type {!Metadata} */ ({size: entries[path]});
      fileSystem.entries[path] =
          MockFileEntry.create(fileSystem, path, metadata);
    }
  }
  return fileSystem;
}

/**
 * Placeholder for mocked volume manager.
 * @type {(FakeVolumeManager|{getVolumeInfo: function()}?)}
 */
let volumeManager;

/**
 * Provide VolumeManager.getInstance() for FileOperationManager using mocked
 * volume manager instance.
 * @return {Promise}
 */
volumeManagerFactory.getInstance = () => {
  return Promise.resolve(volumeManager);
};

/**
 * Test target.
 * @type {FileOperationManagerImpl}
 */
let fileOperationManager;

/**
 * Initializes the test environment.
 */
export function setUp() {
  loadTimeData.overrideValues({'FILES_TRASH_ENABLED': true});
}

/**
 * Tests the fileOperationUtil.resolvePath function.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testResolvePath(callback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file': 10,
    '/directory': DIRECTORY_SIZE,
  });
  const root = fileSystem.root;
  const rootPromise = fileOperationUtil.resolvePath(root, '/');
  const filePromise = fileOperationUtil.resolvePath(root, '/file');
  const directoryPromise = fileOperationUtil.resolvePath(root, '/directory');
  const errorPromise =
      fileOperationUtil.resolvePath(root, '/not_found')
          .then(
              () => {
                assertTrue(false, 'The NOT_FOUND error is not reported.');
              },
              error => {
                return error.name;
              });
  reportPromise(
      Promise
          .all([
            rootPromise,
            filePromise,
            directoryPromise,
            errorPromise,
          ])
          .then(results => {
            assertArrayEquals(
                [
                  fileSystem.entries['/'],
                  fileSystem.entries['/file'],
                  fileSystem.entries['/directory'],
                  'NotFoundError',
                ],
                results);
          }),
      callback);
}

/**
 * Tests the fileOperationUtil.deduplicatePath
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testDeduplicatePath(callback) {
  const fileSystem1 = createTestFileSystem('testVolume', {'/': DIRECTORY_SIZE});
  const fileSystem2 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
  });
  const fileSystem3 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/file (6).txt': 10,
    '/file (7).txt': 10,
    '/file (8).txt': 10,
    '/file (9).txt': 10,
  });

  const nonExistingPromise =
      fileOperationUtil.deduplicatePath(fileSystem1.root, 'file.txt')
          .then(path => {
            assertEquals('file.txt', path);
          });
  const existingPathPromise =
      fileOperationUtil.deduplicatePath(fileSystem2.root, 'file.txt')
          .then(path => {
            assertEquals('file (1).txt', path);
          });
  const moreExistingPathPromise =
      fileOperationUtil.deduplicatePath(fileSystem3.root, 'file.txt')
          .then(path => {
            assertEquals('file (10).txt', path);
          });

  const testPromise = Promise.all([
    nonExistingPromise,
    existingPathPromise,
    moreExistingPathPromise,
  ]);
  reportPromise(testPromise, callback);
}

/**
 * Test writeFile() with file dragged from browser.
 */
export async function testWriteFile(done) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/testdir': DIRECTORY_SIZE,
  });
  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();
  const file = new File(['content'], 'browserfile', {type: 'text/plain'});
  await fileOperationManager.writeFile(
      file, /** @type {!DirectoryEntry} */ (fileSystem.entries['/testdir']));
  const writtenEntry = fileSystem.entries['/testdir/browserfile'];
  assertEquals('content', await writtenEntry.content.text());
  done();
}
