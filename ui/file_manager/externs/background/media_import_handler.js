// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/*
 * Externs definition for  MediaImportHandler to allow for Closure compilation
 * of its media import unittest and caller sites.
 */

/**
 * Define MediaImportHandler constructor: handler for importing media from
 * removable devices into the user's Drive.
 *
 * @interface
 */
importer.MediaImportHandler = class extends importer.ImportRunner {
  /**
   * @param {!ProgressCenter} progressCenter
   * @param {!importer.HistoryLoader} historyLoader
   * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
   * @param {!DriveSyncHandler} driveSyncHandler
   */
  constructor(
      progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {}
};

/**
 * Define MediaImportHandler.ImportTask.
 *
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @extends {importer.TaskQueue.BaseTask}
 * @interface
 */
importer.MediaImportHandler.ImportTask = class {
  /**
   * @param {string} taskId
   * @param {!importer.HistoryLoader} historyLoader
   * @param {!importer.ScanResult} scanResult
   * @param {!Promise<!DirectoryEntry>} directoryPromise
   * @param {!importer.Destination} destination The logical destination.
   * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
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
importer.MediaImportHandler.ImportTask.EntryChangedInfo;
