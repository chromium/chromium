// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Scanner of the entries.
 */
class ContentScanner {
  constructor() {
    this.cancelled_ = false;
  }

  /**
   * Starts to scan the entries. For example, starts to read the entries in a
   * directory, or starts to search with some query on a file system.
   * Derived classes must override this method.
   *
   * @param {function(Array<Entry>)} entriesCallback Called when some chunk of
   *     entries are read. This can be called a couple of times until the
   *     completion.
   * @param {function()} successCallback Called when the scan is completed
   *     successfully.
   * @param {function(DOMError)} errorCallback Called an error occurs.
   */
  scan(entriesCallback, successCallback, errorCallback) {}

  /**
   * Request cancelling of the running scan. When the cancelling is done,
   * an error will be reported from errorCallback passed to scan().
   */
  cancel() {
    this.cancelled_ = true;
  }
}

/**
 * Scanner of the entries in a directory.
 */
class DirectoryContentScanner extends ContentScanner {
  /**
   * @param {DirectoryEntry|FilesAppDirEntry} entry The directory to be read.
   */
  constructor(entry) {
    super();
    this.entry_ = entry;
  }

  /**
   * Starts to read the entries in the directory.
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    if (!this.entry_ || !this.entry_.createReader) {
      // If entry is not specified or if entry doesn't implement createReader,
      // we cannot read it.
      errorCallback(
          util.createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
      return;
    }

    metrics.startInterval('DirectoryScan');
    const reader = this.entry_.createReader();
    const readEntries = () => {
      reader.readEntries(entries => {
        if (this.cancelled_) {
          errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
          return;
        }

        if (entries.length === 0) {
          // All entries are read.
          metrics.recordInterval('DirectoryScan');
          successCallback();
          return;
        }

        entriesCallback(entries);
        readEntries();
      }, errorCallback);
    };
    readEntries();
  }
}

/**
 * Scanner of the entries for the search results on Drive File System.
 */
class DriveSearchContentScanner extends ContentScanner {
  /** @param {string} query The query string. */
  constructor(query) {
    super();
    this.query_ = query;
  }

  /**
   * Starts to search on Drive File System.
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    // Let's give another search a chance to cancel us before we begin.
    setTimeout(() => {
      // Check cancelled state before read the entries.
      if (this.cancelled_) {
        errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
        return;
      }
      chrome.fileManagerPrivate.searchDrive(
          {query: this.query_, nextFeed: ''}, (entries, nextFeed) => {
            if (chrome.runtime.lastError) {
              console.error(chrome.runtime.lastError.message);
            }

            if (this.cancelled_) {
              errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
              return;
            }

            // TODO(tbarzic): Improve error handling.
            if (!entries) {
              console.error('Drive search encountered an error.');
              errorCallback(
                  util.createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
              return;
            }

            if (entries.length >= DriveSearchContentScanner.MAX_RESULTS_) {
              // More results were received than expected, so trim.
              entries =
                  entries.slice(0, DriveSearchContentScanner.MAX_RESULTS_);
            }

            if (entries.length > 0) {
              entriesCallback(entries);
            }

            successCallback();
          });
    }, DriveSearchContentScanner.SCAN_DELAY_);
  }
}

/**
 * Delay in milliseconds to be used for drive search scan, in order to reduce
 * the number of server requests while user is typing the query.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.SCAN_DELAY_ = 200;

/**
 * Maximum number of results which is shown on the search.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.MAX_RESULTS_ = 100;

/**
 * Scanner of the entries of the file name search on the directory tree, whose
 * root is entry.
 */
class LocalSearchContentScanner extends ContentScanner {
  /**
   * @param {DirectoryEntry} entry The root of the search target directory tree.
   * @param {string} query The query of the search.
   */
  constructor(entry, query) {
    super();
    this.entry_ = entry;
    this.query_ = query.toLowerCase();
  }

  /**
   * Starts the file name search.
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    util.readEntriesRecursively(assert(this.entry_), (entries) => {
      const matchEntries = entries.filter(
          entry => entry.name.toLowerCase().indexOf(this.query_) >= 0);
      if (matchEntries.length > 0) {
        entriesCallback(matchEntries);
      }
    }, successCallback, errorCallback, () => this.cancelled_);
  }
}

/**
 * Scanner of the entries for the metadata search on Drive File System.
 */
class DriveMetadataSearchContentScanner extends ContentScanner {
  /**
   * @param {!DriveMetadataSearchContentScanner.SearchType} searchType The
   *     option of the search.
   */
  constructor(searchType) {
    super();
    this.searchType_ = searchType;
  }

