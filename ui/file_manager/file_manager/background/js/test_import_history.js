// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * importer.ImportHistory and importer.HistoryLoader test double.
 * ONE STOP SHOPPING!
 *
 * @implements {importer.HistoryLoader}
 * @implements {importer.ImportHistory}
 */
importer.TestImportHistory = class {
  constructor() {
    /** @type {!Object<!Object<!importer.Destination, string>>} */
    this.copiedPaths = {};

    /** @type {!Object<Array<string>>} */
    this.importedPaths = {};

    /**
     * If null, history has been loaded and listeners notified.
     *
     * @private {Array<!function(!importer.ImportHistory)>}
     */
    this.loadListeners_ = [];
  }

  /** @override */
  getHistory() {
    Promise.resolve().then(() => {
      if (this.loadListeners_) {
        this.loadListeners_.forEach((listener) => listener(this));
        // Null out listeners...this is our signal that history has
        // been loaded ... resulting in all future listener added
        // being notified immediately
        this.loadListeners_ = null;
      }
    });

    return Promise.resolve(this);
  }

  /** @override */
  addHistoryLoadedListener(listener) {
    assertTrue(typeof listener === 'function');
    // Notify listener immediately if history is already loaded.
    if (this.loadListeners_ === null) {
      setTimeout(listener, 0, this);
    } else {
      this.loadListeners_.push(listener);
    }
  }

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   */
  assertCopied(entry, destination) {
    assertTrue(this.wasCopied_(entry, destination));
  }

  /**
   * Fully synchronous version of wasCopied.
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {boolean}
   */
  wasCopied_(entry, destination) {
    const path = entry.fullPath;
    return path in this.copiedPaths &&
        this.copiedPaths[path].indexOf(destination) > -1;
  }

  /** @override */
  wasCopied(entry, destination) {
    const path = entry.fullPath;
    return Promise.resolve(this.wasCopied_(entry, destination));
  }

  /** @override */
  markCopied(entry, destination, destinationUrl) {
    const path = entry.fullPath;
    if (path in this.copiedPaths) {
      this.copiedPaths[path].push(destination);
    } else {
      this.copiedPaths[path] = [destination];
    }
    return Promise.resolve();
  }

  /** @override */
  listUnimportedUrls(destination) {
    return Promise.resolve([]);
  }

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   */
  assertImported(entry, destination) {
    assertTrue(this.wasImported_(entry, destination));
  }

  /**
   * Fully synchronous version of wasImported.
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {boolean}
   */
  wasImported_(entry, destination) {
    const path = entry.fullPath;
    return path in this.importedPaths &&
        this.importedPaths[path].indexOf(destination) > -1;
  }

  /** @override */
  wasImported(entry, destination) {
    const path = entry.fullPath;
    return Promise.resolve(this.wasImported_(entry, destination));
  }

  /** @override */
  markImported(entry, destination) {
    const path = entry.fullPath;
    if (path in this.importedPaths) {
      this.importedPaths[path].push(destination);
    } else {
      this.importedPaths[path] = [destination];
    }
    return Promise.resolve();
  }

  /** @override */
  whenReady() {}

  /** @override */
  markImportedByUrl() {}

  /** @override */
  addObserver() {}

  /** @override */
  removeObserver() {}
};
