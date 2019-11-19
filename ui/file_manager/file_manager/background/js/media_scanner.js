// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Recursively scans through a list of given files and directories, and creates
 * a list of media files.
 *
 * @implements {importer.MediaScanner}
 */
importer.DefaultMediaScanner = class {
  /**
   * @param {function(!FileEntry): !Promise<string>} hashGenerator
   * @param {function(!FileEntry, !importer.Destination,
   *                  !importer.ScanMode):
   *     !Promise<!importer.Disposition>} dispositionChecker
   * @param {!importer.DirectoryWatcherFactory} watcherFactory
   */
  constructor(hashGenerator, dispositionChecker, watcherFactory) {
    /**
     * A little factory for DefaultScanResults which allows us to forgo
     * the saving it's dependencies in our fields.
     * @param {importer.ScanMode} mode Mode of the scan to find new files.
     * @return {!importer.DefaultScanResult}
     */
    this.createScanResult_ = mode => {
      return new importer.DefaultScanResult(mode, hashGenerator);
    };

    /** @private {!Array<!importer.ScanObserver>} */
    this.observers_ = [];

    /**
     * @param {!FileEntry} entry
     * @param {!importer.Destination} destination
     * @param {!importer.ScanMode} mode
     * @return {!Promise<!importer.Disposition>}
     */
    this.getDisposition_ = dispositionChecker;

    /**
     * @private {!importer.DirectoryWatcherFactory}
     * @const
     */
    this.watcherFactory_ = watcherFactory;
  }

  /** @override */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /** @override */
  removeObserver(observer) {
    const index = this.observers_.indexOf(observer);
    if (index > -1) {
      this.observers_.splice(index, 1);
    } else {
      console.warn(
          'Ignoring request to remove observer that is not registered.');
    }
  }

  /** @override */
  scanDirectory(directory, mode) {
    const scan = this.createScanResult_(mode);
    console.info(scan.name + ': Scanning directory ' + directory.fullPath);

    const watcher = this.watcherFactory_(
        /** @this {importer.DefaultMediaScanner} */
        () => {
          scan.cancel();
          this.notify_(importer.ScanEvent.INVALIDATED, scan);
        });

    this.crawlDirectory_(directory, watcher)
        .then(this.scanMediaFiles_.bind(this, scan))
        .then(scan.resolve)
        .catch(scan.reject);

    scan.whenFinal().then(() => {
      console.info(
          scan.name + ': Finished directory scan. Details: ' +
          JSON.stringify(scan.getStatistics()));
      this.notify_(importer.ScanEvent.FINALIZED, scan);
    });

    return scan;
  }

  /** @override */
  scanFiles(entries, mode) {
    if (entries.length === 0) {
      throw new Error('Cannot scan empty list.');
    }
    const scan = this.createScanResult_(mode);
    console.info(
        scan.name + ': Scanning fixed set of ' + entries.length + ' entries.');

    const watcher = this.watcherFactory_(() => {
      scan.cancel();
      this.notify_(importer.ScanEvent.INVALIDATED, scan);
    });

    scan.setCandidateCount(entries.length);
    const scanPromises = entries.map(this.onFileEntryFound_.bind(this, scan));

    Promise.all(scanPromises).then(scan.resolve).catch(scan.reject);

    scan.whenFinal().then(() => {
      console.info(
          scan.name + ': Finished file-selection scan. Details: ' +
          JSON.stringify(scan.getStatistics()));
      this.notify_(importer.ScanEvent.FINALIZED, scan);
    });

    return scan;
  }


  /**
   * @param {!importer.DefaultScanResult} scan
   * @param  {!Array<!FileEntry>} entries
   * @return {!Promise} Resolves when scanning is finished normally
   *     or canceled.
   * @private
   */
  scanMediaFiles_(scan, entries) {
    scan.setCandidateCount(entries.length);
    const handleFileEntry = this.onFileEntryFound_.bind(this, scan);

    /**
     * @param {number} begin The beginning offset in the list of entries
     *     to process.
     * @return {!Promise}
     */
    const scanBatch = begin => {
      if (scan.canceled()) {
        console.debug(
            scan.name + ': Skipping remaining ' + (entries.length - begin) +
            ' entries. Scan was canceled.');
        return Promise.resolve();
      }

      // the second arg to slice is an exclusive end index, so we +1 batch size.
      const end = begin + importer.DefaultMediaScanner.SCAN_BATCH_SIZE;
      console.log(scan.name + ': Processing batch ' + begin + '-' + (end - 1));
      const batch = entries.slice(begin, end);

      return Promise.all(batch.map(handleFileEntry)).then(() => {
        if (end < entries.length) {
          return scanBatch(end);
        }
      });
    };

    return scanBatch(0);
  }

  /**
   * Notifies all listeners at some point in the near future.
   *
   * @param {!importer.ScanEvent} event
   * @param {!importer.DefaultScanResult} result
   * @private
   */
  notify_(event, result) {
    this.observers_.forEach(
        /** @param {!importer.ScanObserver} observer */
        observer => {
          observer(event, result);
        });
  }

  /**
   * Finds all files media files beneath directory AND adds directory
   * watchers for each encountered directory.
   *
   * @param {!DirectoryEntry} directory
   * @param {!importer.DirectoryWatcher} watcher
   * @return {!Promise<!Array<!FileEntry>>}
   * @private
   */
  crawlDirectory_(directory, watcher) {
    const mediaFiles = [];

    return fileOperationUtil
        .findEntriesRecursively(
            directory,
            /** @param  {!Entry} entry */
            entry => {
              if (watcher.triggered) {
                return;
              }

              if (entry.isDirectory) {
                // Note, there is no need for us to recurse, the utility
                // function findEntriesRecursively does that. So we
                // just watch the directory for modifications, and that's it.
                watcher.addDirectory(/** @type {!DirectoryEntry} */ (entry));
              } else if (importer.isEligibleType(entry)) {
                mediaFiles.push(/** @type {!FileEntry} */ (entry));
              }
            })
        .then(() => {
          return mediaFiles;
        });
  }

  /**
   * Finds all files beneath directory.
   *
   * @param {!importer.DefaultScanResult} scan
   * @param {!FileEntry} entry
   * @return {!Promise}
   * @private
   */
  onFileEntryFound_(scan, entry) {
    return this
        .getDisposition_(entry, importer.Destination.GOOGLE_DRIVE, scan.mode)
        .then(
            /**
             * @param {!importer.Disposition} disposition The disposition
             *     of the entry. Either some sort of dupe, or an original.
             * @return {!Promise}
             */
            disposition => {
              return disposition === importer.Disposition.ORIGINAL ?
                  this.onUniqueFileFound_(scan, entry) :
                  this.onDuplicateFileFound_(scan, entry, disposition);
            });
  }

  /**
   * Adds a newly discovered file to the given scan result.
   *
   * @param {!importer.DefaultScanResult} scan
   * @param {!FileEntry} entry
   * @return {!Promise}
   * @private
   */
  onUniqueFileFound_(scan, entry) {
    scan.onCandidatesProcessed(1);
    if (!importer.isEligibleType(entry)) {
      this.notify_(importer.ScanEvent.UPDATED, scan);
      return Promise.resolve();
    }

    return scan.addFileEntry(entry).then(
        /**
         * @param {boolean} added
         */
        added => {
          if (added) {
            this.notify_(importer.ScanEvent.UPDATED, scan);
          }
        });
  }

  /**
   * Adds a duplicate file to the given scan result.  This is to track the
   * number of duplicates that are being encountered.
   *
   * @param {!importer.DefaultScanResult} scan
   * @param {!FileEntry} entry
   * @param {!importer.Disposition} disposition
   * @return {!Promise}
   * @private
   */
  onDuplicateFileFound_(scan, entry, disposition) {
    scan.onCandidatesProcessed(1);
    scan.addDuplicateEntry(entry, disposition);
    this.notify_(importer.ScanEvent.UPDATED, scan);
    return Promise.resolve();
  }
};