  /**
   * Starts to metadata-search on Drive File System.
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    chrome.fileManagerPrivate.searchDriveMetadata(
        {query: '', types: this.searchType_, maxResults: 100}, results => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
          }
          if (this.cancelled_) {
            errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
            return;
          }

          if (!results) {
            console.error('Drive search encountered an error.');
            errorCallback(
                util.createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
            return;
          }

          const entries = results.map(result => {
            return result.entry;
          });
          if (entries.length > 0) {
            entriesCallback(entries);
          }
          successCallback();
        });
  }
}

/**
 * The search types on the Drive File System.
 * @enum {!chrome.fileManagerPrivate.SearchType}
 */
DriveMetadataSearchContentScanner.SearchType = {
  SEARCH_ALL: chrome.fileManagerPrivate.SearchType.ALL,
  SEARCH_SHARED_WITH_ME: chrome.fileManagerPrivate.SearchType.SHARED_WITH_ME,
  SEARCH_RECENT_FILES: chrome.fileManagerPrivate.SearchType.EXCLUDE_DIRECTORIES,
  SEARCH_OFFLINE: chrome.fileManagerPrivate.SearchType.OFFLINE,
};
Object.freeze(DriveMetadataSearchContentScanner.SearchType);

class RecentContentScanner extends ContentScanner {
  /**
   * @param {string} query Search query.
   * @param {string=} opt_sourceRestriction
   */
  constructor(query, opt_sourceRestriction) {
    super();

    /**
     * @private {string}
     */
    this.query_ = query.toLowerCase();

    /**
     * @private {string}
     */
    this.sourceRestriction_ = opt_sourceRestriction ||
        chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE;
  }

  /**
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    chrome.fileManagerPrivate.getRecentFiles(
        this.sourceRestriction_, entries => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            errorCallback(
                util.createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
            return;
          }
          if (entries.length > 0) {
            entriesCallback(entries.filter(
                entry => entry.name.toLowerCase().indexOf(this.query_) >= 0));
          }
          successCallback();
        });
  }
}

/**
 * Scanner of media-view volumes.
 */
class MediaViewContentScanner extends ContentScanner {
  /**
   * @param {!DirectoryEntry} rootEntry The root entry of the media-view volume.
   */
  constructor(rootEntry) {
    super();
    this.rootEntry_ = rootEntry;
  }

  /**
   * This scanner provides flattened view of media providers.
   *
   * In FileSystem API level, each media-view root directory has directory
   * hierarchy. We need to list files under the root directory to provide
   * flatten view. A file will not be shown in multiple directories in
   * media-view hierarchy since no folders will be added in media documents
   * provider. We can list all files without duplication by just retrieveing
   * files in directories recursively.
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    // To provide flatten view of files, this media-view scanner retrieves files
    // in directories inside the media's root entry recursively.
    util.readEntriesRecursively(
        this.rootEntry_,
        entries => entriesCallback(entries.filter(entry => !entry.isDirectory)),
        successCallback, errorCallback, () => false);
  }
}

/**
 * Shows an empty list and spinner whilst starting and mounting the
 * crostini container.
 *
 * This function is only called once to start and mount the crostini
 * container.  When FilesApp starts, the related fake root entry for
 * crostini is shown which uses this CrostiniMounter as its ContentScanner.
 *
 * When the sshfs mount completes, it will show up as a disk volume.
 * NavigationListModel.reorderNavigationItems_ will detect that crostini
 * is mounted as a disk volume and hide the fake root item while the
 * disk volume exists.
 */
class CrostiniMounter extends ContentScanner {
  /**
   * @override
   */
  scan(entriesCallback, successCallback, errorCallback) {
    metrics.startInterval('MountCrostiniContainer');
    chrome.fileManagerPrivate.mountCrostini(() => {
      if (chrome.runtime.lastError) {
        console.error(
            'mountCrostini error: ', chrome.runtime.lastError.message);
        errorCallback(util.createDOMError(
            DirectoryModel.CROSTINI_CONNECT_ERR,
            chrome.runtime.lastError.message));
        return;
      }
      metrics.recordInterval('MountCrostiniContainer');
      successCallback();
    });
  }
}

/**
 * This class manages filters and determines a file should be shown or not.
 * When filters are changed, a 'changed' event is fired.
 */
class FileFilter extends cr.EventTarget {
  /** @param {!MetadataModel} metadataModel */
  constructor(metadataModel) {
    super();

    /**
     * @type {Object<Function>}
     * @private
     */
    this.filters_ = {};
    this.setHiddenFilesVisible(false);
    this.setAllAndroidFoldersVisible(false);

    /**
     * @type {!MetadataModel}
     * @const
     * @private
     */
    this.metadataModel_ = metadataModel;

    this.hideAndroidDownload();
  }

