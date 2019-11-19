// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @implements {importer.MediaScanner}
 */
class TestMediaScanner {
  constructor() {
    /** @private {!Array<!importer.ScanResult>} */
    this.scans_ = [];

    /**
     * List of file entries found while scanning.
     * @type {!Array<!FileEntry>}
     */
    this.fileEntries = [];

    /**
     * List of duplicate file entries found while scanning.
     * @type {!Array<!FileEntry>}
     */
    this.duplicateFileEntries = [];

    /**
     * List of scan observers.
     * @private {!Array<!importer.ScanObserver>}
     */
    this.observers = [];

    /** @private {number} */
    this.totalBytes = 100;

    /** @private {number} */
    this.scanDuration = 100;
  }

  /** @override */
  scanDirectory(directory, mode) {
    const scan = new TestScanResult(this.fileEntries);
    scan.totalBytes = this.totalBytes;
    scan.scanDuration = this.scanDuration;
    this.scans_.push(scan);
    return scan;
  }

  /** @override */
  scanFiles(entries, mode) {
    const scan = new TestScanResult(this.fileEntries);
    scan.totalBytes = this.totalBytes;
    scan.scanDuration = this.scanDuration;
    this.scans_.push(scan);
    return scan;
  }

  /** @override */
  addObserver(observer) {
    this.observers.push(observer);
  }

  /** @override */
  removeObserver(observer) {
    const index = this.observers.indexOf(observer);
    if (index !== -1) {
      this.observers.splice(index, 1);
    } else {
      console.warn('Ignoring request to remove unregistered observer');
    }
  }

  /**
   * Finalizes all previously started scans.
   */
  finalizeScans() {
    this.scans_.forEach(this.finalize.bind(this));
  }

  /**
   * Notifies observers that the most recently started scan has been updated.
   */
  update() {
    assertTrue(this.scans_.length > 0);
    const scan = this.scans_[this.scans_.length - 1];
    this.observers.forEach(observer => {
      observer(importer.ScanEvent.UPDATED, scan);
    });
  }

  /**
   * Notifies observers that a scan has finished.
   * @param {!importer.ScanResult} scan
   */
  finalize(scan) {
    // Note the |scan| has {!TestScanResult} type in test, and needs a
    // finalize() call before being notified to scan observers.
    /** @type {!TestScanResult} */ (scan).finalize();

    this.observers.forEach(observer => {
      observer(importer.ScanEvent.FINALIZED, scan);
    });
  }

  /**
   * @param {number} expected
   */
  assertScanCount(expected) {
    assertEquals(expected, this.scans_.length);
  }

  /**
   * Asserts that the last scan was canceled. Fails if no
   *     scan exists.
   */
  assertLastScanCanceled() {
    assertTrue(this.scans_.length > 0);
    assertTrue(this.scans_[this.scans_.length - 1].canceled());
  }
}

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @implements {importer.ScanResult}
 */
class TestScanResult {
  /**
   * @param {!Array<!FileEntry>} fileEntries
   */
  constructor(fileEntries) {
    /** @private {number} */
    this.scanId_ = ++TestScanResult.lastId_;

    /**
     * List of file entries found while scanning.
     * @type {!Array<!FileEntry>}
     */
    this.fileEntries = fileEntries.slice();

    /**
     * List of duplicate file entries found while scanning.
     * @type {!Array<!FileEntry>}
     */
    this.duplicateFileEntries = [];

    /** @type {number} */
    this.totalBytes = 100;

    /** @type {number} */
    this.scanDuration = 100;

    /** @type {function(*)} */
    this.resolveResult_;

    /** @type {function()} */
    this.rejectResult_;

    /** @type {boolean} */
    this.settled_ = false;

    /** @private {boolean} */
    this.canceled_ = false;

    /** @type {!Promise<!importer.ScanResult>} */
    this.whenFinal_ = new Promise((resolve, reject) => {
      this.resolveResult_ = result => {
        this.settled_ = true;
        resolve(result);
      };
      this.rejectResult_ = () => {
        this.settled_ = true;
        reject();
      };
    });
  }

  /** @return {string} */
  get name() {
    return 'TestScanResult(' + this.scanId_ + ')';
  }

  /** @override */
  isFinal() {
    return this.settled_;
  }

  /** @override */
  cancel() {
    this.canceled_ = true;
  }

  /** @override */
  canceled() {
    return this.canceled_;
  }

  /** @override */
  setCandidateCount() {
    console.warn('setCandidateCount: not implemented');
  }

  /** @override */
  onCandidatesProcessed() {
    console.warn('onCandidatesProcessed: not implemented');
  }

  /** @override */
  getFileEntries() {
    return this.fileEntries;
  }

  /** @override */
  getDuplicateFileEntries() {
    return this.duplicateFileEntries;
  }

  /** @override */
  whenFinal() {
    return this.whenFinal_;
  }

  /** @override */
  getStatistics() {
    const duplicates = {};
    duplicates[importer.Disposition.CONTENT_DUPLICATE] = 0;
    duplicates[importer.Disposition.HISTORY_DUPLICATE] = 0;
    duplicates[importer.Disposition.SCAN_DUPLICATE] = 0;
    return /** @type {importer.ScanResult.Statistics} */ ({
      scanDuration: this.scanDuration,
      newFileCount: this.fileEntries.length,
      duplicates: duplicates,
      sizeBytes: this.totalBytes
    });
  }

  finalize() {
    return this.resolveResult_(this);
  }
}

/** @private {number} */
TestScanResult.lastId_ = 0;

/**
 * @implements {importer.DirectoryWatcher}
 */
class TestDirectoryWatcher {
  constructor(callback) {
    /**
     * @public {function()}
     * @const
     */
    this.callback = callback;

    /**
     * @public {boolean}
     */
    this.triggered = false;
  }

  /**
   * @override
   */
  addDirectory() {}
}
