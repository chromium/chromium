// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var importer = importer || {};

/**
 * Class representing the results of a scan operation.
 *
 * @interface
 */
importer.MediaScanner = class {
  /**
   * Initiates scanning.
   *
   * @param {!DirectoryEntry} directory
   * @param {!importer.ScanMode} mode
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job both while in-progress and when completed.
   */
  scanDirectory(directory, mode) {}

  /**
   * Initiates scanning.
   *
   * @param {!Array<!FileEntry>} entries Must be non-empty, and all entries
   *     must be of a supported media type. Individually supplied files
   *     are not subject to deduplication.
   * @param {!importer.ScanMode} mode The method to detect new files.
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job for the explicitly supplied entries.
   */
  scanFiles(entries, mode) {}

  /**
   * Adds an observer, which will be notified on scan events.
   *
   * @param {!importer.ScanObserver} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ScanObserver} observer
   */
  removeObserver(observer) {}
};

/**
 * @typedef {function(!importer.ScanEvent, importer.ScanResult)}
 */
importer.ScanObserver;

/**
 * Class representing the results of a scan operation.
 *
 * @interface
 */
importer.ScanResult = class {
  /**
   * @return {boolean} true if scanning is complete.
   */
  isFinal() {}

  /**
   * Notifies the scan to stop working. Some in progress work
   * may continue, but no new work will be undertaken.
   */
  cancel() {}

  /**
   * @return {boolean} True if the scan has been canceled. Some
   * work started prior to cancellation may still be ongoing.
   */
  canceled() {}

  /**
   * @param {number} count Sets the total number of candidate entries
   *     that were checked while scanning. Used when determining
   *     total progress.
   */
  setCandidateCount(count) {}

  /**
   * Event method called when a candidate has been processed.
   * @param {number} count
   */
  onCandidatesProcessed(count) {}

  /**
   * Returns all files entries discovered so far. The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * @return {!Array<!FileEntry>}
   */
  getFileEntries() {}

  /**
   * Returns all files entry duplicates discovered so far.
   * The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * Duplicates are files that were found during scanning,
   * where not found in import history, and were matched to
   * an existing entry either in the import destination, or
   * to another entry within the scan itself.
   *
   * @return {!Array<!FileEntry>}
   */
  getDuplicateFileEntries() {}

  /**
   * Returns a promise that fires when scanning is finished
   * normally or has been canceled.
   *
   * @return {!Promise<!importer.ScanResult>}
   */
  whenFinal() {}

  /**
   * @return {!importer.ScanResult.Statistics}
   */
  getStatistics() {}
};

/**
 * @typedef {{
 *   scanDuration: number,
 *   newFileCount: number,
 *   duplicates: !Object<!importer.Disposition, number>,
 *   sizeBytes: number,
 *   candidates: {
 *     total: number,
 *     processed: number
 *   },
 *   progress: number
 * }}
 */
importer.ScanResult.Statistics;
