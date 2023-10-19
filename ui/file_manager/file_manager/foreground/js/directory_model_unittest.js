// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockFileOperationManager} from '../../background/js/mock_file_operation_manager.js';
import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';

/**
 * @type {?function(!chrome.fileManagerPrivate.ProgressStatus):void}
 */
let onIOTaskProgressStatusCallback;

/**
 * Initializes the test environment.
 */
export function setUp() {
  /**
   * Mock chrome APIs.
   * @type {!Object}
   */
  const mockChrome = {
    fileManagerPrivate: {
      onIOTaskProgressStatus: {
        /**
         * @param {?function(!chrome.fileManagerPrivate.ProgressStatus):void}
         *     callback
         */
        addListener(callback) {
          onIOTaskProgressStatusCallback = callback;
        },
      },
    },
  };

  // Install mock chrome APIs.
  installMockChrome(mockChrome);
}

/**
 * Mock DirectoryModel's dependencies and return a DirectoryModel instance.
 *
 * @returns {!DirectoryModel}
 */
function getDirectoryModel() {
  const volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);
  const fileFilter = new FileFilter(volumeManager);
  const metadataModel = new MockMetadataModel({});
  const fileOperationManager = new MockFileOperationManager();
  return new DirectoryModel(
      false, fileFilter, metadataModel, volumeManager, fileOperationManager);
}

/**
 * Tests that the fake directory will be re-scanned after the delete and copy
 * operation.
 */
export function testRecanAfterDeletionForRecents() {
  const deleteEvent = /** @type {chrome.fileManagerPrivate.ProgressStatus} */ ({
    type: chrome.fileManagerPrivate.IOTaskType.DELETE,
    state: chrome.fileManagerPrivate.IOTaskState.SUCCESS,
  });
  const copyEvent = /** @type {chrome.fileManagerPrivate.ProgressStatus} */ ({
    type: chrome.fileManagerPrivate.IOTaskType.COPY,
    state: chrome.fileManagerPrivate.IOTaskState.SUCCESS,
  });

  const directoryModel = getDirectoryModel();
  let isRescanCalled = false;
  directoryModel.rescanLater = () => {
    isRescanCalled = true;
  };

  // Current directory is not Recent.
  directoryModel.getCurrentRootType = () =>
      VolumeManagerCommon.RootType.DOWNLOADS;
  onIOTaskProgressStatusCallback(deleteEvent);
  assertFalse(isRescanCalled);
  onIOTaskProgressStatusCallback(copyEvent);
  assertFalse(isRescanCalled);

  // Current directory is Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;
  onIOTaskProgressStatusCallback(deleteEvent);
  assertTrue(isRescanCalled);
  isRescanCalled = false;
  onIOTaskProgressStatusCallback(copyEvent);
  assertTrue(isRescanCalled);
}

/**
 * Tests that the non-watchable volume will be re-scanned after each of
 * the IOTask operations.
 */
export function testRescanAfterIOTaskOperationOnlyForNonWatchableVolume() {
  const directoryModel = getDirectoryModel();
  let isRescanCalled = false;
  directoryModel.rescanLater = () => {
    isRescanCalled = true;
  };
  // Current directory is non-watchable.
  directoryModel.getCurrentVolumeInfo = () => {
    return {
      watchable: false,
    };
  };

  /** @type {!Array<!chrome.fileManagerPrivate.IOTaskType>} */
  const operations = [
    chrome.fileManagerPrivate.IOTaskType.COPY,
    chrome.fileManagerPrivate.IOTaskType.DELETE,
    chrome.fileManagerPrivate.IOTaskType.EMPTY_TRASH,
    chrome.fileManagerPrivate.IOTaskType.EXTRACT,
    chrome.fileManagerPrivate.IOTaskType.MOVE,
    chrome.fileManagerPrivate.IOTaskType.RESTORE,
    chrome.fileManagerPrivate.IOTaskType.RESTORE_TO_DESTINATION,
    chrome.fileManagerPrivate.IOTaskType.TRASH,
    chrome.fileManagerPrivate.IOTaskType.ZIP,
  ];

  for (const operation of operations) {
    const event = /** @type {chrome.fileManagerPrivate.ProgressStatus} */ ({
      type: operation,
      state: chrome.fileManagerPrivate.IOTaskState.SUCCESS,
    });

    isRescanCalled = false;
    onIOTaskProgressStatusCallback(event);
    assertTrue(isRescanCalled);
  }
}
