// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../definitions/file_manager_private.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockProgressCenter} from '../../background/js/mock_progress_center.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {toFilesAppURL} from '../../common/js/url_constants.js';
import {util} from '../../common/js/util.js';

import {DriveSyncHandlerImpl} from './drive_sync_handler.js';

/**
 * Global progress center object.
 */
let progressCenter: MockProgressCenter;

/**
 * Global DriveSyncHandler object.
 */
let driveSyncHandler: DriveSyncHandlerImpl;

/**
 * Converts a `name` to a filesystem URL.
 */
function asFileURL(name: string) {
  return 'filesystem:' + toFilesAppURL(`external/${name}`).toString();
}

/**
 * Mock chrome APIs.
 */
const mockChrome: any = {};

util.isInlineSyncStatusEnabled = () => false;

mockChrome.fileManagerPrivate = {
  onFileTransfersUpdated: {
    addListener: function(
        callback: (status: chrome.fileManagerPrivate.FileTransferStatus) =>
            void) {
      mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = null;
    },
    listener_: null,
  },
  onPinTransfersUpdated: {
    addListener: function(
        callback: (status: chrome.fileManagerPrivate.FileTransferStatus) =>
            void) {
      mockChrome.fileManagerPrivate.onPinTransfersUpdated.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onPinTransfersUpdated.listener_ = null;
    },
    listener_: null,
  },
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
      callback: chrome.fileManagerPrivate.GetDriveConnectionStateCallback) {
    callback({
      type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
      reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_NETWORK,
      hasCellularNetworkAccess: false,
      canPinHostedFiles: false,
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
  // Install mock chrome APIs.
  installMockChrome(mockChrome);

  // Create a mock ProgressCenter.
  progressCenter = new MockProgressCenter();

  // Create DriveSyncHandlerImpl.
  driveSyncHandler = new DriveSyncHandlerImpl(progressCenter);

  // Check: Drive sync is enabled at creation time.
  assertFalse(driveSyncHandler.isSyncSuppressed());
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

// Test offline.
export async function testOffline() {
  // Start a transfer.
  await mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'in_progress',
    processed: 50.0,
    total: 100.0,
    numTotalJobs: 1,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // Check that this created one progressing item.
  assertEquals(
      1, progressCenter.getItemsByState(ProgressItemState.PROGRESSING).length);
  let item = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.PROGRESSING, item.state);
  assertTrue(driveSyncHandler.syncing);

  // Go offline.
  mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged.listener_();

  // Check that this item was cancelled.
  // There are two items cancelled including the pin item.
  assertEquals(
      2, progressCenter.getItemsByState(ProgressItemState.CANCELED).length);
  item = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.CANCELED, item.state);
  assertFalse(driveSyncHandler.syncing);
}

// Test transfer status updates.
export async function testTransferUpdate() {
  // Start a pin transfer.
  await mockChrome.fileManagerPrivate.onPinTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'in_progress',
    processed: 50.0,
    total: 100.0,
    numTotalJobs: 1,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // There should be one progressing pin item and one canceled sync item.
  assertEquals(2, progressCenter.getItemCount());
  let syncItem = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.CANCELED, syncItem.state);
  let pinItem = progressCenter.getItemById('drive-pin');
  assertEquals(ProgressItemState.PROGRESSING, pinItem.state);

  // Start a sync transfer.
  await mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'in_progress',
    processed: 25.0,
    total: 100.0,
    numTotalJobs: 1,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // There should be two progressing items.
  assertEquals(2, progressCenter.getItemCount());
  assertEquals(
      2, progressCenter.getItemsByState(ProgressItemState.PROGRESSING).length);

  // Finish the pin transfer.
  await mockChrome.fileManagerPrivate.onPinTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'completed',
    processed: 100.0,
    total: 100.0,
    numTotalJobs: 0,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // There should be one completed pin item and one progressing sync item.
  assertEquals(2, progressCenter.getItemCount());
  syncItem = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.PROGRESSING, syncItem.state);
  pinItem = progressCenter.getItemById('drive-pin');
  assertEquals(ProgressItemState.COMPLETED, pinItem.state);

  // Fail the sync transfer.
  await mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'failed',
    processed: 40.0,
    total: 100.0,
    numTotalJobs: 0,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // There should be one completed pin item and one canceled sync item.
  assertEquals(2, progressCenter.getItemCount());
  syncItem = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.CANCELED, syncItem.state);
  pinItem = progressCenter.getItemById('drive-pin');
  assertEquals(ProgressItemState.COMPLETED, pinItem.state);
}

// Test transfer status updates when notifications should be hidden.
export async function
testTransferUpdateNoNotificationPartiallyIgnoredTransferUpdates() {
  // Start a sync transfer.
  await mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'in_progress',
    processed: 50.0,
    total: 100.0,
    numTotalJobs: 2,
    showNotification: true,
    hideWhenZeroJobs: true,
  });

  // There should be one progressing sync item and one canceled pin item.
  assertEquals(2, progressCenter.getItemCount());
  let syncItem = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.PROGRESSING, syncItem.state);
  const pinItem = progressCenter.getItemById('drive-pin');
  assertEquals(ProgressItemState.CANCELED, pinItem.state);

  // In the event where the syncing paths are ignored, the following transfer
  // status is received with `show_notification = false`.
  await mockChrome.fileManagerPrivate.onFileTransfersUpdated.listener_({
    fileUrl: asFileURL('name'),
    transferState: 'in_progress',
    processed: 0.0,
    total: 0.0,
    numTotalJobs: 0,
    showNotification: false,
    hideWhenZeroJobs: true,
  });

  // The progressing item should be hidden.
  syncItem = progressCenter.getItemById('drive-sync');
  assertEquals(ProgressItemState.CANCELED, syncItem.state);
}
