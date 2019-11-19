// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * A persistent data store for Cloud Import history information.
 *
 * @interface
 */
importer.ImportHistory = class {
  /**
   * @return {!Promise<!importer.ImportHistory>} Resolves when history
   *     has been fully loaded.
   */
  whenReady() {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously copied to the device.
   */
  wasCopied(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously imported to the specified destination.
   */
  wasImported(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {string} destinationUrl
   */
  markCopied(entry, destination, destinationUrl) {}

  /**
   * List urls of all files that are marked as copied, but not marked as synced.
   * @param {!importer.Destination} destination
   * @return {!Promise<!Array<string>>}
   */
  listUnimportedUrls(destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImported(entry, destination) {}

  /**
   * @param {string} destinationUrl
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImportedByUrl(destinationUrl) {}

  /**
   * Adds an observer, which will be notified when cloud import history changes.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  removeObserver(observer) {}
};

/**
 * @enum {string} Import history changed event |state| values.
 */
importer.ImportHistoryState = {
  'COPIED': 'copied',
  'IMPORTED': 'imported'
};

/**
 * Import history changed event (sent to ImportHistory.Observer's).
 *
 * @typedef {{
 *   state: !importer.ImportHistoryState,
 *   entry: !FileEntry,
 *   destination: !importer.Destination,
 *   destinationUrl: (string|undefined)
 * }}
 */
importer.ImportHistory.ChangedEvent;

/** @typedef {function(!importer.ImportHistory.ChangedEvent)} */
importer.ImportHistory.Observer;

/**
 * Provider of lazy loaded importer.ImportHistory. This is the main
 * access point for a fully prepared {@code importer.ImportHistory} object.
 *
 * @interface
 */
importer.HistoryLoader = class {
  /**
   * Instantiates an {@code importer.ImportHistory} object and manages any
   * necessary ongoing maintenance of the object with respect to
   * its external dependencies.
   *
   * @see importer.SynchronizedHistoryLoader for an example.
   *
   * @return {!Promise<!importer.ImportHistory>} Resolves when history instance
   *     is ready.
   */
  getHistory() {}

  /**
   * Adds a listener to be notified when history is fully loaded for the first
   * time. If history is already loaded...will be called immediately.
   *
   * @param {function(!importer.ImportHistory)} listener
   */
  addHistoryLoadedListener(listener) {}
};