  /**
   * @param {string} name Filter identifier.
   * @param {function(Entry)} callback A filter - a function receiving an Entry,
   *     and returning bool.
   */
  addFilter(name, callback) {
    this.filters_[name] = callback;
    cr.dispatchSimpleEvent(this, 'changed');
  }

  /**
   * @param {string} name Filter identifier.
   */
  removeFilter(name) {
    delete this.filters_[name];
    cr.dispatchSimpleEvent(this, 'changed');
  }

  /**
   * Show/Hide hidden files (i.e. files starting with '.').
   * @param {boolean} visible True if hidden files should be visible to the
   *     user.
   */
  setHiddenFilesVisible(visible) {
    if (!visible) {
      this.addFilter('hidden', entry => {
        return entry.name.substr(0, 1) !== '.';
      });
    } else {
      this.removeFilter('hidden');
    }
  }

  /**
   * @return {boolean} True if hidden files are visible to the user now.
   */
  isHiddenFilesVisible() {
    return !('hidden' in this.filters_);
  }

  /**
   * Show/Hide uncommon Android folders which are not whitelisted.
   * @param {boolean} visible True if uncommon folders should be visible to the
   *     user.
   */
  setAllAndroidFoldersVisible(visible) {
    if (!visible) {
      this.addFilter('android_hidden', entry => {
        if (entry.filesystem && entry.filesystem.name !== 'android_files') {
          return true;
        }
        // If |entry| is an Android top-level folder which is not whitelisted or
        // its sub folder, it should be hidden.
        if (entry.fullPath) {
          const components = entry.fullPath.split('/');
          if (components[1] &&
              FileFilter.DEFAULT_ANDROID_FOLDERS.indexOf(components[1]) == -1) {
            return false;
          }
        }
        return true;
      });
    } else {
      this.removeFilter('android_hidden');
    }
  }

  /**
   * @return {boolean} True if uncommon folders is visible to the user now.
   */
  isAllAndroidFoldersVisible() {
    return !('android_hidden' in this.filters_);
  }

  /**
   * Sets up a filter to hide /Download directory in 'Play files' volume.
   *
   * "Play files/Download" is an alias to Chrome OS's Downloads volume. It is
   * convenient in Android file picker, but can be confusing in Chrome OS Files
   * app. This function adds a filter to hide the Android's /Download.
   */
  hideAndroidDownload() {
    this.addFilter('android_download', entry => {
      if (entry.filesystem && entry.filesystem.name === 'android_files' &&
          entry.fullPath === '/Download') {
        return false;
      }
      return true;
    });
  }

