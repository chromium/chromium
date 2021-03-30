// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import { assertArrayEquals,assertEquals, assertTrue} from 'chrome://test/chai_assert.js';

import {FileOperationProgressEvent} from '../../common/js/file_operation_common.m.js';
import {ProgressItemState} from '../../common/js/progress_center_common.m.js';
import {util} from '../../common/js/util.m.js';

import {FileOperationHandler} from './file_operation_handler.m.js';
import {fileOperationUtil} from './file_operation_util.m.js';
import {MockFileOperationManager} from './mock_file_operation_manager.m.js';
import {MockProgressCenter} from './mock_progress_center.m.js';
// clang-format on


/** @type {!MockFileOperationManager} */
let fileOperationManager;

/** @type {!MockProgressCenter} */
let progressCenter;

/** @type {!FileOperationHandler} */
let fileOperationHandler;


/**
 * Mock JS Date.
 *
 * The stop() method should be called to restore original Date.
 */
class MockDate {
  constructor() {
    this.originalNow = Date.now;
    Date.tick_ = 0;
    Date.now = this.now;
  }

  /**
   * Increases current timestamp.
   *
   * @param {number} msec Milliseconds to add to the current timestamp.
   */
  tick(msec) {
    Date.tick_ += msec;
  }

  /**
   * @returns {number} Current timestamp of the mock object.
   */
  now() {
    return Date.tick_;
  }

  /**
   * Restore original Date.now method.
   */
  stop() {
    Date.now = this.originalNow;
  }
}

// Set up the test components.
export function setUp() {
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
export function testCopySuccess() {
  // Dispatch copy event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: FileOperationProgressEvent.EventType.BEGIN,
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
        reason: FileOperationProgressEvent.EventType.SUCCESS,
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
export function testCopyCancel() {
  // Dispatch copy event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: FileOperationProgressEvent.EventType.BEGIN,
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
        reason: FileOperationProgressEvent.EventType.CANCELED,
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
export function testCopyTargetExistsError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: FileOperationProgressEvent.EventType.ERROR,
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
export function testCopyFileSystemError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: FileOperationProgressEvent.EventType.ERROR,
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
export function testCopyUnexpectedError() {
  // Dispatch error event.
  fileOperationManager.dispatchEvent(
      /** @type {!Event} */ (Object.assign(new Event('copy-progress'), {
        taskId: 'TASK_ID',
        reason: FileOperationProgressEvent.EventType.ERROR,
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

/**
 * Tests Speedometer moving average calculations.
 */
export function testSpeedometerMovingAverage() {
  const bufferLength = 20;
  const speedometer = new fileOperationUtil.Speedometer(bufferLength);
  const mockDate = new MockDate();

  speedometer.setTotalBytes(2000);
  mockDate.tick(1000);
  speedometer.update(100);
  mockDate.tick(1000);
  speedometer.update(300);

  // Verify calculated instantaneous speeds.
  assertArrayEquals([100, 200], speedometer.getBufferForTesting());

  // Check moving average speed calculation.
  assertEquals(150, speedometer.getCurrentSpeed());

  // Check calculated remaining time.
  assertEquals(12, speedometer.getRemainingTime());

  mockDate.stop();
}

/**
 * Tests Speedometer buffer ring rotate and substitute.
 */
export function testSpeedometerBufferRing() {
  const bufferLength = 20;
  const speedometer = new fileOperationUtil.Speedometer(bufferLength);
  const mockDate = new MockDate();

  for (let i = 0; i < bufferLength; i++) {
    mockDate.tick(1000);
    speedometer.update(i * 100);
  }

  mockDate.tick(1000);
  speedometer.update(2200);

  const buffer = speedometer.getBufferForTesting();

  // Check buffer not expanded more than the specified length.
  assertEquals(bufferLength, buffer.length);

  // Check first element replaced by recent calculated speed.
  assertEquals(300, buffer[0]);

  mockDate.stop();
}
