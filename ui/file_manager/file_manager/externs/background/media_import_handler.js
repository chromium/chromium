// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Externs definition for  MediaImportHandler to allow for Closure compilation
 * of its media import unittest and caller sites.
 */

import {importer} from '../../common/js/importer_common.js';

import {DriveSyncHandler} from './drive_sync_handler.js';
import {duplicateFinderInterfaces} from './duplicate_finder.js';
import {importerHistoryInterfaces} from './import_history.js';
import {mediaScannerInterfaces} from './media_scanner.js';
import {ProgressCenter} from './progress_center.js';
import {taskQueueInterfaces} from './task_queue.js';

export const mediaImportInterfaces = {};

/**
 * Interface providing access to information about active import processes.
 *
 * @interface
 */
mediaImportInterfaces.ImportRunner = class {
  /**
   * Imports all media identified by a scanResult.
   *
   * @param {!mediaScannerInterfaces.ScanResult} scanResult
   * @param {!importer.Destination} destination
   * @param {!Promise<!DirectoryEntry>} directoryPromise
   *
   * @return {!mediaImportInterfaces.MediaImportHandler.ImportTask} The media
   *     import task.
   */
  importFromScanResult(scanResult, destination, directoryPromise) {}
};

/**
 * Define MediaImportHandler constructor: handler for importing media from
 * removable devices into the user's Drive.
 *
 * @interface
 */
mediaImportInterfaces.MediaImportHandler =
    class extends mediaImportInterfaces.ImportRunner {
  /**
   * @param {!ProgressCenter} progressCenter
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   * @param {!duplicateFinderInterfaces.DispositionChecker.CheckerFunction}
   *     dispositionChecker
   * @param {!DriveSyncHandler} driveSyncHandler
   */
  constructor(
      progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {
    super();
  }
};

/**
 * Define MediaImportHandler.ImportTask.
 *
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @extends {taskQueueInterfaces.BaseTask}
 * @interface
 */
mediaImportInterfaces.MediaImportHandler.ImportTask = class {
  /**
   * @param {string} taskId
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   * @param {!mediaScannerInterfaces.ScanResult} scanResult
   * @param {!Promise<!DirectoryEntry>} directoryPromise
   * @param {!importer.Destination} destination The logical destination.
   * @param {!duplicateFinderInterfaces.DispositionChecker.CheckerFunction}
   *     dispositionChecker
   */
  constructor(
      taskId, historyLoader, scanResult, directoryPromise, destination,
      dispositionChecker) {}

  /**
   * @return {!Promise} Resolves when task is complete, or cancelled, rejects
   *     on error.
   */
  get whenFinished() {}

  /**
   * Requests cancellation of this task: an update will be sent to all observers
   * of this task when the task is actually cancelled.
   */
  requestCancel() {}
};

/**
 * Auxiliary info for ENTRY_CHANGED notifications.
 * @typedef {{
 *   sourceUrl: string,
 *   destination: !Entry
 * }}
 */
mediaImportInterfaces.MediaImportHandler.ImportTask.EntryChangedInfo;