/** @const {number} */
importer.DefaultMediaScanner.SCAN_BATCH_SIZE = 1;

/**
 * Results of a scan operation. The object is "live" in that data can and
 * will change as the scan operation discovers files.
 *
 * <p>The scan is complete, and the object will become static once the
 * {@code whenFinal} promise resolves.
 *
 * Note that classes implementing this should provide a read-only
 * {@code name} field.
 *
 * @implements {importer.ScanResult}
 */
importer.DefaultScanResult = class {
  /**
   * @param {importer.ScanMode} mode The scan mode applied for finding new
   *     files.
   * @param {function(!FileEntry): !Promise<string>} hashGenerator Hash-code
   *     generator used to dedupe within the scan results itself.
   */
  constructor(mode, hashGenerator) {
    /**
     * The scan mode applied for finding new files.
     * @type {importer.ScanMode}
     */
    this.mode = mode;

    /** @private {number} */
    this.scanId_ = importer.generateId();

    /** @private {function(!FileEntry): !Promise<string>} */
    this.createHashcode_ = hashGenerator;

    /** @private {number} */
    this.candidateCount_ = 0;

    /** @private {number} */
    this.candidatesProcessed_ = 0;

    /**
     * List of file entries found while scanning.
     * @private {!Array<!FileEntry>}
     */
    this.fileEntries_ = [];

    /**
     * List of duplicate file entries found while scanning.
     * This is exclusive of entries already known by
     * import history.
     *
     * @private {!Array<!FileEntry>}
     */
    this.duplicateFileEntries_ = [];

    /**
     * Hashcodes of all files included captured by this result object so-far.
     * Used to dedupe newly discovered files against other files within
     * the ScanResult.
     * @private {!Object<!FileEntry>}
     */
    this.fileHashcodes_ = {};

    /** @private {number} */
    this.totalBytes_ = 0;

    /** @private {!Object<!importer.Disposition, number>} */
    this.duplicateStats_ = {};
    this.duplicateStats_[importer.Disposition.CONTENT_DUPLICATE] = 0;
    this.duplicateStats_[importer.Disposition.HISTORY_DUPLICATE] = 0;
    this.duplicateStats_[importer.Disposition.SCAN_DUPLICATE] = 0;

    /**
     * The point in time when the scan was started.
     * @type {Date}
     */
    this.scanStarted_ = new Date();

    /**
     * The point in time when the last scan activity occurred.
     * @type {Date}
     */
    this.lastScanActivity_ = this.scanStarted_;

    /**
     * @private {boolean}
     */
    this.canceled_ = false;

    /** @private {!importer.Resolver.<!importer.ScanResult>} */
    this.resolver_ = new importer.Resolver();
  }

  /** @return {string} */
  get name() {
    return 'ScanResult(' + this.scanId_ + ')';
  }

  /** @return {function()} */
  get resolve() {
    return this.resolver_.resolve.bind(null, this);
  }

  /** @return {function(*=)} */
  get reject() {
    return this.resolver_.reject;
  }

  /** @override */
  isFinal() {
    return this.resolver_.settled;
  }

  /** @override */
  setCandidateCount(count) {
    this.candidateCount_ = count;
  }

  /** @override */
  onCandidatesProcessed(count) {
    this.candidatesProcessed_ = this.candidatesProcessed_ + count;
  }

  /** @override */
  getFileEntries() {
    return this.fileEntries_;
  }

  /** @override */
  getDuplicateFileEntries() {
    return this.duplicateFileEntries_;
  }

  /** @override */
  whenFinal() {
    return this.resolver_.promise;
  }

  /** @override */
  cancel() {
    this.canceled_ = true;
  }

  /** @override */
  canceled() {
    return this.canceled_;
  }

  /**
   * Adds a file to results.
   *
   * @param {!FileEntry} entry
   * @return {!Promise<boolean>} True if the file as added, false if it was
   *     rejected as a dupe.
   */
  addFileEntry(entry) {
    return metadataProxy.getEntryMetadata(entry).then(
        /**
         * @param {!Metadata} metadata
         */
        metadata => {
          console.assert(
              'size' in metadata, 'size attribute missing from metadata.');

          return this.createHashcode_(entry).then(
              /**
               * @param {string} hashcode
               */
              hashcode => {
                this.lastScanActivity_ = new Date();

                if (hashcode in this.fileHashcodes_) {
                  this.addDuplicateEntry(
                      entry, importer.Disposition.SCAN_DUPLICATE);
                  return false;
                }

                entry.size = metadata.size;
                this.totalBytes_ += metadata.size;
                this.fileHashcodes_[hashcode] = entry;
                this.fileEntries_.push(entry);
                return true;
              });
        });
  }

  /**
   * Logs the fact that a duplicate file entry was discovered during the scan.
   * @param {!FileEntry} entry
   * @param {!importer.Disposition} disposition
   */
  addDuplicateEntry(entry, disposition) {
    switch (disposition) {
      case importer.Disposition.SCAN_DUPLICATE:
      case importer.Disposition.CONTENT_DUPLICATE:
        this.duplicateFileEntries_.push(entry);
    }

    if (!(disposition in this.duplicateStats_)) {
      this.duplicateStats_[disposition] = 0;
    }
    this.duplicateStats_[disposition]++;
  }

  /** @override */
  getStatistics() {
    return {
      scanDuration:
          this.lastScanActivity_.getTime() - this.scanStarted_.getTime(),
      newFileCount: this.fileEntries_.length,
      duplicates: this.duplicateStats_,
      sizeBytes: this.totalBytes_,
      candidates: {
        total: this.candidateCount_,
        processed: this.candidatesProcessed_,
      },
      progress: this.calculateProgress_()
    };
  }

  /**
   * @return {number} Progress as an integer from 0-100.
   * @private
   */
  calculateProgress_() {
    let progress = (this.candidateCount_ > 0) ?
        Math.floor(this.candidatesProcessed_ / this.candidateCount_ * 100) :
        0;

    // In case candate count was off, or any other mischief has happened,
    // we want to ensure progress never exceeds 100.
    if (progress > 100) {
      console.warn('Progress exceeded 100.');
      progress = 100;
    }

    return progress;
  }
};

