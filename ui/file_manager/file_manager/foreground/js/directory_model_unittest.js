// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
  // Mock loadTimeData strings.
  loadTimeData.resetForTesting({
    DRIVE_DIRECTORY_LABEL: 'Google Drive',
    DRIVE_OFFLINE_COLLECTION_LABEL: 'Offline',
    DRIVE_SHARED_WITH_ME_COLLECTION_LABEL: 'Shared with me',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
    FILTERS_IN_RECENTS_V2_ENABLED: true,
  });

  /**
   * Mock chrome APIs.
   * @type {!Object}
   */
  const mockChrome = {
    fileManagerPrivate: {
      SourceRestriction: {
        ANY_SOURCE: 'any_source',
      },
      RecentFileType: {
        ALL: 'all',
      },
      SearchType: {
        EXCLUDE_DIRECTORIES: 'EXCLUDE_DIRECTORIES',
        SHARED_WITH_ME: 'SHARED_WITH_ME',
        OFFLINE: 'OFFLINE',
        ALL: 'ALL',
      },
      DriveConnectionStateType: {
        OFFLINE: 'OFFLINE',
      },
      onIOTaskProgressStatus: {
        /**
         * @param {?function(!chrome.fileManagerPrivate.ProgressStatus):void}
         *     callback
         */
        addListener(callback) {
          onIOTaskProgressStatusCallback = callback;
        },
      },
      IOTaskType: {
        DELETE: 'delete',
        COPY: 'copy',
      },
      IOTaskState: {
        SUCCESS: 'success',
      },
      onDirectoryChanged: {addListener(callback) {}},
      getRecentFiles: () => {},
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
 * Tests that the directory will be re-scanned after the delete operation.
 */
export function testRecanAfterDeletionForRecents() {
  const deleteEvent = /** @type {chrome.fileManagerPrivate.ProgressStatus} */ ({
    type: chrome.fileManagerPrivate.IOTaskType.DELETE,
    state: chrome.fileManagerPrivate.IOTaskState.SUCCESS,
  });
  const otherEvent = /** @type {chrome.fileManagerPrivate.ProgressStatus} */ ({
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
  onIOTaskProgressStatusCallback(otherEvent);
  assertFalse(isRescanCalled);

  // Current directory is Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;
  onIOTaskProgressStatusCallback(deleteEvent);
  assertTrue(isRescanCalled);
  isRescanCalled = false;
  onIOTaskProgressStatusCallback(otherEvent);
  assertFalse(isRescanCalled);
}
