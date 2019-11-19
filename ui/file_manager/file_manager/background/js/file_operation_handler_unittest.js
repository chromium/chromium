// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {!MockFileOperationManager} */
let fileOperationManager;

/** @type {!MockProgressCenter} */
let progressCenter;

/** @type {!FileOperationHandler} */
let fileOperationHandler;

// Set up the test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.data = {
    COPY_FILE_NAME: 'Copying $1...',
    COPY_TARGET_EXISTS_ERROR: '$1 is already exists.',
    COPY_FILESYSTEM_ERROR: 'Copy filesystem error: $1',
    FILE_ERROR_GENERIC: 'File error generic.',
    COPY_UNEXPECTED_ERROR: 'Copy unexpected error: $1'
  };

  // Create mock items needed for FileOperationHandler.
  fileOperationManager = new MockFileOperationManager();
  progressCenter = new MockProgressCenter();

  // Create FileOperationHandler. Note: the file operation handler is
  // required, but not used directly, by the unittests.
  fileOperationHandler =
      new FileOperationHandler(fileOperationManager, progressCenter);
}

/**
 * Tests copy success.
 */
function testCopySuccess() {
  // Dispatch copy event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.BEGIN,
        status: {
          operationType: 'COPY',
          numRemainingItems: 1,
          processingEntryName: 'sample.txt',
          totalBytes: 200,
          processedBytes: 0,
        },
      })));

  // Check the updated item.
  let item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.PROGRESSING, item.state);
  assertEquals('TASK_ID', item.id);
  assertEquals('Copying sample.txt...', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);

  // Dispatch success event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.SUCCESS,
        status: {
          operationType: 'COPY',
        },
      })));

  // Check the item completed.
  item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.COMPLETED, item.state);
  assertEquals('TASK_ID', item.id);
  assertEquals('', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(100, item.progressRateInPercent);
}

/**
 * Tests copy cancel.
 */
function testCopyCancel() {
  // Dispatch copy event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.BEGIN,
        status: {
          operationType: 'COPY',
          numRemainingItems: 1,
          processingEntryName: 'sample.txt',
          totalBytes: 200,
          processedBytes: 0,
        },
      })));

  // Check the updated item.
  let item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.PROGRESSING, item.state);
  assertEquals('Copying sample.txt...', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);

  // Setup cancel event.
  fileOperationManager.cancelEvent =
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.CANCELED,
        status: {
          operationType: 'COPY',
        },
      }));

  // Dispatch cancel event.
  assertTrue(item.cancelable);
  item.cancelCallback();

  // Check the item cancelled.
  item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.CANCELED, item.state);
  assertEquals('', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);
}

/**
 * Tests target already exists error.
 */
function testCopyTargetExistsError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.ERROR,
        status: {
          operationType: 'COPY',
        },
        error: {
          code: util.FileOperationErrorType.TARGET_EXISTS,
          data: {
            name: 'sample.txt',
          },
        },
      })));

  // Check the item errored.
  const item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.ERROR, item.state);
  assertEquals('sample.txt is already exists.', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);
}

/**
 * Tests file system error.
 */
function testCopyFileSystemError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.ERROR,
        status: {
          operationType: 'COPY',
        },
        error: {
          code: util.FileOperationErrorType.FILESYSTEM_ERROR,
          data: {
            name: 'sample.txt',
          },
        },
      })));

  // Check the item errored.
  const item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.ERROR, item.state);
  assertEquals('Copy filesystem error: File error generic.', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);
}

/**
 * Tests unexpected error.
 */
function testCopyUnexpectedError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: fileOperationUtil.EventRouter.EventType.ERROR,
        status: {
          operationType: 'COPY',
        },
        error: {
          code: 'Unexpected',
          data: {
            name: 'sample.txt',
          },
        },
      })));

  // Check the item errored.
  const item = progressCenter.items['TASK_ID'];
  assertEquals(ProgressItemState.ERROR, item.state);
  assertEquals('Copy unexpected error: Unexpected', item.message);
  assertEquals('copy', item.type);
  assertEquals(true, item.single);
  assertEquals(0, item.progressRateInPercent);
}