  /**
   * @param {Entry} entry File entry.
   * @return {boolean} True if the file should be shown, false otherwise.
   */
  filter(entry) {
    for (const name in this.filters_) {
      if (!this.filters_[name](entry)) {
        return false;
      }
    }
    return true;
  }
}

/**
 * Top-level Android folders which are visible by default.
 * @const {!Array<string>}
 */
FileFilter.DEFAULT_ANDROID_FOLDERS =
    ['Documents', 'Movies', 'Music', 'Pictures'];

/**
 * A context of DirectoryContents.
 * TODO(yoshiki): remove this. crbug.com/224869.
 */
class FileListContext {
  /**
   * @param {FileFilter} fileFilter The file-filter context.
   * @param {!MetadataModel} metadataModel
   * @param {!VolumeManager} volumeManager The volume manager.
   */
  constructor(fileFilter, metadataModel, volumeManager) {
    /**
     * @type {FileListModel}
     */
    this.fileList = new FileListModel(metadataModel);

    /**
     * @public {!MetadataModel}
     * @const
     */
    this.metadataModel = metadataModel;

    /**
     * @type {FileFilter}
     */
    this.fileFilter = fileFilter;

    /**
     * @public {!Array<string>}
     * @const
     */
    this.prefetchPropertyNames = FileListContext.createPrefetchPropertyNames_();

    /** @public {!VolumeManager} */
    this.volumeManager = volumeManager;
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  static createPrefetchPropertyNames_() {
    const set = {};
    for (let i = 0;
         i < constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES.length;
         i++) {
      set[constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES[i]] = true;
    }
    for (let i = 0;
         i < constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES.length;
         i++) {
      set[constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES[i]] = true;
    }
    for (let i = 0;
         i < constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES.length;
         i++) {
      set[constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES[i]] = true;
    }
    return Object.keys(set);
  }
}

/**
 * This class is responsible for scanning directory (or search results),
 * and filling the fileList. Different descendants handle various types of
 * directory contents shown: basic directory, drive search results, local search
 * results.
 * TODO(hidehiko): Remove EventTarget from this.
 */
class DirectoryContents extends cr.EventTarget {
  /**
   *
   * @param {FileListContext} context The file list context.
   * @param {boolean} isSearch True for search directory contents, otherwise
   *     false.
   * @param {DirectoryEntry|FakeEntry|FilesAppDirEntry} directoryEntry The entry
   *     of the current directory.
   * @param {function():ContentScanner} scannerFactory The factory to create
   *     ContentScanner instance.
   */
  constructor(context, isSearch, directoryEntry, scannerFactory) {
    super();

    /** @private {FileListContext} */
    this.context_ = context;

    /** @private {FileListModel} */
    this.fileList_ = context.fileList;
    this.fileList_.InitNewDirContents(context.volumeManager);

    this.isSearch_ = isSearch;
    this.directoryEntry_ = directoryEntry;

    this.scannerFactory_ = scannerFactory;
    this.scanner_ = null;
    this.processNewEntriesQueue_ = new AsyncUtil.Queue();
    this.scanCancelled_ = false;

    /**
     * Metadata snapshot which is used to know which file is actually changed.
     * @type {Object}
     */
    this.metadataSnapshot_ = null;
  }

  /**
   * Create the copy of the object, but without scan started.
   * @return {!DirectoryContents} Object copy.
   */
  clone() {
    return new DirectoryContents(
        this.context_, this.isSearch_, this.directoryEntry_,
        this.scannerFactory_);
  }

  /**
   * Returns the file list length.
   * @return {number}
   */
  getFileListLength() {
    return this.fileList_.length;
  }

  /**
   * Use a given fileList instead of the fileList from the context.
   * @param {!FileListModel} fileList The new file list.
   */
  setFileList(fileList) {
    this.fileList_ = fileList;
  }

  /**
   * Creates snapshot of metadata in the directory.
   * @return {!Object} Metadata snapshot of current directory contents.
   */
  createMetadataSnapshot() {
    const snapshot = {};
    const entries = /** @type {!Array<!Entry>} */ (this.fileList_.slice());
    const metadata =
        this.context_.metadataModel.getCache(entries, ['modificationTime']);
    for (let i = 0; i < entries.length; i++) {
      snapshot[entries[i].toURL()] = metadata[i];
    }
    return snapshot;
  }

  /**
   * Sets metadata snapshot which is used to check changed files.
   * @param {!Object} metadataSnapshot A metadata snapshot.
   */
  setMetadataSnapshot(metadataSnapshot) {
    this.metadataSnapshot_ = metadataSnapshot;
  }

