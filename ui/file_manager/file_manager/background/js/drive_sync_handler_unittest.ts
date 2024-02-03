// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';

import {DriveSyncHandlerImpl} from './drive_sync_handler.js';
import {MockProgressCenter} from './mock_progress_center.js';

/**
 * Global progress center object.
 */
let progressCenter: MockProgressCenter;

/**
 * Global DriveSyncHandler object.
 */
let driveSyncHandler: DriveSyncHandlerImpl;

/**
 * Mock chrome APIs.
 */
const mockChrome: any = {};

mockChrome.fileManagerPrivate = {
  onDriveSyncError: {
    addListener: function(
        callback: (error: chrome.fileManagerPrivate.DriveSyncErrorEvent) =>
            void) {
      mockChrome.fileManagerPrivate.onDriveSyncError.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onDriveSyncError.listener_ = null;
    },
    listener_: null,
  },
  onDriveConfirmDialog: {
    addListener: function(
        callback:
            (confirmEvent: chrome.fileManagerPrivate.DriveConfirmDialogEvent) =>
                void) {
      mockChrome.fileManagerPrivate.onDriveConfirmDialog.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onDriveConfirmDialog.listener_ = null;
    },
    listener_: null,
  },
  onPreferencesChanged: {
    addListener: function(callback: VoidCallback) {
      mockChrome.fileManagerPrivate.onPreferencesChanged.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onPreferencesChanged.listener_ = null;
    },
    listener_: null,
  },
  onDriveConnectionStatusChanged: {
    addListener: function(callback: VoidCallback) {
      mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_ =
          callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_ =
          null;
    },
    listener_: null,
  },
  onMountCompleted: {
    addListener: function(
        callback: (completedEvent:
                       chrome.fileManagerPrivate.MountCompletedEvent) => void) {
      mockChrome.fileManagerPrivate.onMountCompleted.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onMountCompleted.listener_ = null;
    },
    listener_: null,
  },
  getDriveConnectionState: function(
      callback: (result: chrome.fileManagerPrivate.DriveConnectionState) =>
          void) {
    callback({
      type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
      reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_NETWORK,
    });
  },
};

mockChrome.notifications = {
  onButtonClicked: {
    addListener: function(callback: any) {
      mockChrome.notifications.onButtonClicked.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.notifications.onButtonClicked.listener_ = null;
    },
    listener_: null,
  },
  onClosed: {
    addListener: function(callback: any) {
      mockChrome.notifications.onClosed.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.notifications.onClosed.listener_ = null;
    },
    listener_: null,
  },
};

/**
 * Stub out file URLs handling.
 */
window.webkitResolveLocalFileSystemURL =
    (url: string, successCallback: FileSystemEntryCallback,
     _?: ErrorCallback) => {
      successCallback({name: url} as Entry);
    };

// Set up the test components.
export function setUp() {
  loadTimeData.overrideValues({INLINE_SYNC_STATUS: false});

  // Install mock chrome APIs.
  installMockChrome(mockChrome);

  // Create a mock ProgressCenter.
  progressCenter = new MockProgressCenter();

  // Create DriveSyncHandlerImpl.
  driveSyncHandler = new DriveSyncHandlerImpl(progressCenter);
  driveSyncHandler.metadataModel = new MockMetadataModel({});
}

// Test that in general case item IDs produced for errors are unique.
export function testUniqueErrorIds() {
  // Dispatch an event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, progressCenter.getItemCount());

  // Dispatch another event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created a second item.
  assertEquals(2, progressCenter.getItemCount());
}

// Test that item IDs produced for quota errors are same.
export function testErrorDedupe() {
  // Dispatch an event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, progressCenter.getItemCount());

  // Dispatch another event.
  mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this did not create a second item.
  assertEquals(1, progressCenter.getItemCount());
}

export function testErrorWithoutPath() {
  const originalStub = window.webkitResolveLocalFileSystemURL;
  /**
   * Temporary stub the entry resolving to always fail.
   */
  window.webkitResolveLocalFileSystemURL = (_url, _success, errorCallback) => {
    errorCallback({} as FileError);
  };

  try {
    // Dispatch an event.
    mockChrome.fileManagerPrivate.onDriveSyncError.listener_({
      type: 'service_unavailable',
      fileUrl: '',
    });

    // Check that this created one item.
    assertEquals(1, progressCenter.getItemCount());
  } finally {
    window.webkitResolveLocalFileSystemURL = originalStub;
  }
}
