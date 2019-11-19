// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Interface providing access to information about active import processes.
 *
 * @interface
 */
importer.ImportRunner = class {
  /**
   * Imports all media identified by a scanResult.
   *
   * @param {!importer.ScanResult} scanResult
   * @param {!importer.Destination} destination
   * @param {!Promise<!DirectoryEntry>} directoryPromise
   *
   * @return {!importer.MediaImportHandler.ImportTask} The media import task.
   */
  importFromScanResult(scanResult, destination, directoryPromise) {}
};