  /**
   * Use the filelist from the context and replace its contents with the entries
   * from the current fileList. If metadata snapshot is set, this method checks
   * actually updated files and dispatch change events by calling updateIndexes.
   */
  replaceContextFileList() {
    if (this.context_.fileList !== this.fileList_) {
      // TODO(yawano): While we should update the list with adding or deleting
      // what actually added and deleted instead of deleting and adding all
      // items, splice of array data model is expensive since it always runs
      // sort and we replace the list in this way to reduce the number of splice
      // calls.
      const spliceArgs = this.fileList_.slice();
      const fileList = this.context_.fileList;
      spliceArgs.unshift(0, fileList.length);
      fileList.splice.apply(fileList, spliceArgs);
      this.fileList_ = fileList;

      // Check updated files and dispatch change events.
      if (this.metadataSnapshot_) {
        const updatedIndexes = [];
        const entries = /** @type {!Array<!Entry>} */ (this.fileList_.slice());
        const newMetadatas =
            this.context_.metadataModel.getCache(entries, ['modificationTime']);

        for (let i = 0; i < entries.length; i++) {
          const url = entries[i].toURL();
          const newMetadata = newMetadatas[i];
          // If the Files app fails to obtain both old and new modificationTime,
          // regard the entry as not updated.
          if ((this.metadataSnapshot_[url] &&
               this.metadataSnapshot_[url].modificationTime &&
               this.metadataSnapshot_[url].modificationTime.getTime()) !==
              (newMetadata.modificationTime &&
               newMetadata.modificationTime.getTime())) {
            updatedIndexes.push(i);
          }
        }

        if (updatedIndexes.length > 0) {
          this.fileList_.updateIndexes(updatedIndexes);
        }
      }
    }
  }

  /**
   * @return {boolean} If the scan is active.
   */
  isScanning() {
    return this.scanner_ || this.processNewEntriesQueue_.isRunning();
  }

  /**
   * @return {boolean} True if search results (drive or local).
   */
  isSearch() {
    return this.isSearch_;
  }

  /**
   * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} A DirectoryEntry for
   *     current directory. In case of search -- the top directory from which
   *     search is run.
   */
  getDirectoryEntry() {
    return this.directoryEntry_;
  }

  /**
   * Start directory scan/search operation. Either 'scan-completed' or
   * 'scan-failed' event will be fired upon completion.
   *
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   */
  scan(refresh) {
    /**
     * Invoked when the scanning is completed successfully.
     * @this {DirectoryContents}
     */
    function completionCallback() {
      this.onScanFinished_();
      this.onScanCompleted_();
    }

    /**
     * Invoked when the scanning is finished but is not completed due to error.
     * @param {DOMError} error error.
     * @this {DirectoryContents}
     */
    function errorCallback(error) {
      this.onScanFinished_();
      this.onScanError_(error);
    }

    // TODO(hidehiko,mtomasz): this scan method must be called at most once.
    // Remove such a limitation.
    this.scanner_ = this.scannerFactory_();
    this.scanner_.scan(
        this.onNewEntries_.bind(this, refresh), completionCallback.bind(this),
        errorCallback.bind(this));
  }

  /**
   * Adds/removes/updates items of file list.
   * @param {Array<Entry>} updatedEntries Entries of updated/added files.
   * @param {Array<string>} removedUrls URLs of removed files.
   */
  update(updatedEntries, removedUrls) {
    const removedMap = {};
    for (let i = 0; i < removedUrls.length; i++) {
      removedMap[removedUrls[i]] = true;
    }

    const updatedMap = {};
    for (let i = 0; i < updatedEntries.length; i++) {
      updatedMap[updatedEntries[i].toURL()] = updatedEntries[i];
    }

    const updatedList = [];
    const updatedIndexes = [];
    for (let i = 0; i < this.fileList_.length; i++) {
      const url = this.fileList_.item(i).toURL();

      if (url in removedMap) {
        // Find the maximum range in which all items need to be removed.
        const begin = i;
        let end = i + 1;
        while (end < this.fileList_.length &&
               this.fileList_.item(end).toURL() in removedMap) {
          end++;
        }
        // Remove the range [begin, end) at once to avoid multiple sorting.
        this.fileList_.splice(begin, end - begin);
        i--;
        continue;
      }

      if (url in updatedMap) {
        updatedList.push(updatedMap[url]);
        updatedIndexes.push(i);
        delete updatedMap[url];
      }
    }

    if (updatedIndexes.length > 0) {
      this.fileList_.updateIndexes(updatedIndexes);
    }

    const addedList = [];
    for (let url in updatedMap) {
      addedList.push(updatedMap[url]);
    }

    if (removedUrls.length > 0) {
      this.context_.metadataModel.notifyEntriesRemoved(removedUrls);
    }

    this.prefetchMetadata(updatedList, true, () => {
      this.onNewEntries_(true, addedList);
      this.onScanFinished_();
      this.onScanCompleted_();
    });
  }