/**
 * Watcher for directories.
 * @interface
 */
importer.DirectoryWatcher = class {
  constructor() {
    /** @type {boolean} */
    this.triggered = false;
  }

  /**
   * Registers new directory to be watched.
   * @param {!DirectoryEntry} entry
   */
  addDirectory(entry) {}
};

/**
 * @typedef {function()}
 */
importer.DirectoryWatcherFactoryCallback;

/**
 * @typedef {function(importer.DirectoryWatcherFactoryCallback):
 *     !importer.DirectoryWatcher}
 */
importer.DirectoryWatcherFactory;

/**
 * Watcher for directories.
 * @implements {importer.DirectoryWatcher}
 */
importer.DefaultDirectoryWatcher = class {
  /**
   * @param {function()} callback Callback to be invoked when one of watched
   *     directories is changed.
   */
  constructor(callback) {
    this.callback_ = callback;
    this.watchedDirectories_ = {};
    this.triggered = false;
    this.listener_ = null;
  }

  /**
   * Creates new directory watcher.
   * @param {function()} callback Callback to be invoked when one of watched
   *     directories is changed.
   * @return {!importer.DirectoryWatcher}
   */
  static create(callback) {
    return new importer.DefaultDirectoryWatcher(callback);
  }

  /**
   * Registers new directory to be watched.
   * @param {!DirectoryEntry} entry
   */
  addDirectory(entry) {
    if (!this.listener_) {
      this.listener_ = this.onWatchedDirectoryModified_.bind(this);
      chrome.fileManagerPrivate.onDirectoryChanged.addListener(
          assert(this.listener_));
    }
    this.watchedDirectories_[entry.toURL()] = true;
    chrome.fileManagerPrivate.addFileWatch(entry, () => {});
  }

  /**
   * @param {chrome.fileManagerPrivate.FileWatchEvent} event
   * @private
   */
  onWatchedDirectoryModified_(event) {
    if (!this.watchedDirectories_[event.entry.toURL()]) {
      return;
    }
    this.triggered = true;
    for (const url in this.watchedDirectories_) {
      window.webkitResolveLocalFileSystemURL(url, entry => {
        if (chrome.runtime.lastError) {
          console.error(chrome.runtime.lastError.name);
          return;
        }
        chrome.fileManagerPrivate.removeFileWatch(entry, () => {});
      });
    }
    chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
        assert(this.listener_));
    this.callback_();
  }
};