  /**
   * Cancels the running scan.
   */
  cancelScan() {
    if (this.scanCancelled_) {
      return;
    }
    this.scanCancelled_ = true;
    if (this.scanner_) {
      this.scanner_.cancel();
    }

    this.onScanFinished_();

    this.processNewEntriesQueue_.cancel();
    cr.dispatchSimpleEvent(this, 'scan-cancelled');
  }

  /**
   * Called when the scanning by scanner_ is done, even when the scanning is
   * succeeded or failed. This is called before completion (or error) callback.
   *
   * @private
   */
  onScanFinished_() {
    this.scanner_ = null;
  }

  /**
   * Called when the scanning by scanner_ is succeeded.
   * @private
   */
  onScanCompleted_() {
    if (this.scanCancelled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();

      cr.dispatchSimpleEvent(this, 'scan-completed');
    });
  }

  /**
   * Called in case scan has failed. Should send the event.
   * @param {DOMError} error error.
   * @private
   */
  onScanError_(error) {
    if (this.scanCancelled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();
      const event = new Event('scan-failed');
      event.error = error;
      this.dispatchEvent(event);
    });
  }

  /**
   * Called when some chunk of entries are read by scanner.
   *
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {Array<Entry>} entries The list of the scanned entries.
   * @private
   */
  onNewEntries_(refresh, entries) {
    if (this.scanCancelled_) {
      return;
    }

    // Caching URL to reduce a number of calls of toURL in sort.
    // This is a temporary solution. We need to fix a root cause of slow toURL.
    // See crbug.com/370908 for detail.
    entries.forEach(entry => {
      entry['cachedUrl'] = entry.toURL();
    });

    if (entries.length === 0) {
      return;
    }

    // Enlarge the cache size into the new filelist size.
    const newListSize = this.fileList_.length + entries.length;

    this.processNewEntriesQueue_.run(callbackOuter => {
      const finish = () => {
        if (!this.scanCancelled_) {
          let entriesFiltered = [].filter.call(
              entries,
              this.context_.fileFilter.filter.bind(this.context_.fileFilter));

          // Just before inserting entries into the file list, check and avoid
          // duplication.
          const currentURLs = {};
          for (let i = 0; i < this.fileList_.length; i++) {
            currentURLs[this.fileList_.item(i).toURL()] = true;
          }
          entriesFiltered = entriesFiltered.filter(entry => {
            return !currentURLs[entry.toURL()];
          });
          // Update the filelist without waiting the metadata.
          this.fileList_.push.apply(this.fileList_, entriesFiltered);
          cr.dispatchSimpleEvent(this, 'scan-updated');
        }
        callbackOuter();
      };
      // Because the prefetchMetadata can be slow, throttling by splitting
      // entries into smaller chunks to reduce UI latency.
      // TODO(hidehiko,mtomasz): This should be handled in MetadataCache.
      const MAX_CHUNK_SIZE = 25;
      const prefetchMetadataQueue = new AsyncUtil.ConcurrentQueue(4);
      for (let i = 0; i < entries.length; i += MAX_CHUNK_SIZE) {
        if (prefetchMetadataQueue.isCancelled()) {
          break;
        }

        const chunk = entries.slice(i, i + MAX_CHUNK_SIZE);
        prefetchMetadataQueue.run(
            ((chunk, callbackInner) => {
              this.prefetchMetadata(chunk, refresh, () => {
                if (!prefetchMetadataQueue.isCancelled()) {
                  if (this.scanCancelled_) {
                    prefetchMetadataQueue.cancel();
                  }
                }

                // Checks if this is the last task.
                if (prefetchMetadataQueue.getWaitingTasksCount() === 0 &&
                    prefetchMetadataQueue.getRunningTasksCount() === 1) {
                  // |callbackOuter| in |finish| must be called before
                  // |callbackInner|, to prevent double-calling.
                  finish();
                }

                callbackInner();
              });
            }).bind(null, chunk));
      }
    });
  }

  /**
   * @param {!Array<!Entry>} entries Files.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {function(Object)} callback Callback on done.
   */
  prefetchMetadata(entries, refresh, callback) {
    if (refresh) {
      this.context_.metadataModel.notifyEntriesChanged(entries);
    }
    this.context_.metadataModel
        .get(entries, this.context_.prefetchPropertyNames)
        .then(callback);
  }

  /**
   * Creates a DirectoryContents instance to show entries in a directory.
   *
   * @param {FileListContext} context File list context.
   * @param {DirectoryEntry|FilesAppDirEntry} directoryEntry The current
   *     directory entry.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForDirectory(context, directoryEntry) {
    return new DirectoryContents(
        context,
        false,  // Non search.
        directoryEntry, () => {
          return new DirectoryContentScanner(directoryEntry);
        });
  }

  /**
   * Creates a DirectoryContents instance to show the result of the search on
   * Drive File System.
   *
   * @param {FileListContext} context File list context.
   * @param {DirectoryEntry} directoryEntry The current directory entry.
   * @param {string} query Search query.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForDriveSearch(context, directoryEntry, query) {
    return new DirectoryContents(
        context,
        true,  // Search.
        directoryEntry, () => {
          return new DriveSearchContentScanner(query);
        });
  }

  /**
   * Creates a DirectoryContents instance to show the result of the search on
   * Local File System.
   *
   * @param {FileListContext} context File list context.
   * @param {DirectoryEntry} directoryEntry The current directory entry.
   * @param {string} query Search query.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForLocalSearch(context, directoryEntry, query) {
    return new DirectoryContents(
        context,
        true,  // Search.
        directoryEntry, () => {
          return new LocalSearchContentScanner(directoryEntry, query);
        });
  }

  /**
   * Creates a DirectoryContents instance to show the result of metadata search
   * on Drive File System.
   *
   * @param {FileListContext} context File list context.
   * @param {!FakeEntry} fakeDirectoryEntry Fake directory entry representing
   *     the set of result entries. This serves as a top directory for the
   *     search.
   * @param {!DriveMetadataSearchContentScanner.SearchType} searchType The type
   *     of the search. The scanner will restricts the entries based on the
   *     given type.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForDriveMetadataSearch(context, fakeDirectoryEntry, searchType) {
    return new DirectoryContents(
        context,
        true,  // Search
        fakeDirectoryEntry, () => {
          return new DriveMetadataSearchContentScanner(searchType);
        });
  }

  /**
   * Creates a DirectoryContents instance to show the mixed recent files.
   *
   * @param {FileListContext} context File list context.
   * @param {!FakeEntry} recentRootEntry Fake directory entry representing the
   *     root of recent files.
   * @param {string} query Search query.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForRecent(context, recentRootEntry, query) {
    return new DirectoryContents(context, true, recentRootEntry, () => {
      return new RecentContentScanner(query, recentRootEntry.sourceRestriction);
    });
  }

  /**
   * Creates a DirectoryContents instance to show the flatten media views.
   *
   * @param {FileListContext} context File list context.
   * @param {!DirectoryEntry} rootEntry Root directory entry representing the
   *     root of each media view volume.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForMediaView(context, rootEntry) {
    return new DirectoryContents(context, true, rootEntry, () => {
      return new MediaViewContentScanner(rootEntry);
    });
  }

  /**
   * Creates a DirectoryContents instance to show the sshfs crostini files.
   *
   * @param {FileListContext} context File list context.
   * @param {!FakeEntry} crostiniRootEntry Fake directory entry representing the
   *     root of recent files.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForCrostiniMounter(context, crostiniRootEntry) {
    return new DirectoryContents(context, true, crostiniRootEntry, () => {
      return new CrostiniMounter();
    });
  }

  /**
   * Creates an empty DirectoryContents instance to show the Google Drive
   * placeholder that never completes loading.
   *
   * @param {FileListContext} context File list context.
   * @param {!FakeEntry} rootEntry Fake directory entry representing the fake
   *     root of Google Drive.
   * @return {DirectoryContents} Created DirectoryContents instance.
   */
  static createForFakeDrive(context, rootEntry) {
    return new DirectoryContents(context, true, rootEntry, () => {
      return new ContentScanner();
    });
  }
}
