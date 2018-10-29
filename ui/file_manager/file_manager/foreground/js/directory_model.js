// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If directory files changes too often, don't rescan directory more than once
// per specified interval
var SIMULTANEOUS_RESCAN_INTERVAL = 500;
// Used for operations that require almost instant rescan.
var SHORT_RESCAN_INTERVAL = 100;

/**
 * Data model of the file manager.
 *
 * @constructor
 * @extends {cr.EventTarget}
 *
 * @param {boolean} singleSelection True if only one file could be selected
 *                                  at the time.
 * @param {FileFilter} fileFilter Instance of FileFilter.
 * @param {!MetadataModel} metadataModel Metadata model.
 *     service.
 * @param {!VolumeManager} volumeManager The volume manager.
 * @param {!FileOperationManager} fileOperationManager File operation manager.
 * @param {!analytics.Tracker} tracker
 */
function DirectoryModel(
    singleSelection,
    fileFilter,
    metadataModel,
    volumeManager,
    fileOperationManager,
    tracker) {
  this.fileListSelection_ = singleSelection ?
      new FileListSingleSelectionModel() : new FileListSelectionModel();

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTime_ = null;
  this.scanFailures_ = 0;
  this.changeDirectorySequence_ = 0;

  /**
   * @private {boolean}
   */
  this.ignoreCurrentDirectoryDeletion_ = false;

  this.directoryChangeQueue_ = new AsyncUtil.Queue();
  this.rescanAggregator_ = new AsyncUtil.Aggregator(
      this.rescanSoon.bind(this, true), 500);

  this.fileFilter_ = fileFilter;
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.currentFileListContext_ =
      new FileListContext(fileFilter, metadataModel);
  this.currentDirContents_ =
      DirectoryContents.createForDirectory(this.currentFileListContext_, null);
  /**
   * Empty file list which is used as a dummy for inactive view of file list.
   * @private {!FileListModel}
   */
  this.emptyFileList_ = new FileListModel(metadataModel);

  this.metadataModel_ = metadataModel;

  this.volumeManager_ = volumeManager;
  this.volumeManager_.volumeInfoList.addEventListener(
      'splice', this.onVolumeInfoListUpdated_.bind(this));

  /**
   * File watcher.
   * @private {!FileWatcher}
   * @const
   */
  this.fileWatcher_ = new FileWatcher();
  this.fileWatcher_.addEventListener(
      'watcher-directory-changed',
      this.onWatcherDirectoryChanged_.bind(this));
  util.addEventListenerToBackgroundComponent(
      fileOperationManager,
      'entries-changed',
      this.onEntriesChanged_.bind(this));

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /** @private {string} */
  this.lastSearchQuery_ = '';
}

/**
 * DirectoryModel extends cr.EventTarget.
 */
DirectoryModel.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Disposes the directory model by removing file watchers.
 */
DirectoryModel.prototype.dispose = function() {
  this.fileWatcher_.dispose();
};

/**
 * @return {FileListModel} Files in the current directory.
 */
DirectoryModel.prototype.getFileList = function() {
  return this.currentFileListContext_.fileList;
};

/**
 * @return {!FileListModel} File list which is always empty.
 */
DirectoryModel.prototype.getEmptyFileList = function() {
  return this.emptyFileList_;
};

/**
 * @return {!FileListSelectionModel|!FileListSingleSelectionModel} Selection
 * in the fileList.
 */
DirectoryModel.prototype.getFileListSelection = function() {
  return this.fileListSelection_;
};

/**
 * Obtains current volume information.
 * @return {VolumeInfo}
 */
DirectoryModel.prototype.getCurrentVolumeInfo = function() {
  var entry = this.getCurrentDirEntry();
  if (!entry)
    return null;
  return this.volumeManager_.getVolumeInfo(entry);
};

/**
 * @return {?VolumeManagerCommon.RootType} Root type of current root, or null if
 *     not found.
 */
DirectoryModel.prototype.getCurrentRootType = function() {
  var entry = this.currentDirContents_.getDirectoryEntry();
  if (!entry)
    return null;

  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  if (!locationInfo)
    return null;

  return locationInfo.rootType;
};

/**
 * Metadata property names that are expected to be Prefetched.
 * @return {!Array<string>}
 */
DirectoryModel.prototype.getPrefetchPropertyNames = function() {
  return this.currentFileListContext_.prefetchPropertyNames;
};

/**
 * @return {boolean} True if the current directory is read only. If there is
 *     no entry set, then returns true.
 */
DirectoryModel.prototype.isReadOnly = function() {
  var currentDirEntry = this.getCurrentDirEntry();
  if (currentDirEntry) {
    var locationInfo = this.volumeManager_.getLocationInfo(currentDirEntry);
    if (locationInfo)
      return locationInfo.isReadOnly;
  }
  return true;
};

/**
 * @return {boolean} True if the a scan is active.
 */
DirectoryModel.prototype.isScanning = function() {
  return this.currentDirContents_.isScanning();
};

/**
 * @return {boolean} True if search is in progress.
 */
DirectoryModel.prototype.isSearching = function() {
  return this.currentDirContents_.isSearch();
};

/**
 * @return {boolean} True if it's on Drive.
 */
DirectoryModel.prototype.isOnDrive = function() {
  return this.isCurrentRootVolumeType_(VolumeManagerCommon.VolumeType.DRIVE);
};

/**
 * @return {boolean} True if it's on MTP volume.
 */
DirectoryModel.prototype.isOnMTP = function() {
  return this.isCurrentRootVolumeType_(VolumeManagerCommon.VolumeType.MTP);
};

/**
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume Type
 * @return {boolean} True if current root volume type is equal to specified
 *     volume type.
 * @private
 */
DirectoryModel.prototype.isCurrentRootVolumeType_ = function(volumeType) {
  var rootType = this.getCurrentRootType();
  return rootType != null && rootType != VolumeManagerCommon.RootType.RECENT &&
      VolumeManagerCommon.getVolumeTypeFromRootType(rootType) === volumeType;
};

/**
 * Updates the selection by using the updateFunc and publish the change event.
 * If updateFunc returns true, it force to dispatch the change event even if the
 * selection index is not changed.
 *
 * @param {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel} selection
 *     Selection to be updated.
 * @param {function(): boolean} updateFunc Function updating the selection.
 * @private
 */
DirectoryModel.prototype.updateSelectionAndPublishEvent_ =
    function(selection, updateFunc) {
  // Begin change.
  selection.beginChange();

  // If dispatchNeeded is true, we should ensure the change event is
  // dispatched.
  var dispatchNeeded = updateFunc();

  // Check if the change event is dispatched in the endChange function
  // or not.
  var eventDispatched = function() { dispatchNeeded = false; };
  selection.addEventListener('change', eventDispatched);
  selection.endChange();
  selection.removeEventListener('change', eventDispatched);

  // If the change event have been already dispatched, dispatchNeeded is false.
  if (dispatchNeeded) {
    var event = new Event('change');
    // The selection status (selected or not) is not changed because
    // this event is caused by the change of selected item.
    event.changes = [];
    selection.dispatchEvent(event);
  }
};

/**
 * Sets to ignore current directory deletion. This method is used to prevent
 * going up to the volume root with the deletion of current directory by rename
 * operation in directory tree.
 * @param {boolean} value True to ignore current directory deletion.
 */
DirectoryModel.prototype.setIgnoringCurrentDirectoryDeletion = function(value) {
  this.ignoreCurrentDirectoryDeletion_ = value;
};

/**
 * Invoked when a change in the directory is detected by the watcher.
 * @param {Event} event Event object.
 * @private
 */
DirectoryModel.prototype.onWatcherDirectoryChanged_ = function(event) {
  var directoryEntry = this.getCurrentDirEntry();

  if (!this.ignoreCurrentDirectoryDeletion_) {
    // If the change is deletion of currentDir, move up to its parent directory.
    directoryEntry.getDirectory(
        directoryEntry.fullPath, {create: false}, function() {},
        function() {
          var volumeInfo =
              this.volumeManager_.getVolumeInfo(assert(directoryEntry));
          if (volumeInfo) {
            volumeInfo.resolveDisplayRoot().then(function(displayRoot) {
              this.changeDirectoryEntry(displayRoot);
            }.bind(this));
          }
        }.bind(this));
  }

  if (event.changedFiles) {
    var addedOrUpdatedFileUrls = [];
    var deletedFileUrls = [];
    event.changedFiles.forEach(function(change) {
      if (change.changes.length === 1 && change.changes[0] === 'delete')
        deletedFileUrls.push(change.url);
      else
        addedOrUpdatedFileUrls.push(change.url);
    });

    util.URLsToEntries(addedOrUpdatedFileUrls).then(function(result) {
      deletedFileUrls = deletedFileUrls.concat(result.failureUrls);

      // Passing the resolved entries and failed URLs as the removed files.
      // The URLs are removed files and they chan't be resolved.
      this.partialUpdate_(result.entries, deletedFileUrls);
    }.bind(this)).catch(function(error) {
      console.error('Error in proceeding the changed event.', error,
                    'Fallback to force-refresh');
      this.rescanAggregator_.run();
    }.bind(this));
  } else {
    // Invokes force refresh if the detailed information isn't provided.
    // This can occur very frequently (e.g. when copying files into Downlaods)
    // and rescan is heavy operation, so we keep some interval for each rescan.
    this.rescanAggregator_.run();
  }
};

/**
 * Invoked when filters are changed.
 * @private
 */
DirectoryModel.prototype.onFilterChanged_ = function() {
  const currentDirectory = this.getCurrentDirEntry();
  if (currentDirectory && util.isNativeEntry(currentDirectory) &&
      !this.fileFilter_.filter(
          /** @type {!DirectoryEntry} */ (currentDirectory))) {
    // If the current directory should be hidden in the new filter setting,
    // change the current directory to the current volume's root.
    const volumeInfo = this.volumeManager_.getVolumeInfo(currentDirectory);
    if (volumeInfo) {
      volumeInfo.resolveDisplayRoot().then(displayRoot => {
        this.changeDirectoryEntry(displayRoot);
      });
    }
  } else {
    this.rescanSoon(false);
  }
};

/**
 * Returns the filter.
 * @return {FileFilter} The file filter.
 */
DirectoryModel.prototype.getFileFilter = function() {
  return this.fileFilter_;
};

/**
 * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} Current directory.
 */
DirectoryModel.prototype.getCurrentDirEntry = function() {
  return this.currentDirContents_.getDirectoryEntry();
};

/**
 * @return {Array<Entry>} Array of selected entries.
 * @private
 */
DirectoryModel.prototype.getSelectedEntries_ = function() {
  var indexes = this.fileListSelection_.selectedIndexes;
  var fileList = this.getFileList();
  if (fileList) {
    return indexes.map(function(i) {
      return fileList.item(i);
    });
  }
  return [];
};

/**
 * @param {Array<Entry>} value List of selected entries.
 * @private
 */
DirectoryModel.prototype.setSelectedEntries_ = function(value) {
  var indexes = [];
  var fileList = this.getFileList();
  var urls = util.entriesToURLs(value);

  for (var i = 0; i < fileList.length; i++) {
    if (urls.indexOf(fileList.item(i).toURL()) !== -1)
      indexes.push(i);
  }
  this.fileListSelection_.selectedIndexes = indexes;
};

/**
 * @return {Entry} Lead entry.
 * @private
 */
DirectoryModel.prototype.getLeadEntry_ = function() {
  var index = this.fileListSelection_.leadIndex;
  return index >= 0 ?
      /** @type {Entry} */ (this.getFileList().item(index)) : null;
};

/**
 * @param {Entry} value The new lead entry.
 * @private
 */
DirectoryModel.prototype.setLeadEntry_ = function(value) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (util.isSameEntry(/** @type {Entry} */ (fileList.item(i)), value)) {
      this.fileListSelection_.leadIndex = i;
      return;
    }
  }
};

/**
 * Schedule rescan with short delay.
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 */
DirectoryModel.prototype.rescanSoon = function(refresh) {
  this.scheduleRescan(SHORT_RESCAN_INTERVAL, refresh);
};

/**
 * Schedule rescan with delay. Designed to handle directory change
 * notification.
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 */
DirectoryModel.prototype.rescanLater = function(refresh) {
  this.scheduleRescan(SIMULTANEOUS_RESCAN_INTERVAL, refresh);
};

/**
 * Schedule rescan with delay. If another rescan has been scheduled does
 * nothing. File operation may cause a few notifications what should cause
 * a single refresh.
 * @param {number} delay Delay in ms after which the rescan will be performed.
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 */
DirectoryModel.prototype.scheduleRescan = function(delay, refresh) {
  if (this.rescanTime_) {
    if (this.rescanTime_ <= Date.now() + delay)
      return;
    clearTimeout(this.rescanTimeoutId_);
  }

  var sequence = this.changeDirectorySequence_;

  this.rescanTime_ = Date.now() + delay;
  this.rescanTimeoutId_ = setTimeout(function() {
    this.rescanTimeoutId_ = null;
    if (sequence === this.changeDirectorySequence_)
      this.rescan(refresh);
  }.bind(this), delay);
};

/**
 * Cancel a rescan on timeout if it is scheduled.
 * @private
 */
DirectoryModel.prototype.clearRescanTimeout_ = function() {
  this.rescanTime_ = null;
  if (this.rescanTimeoutId_) {
    clearTimeout(this.rescanTimeoutId_);
    this.rescanTimeoutId_ = null;
  }
};

/**
 * Rescan current directory. May be called indirectly through rescanLater or
 * directly in order to reflect user action. Will first cache all the directory
 * contents in an array, then seamlessly substitute the fileList contents,
 * preserving the select element etc.
 *
 * This should be to scan the contents of current directory (or search).
 *
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 */
DirectoryModel.prototype.rescan = function(refresh) {
  this.clearRescanTimeout_();
  if (this.runningScan_) {
    this.pendingRescan_ = true;
    return;
  }

  var dirContents = this.currentDirContents_.clone();
  dirContents.setFileList([]);
  dirContents.setMetadataSnapshot(
      this.currentDirContents_.createMetadataSnapshot());

  var sequence = this.changeDirectorySequence_;

  var successCallback = (function() {
    if (sequence === this.changeDirectorySequence_) {
      this.replaceDirectoryContents_(dirContents);
      cr.dispatchSimpleEvent(this, 'rescan-completed');
    }
  }).bind(this);

  this.scan_(dirContents,
             refresh,
             successCallback, function() {}, function() {}, function() {});
};

/**
 * Run scan on the current DirectoryContents. The active fileList is cleared and
 * the entries are added directly.
 *
 * This should be used when changing directory or initiating a new search.
 *
 * @param {DirectoryContents} newDirContents New DirectoryContents instance to
 *     replace currentDirContents_.
 * @param {function(boolean)} callback Callback with result. True if the scan
 *     is completed successfully, false if the scan is failed.
 * @private
 */
DirectoryModel.prototype.clearAndScan_ = function(newDirContents,
                                                  callback) {
  if (this.currentDirContents_.isScanning())
    this.currentDirContents_.cancelScan();
  this.currentDirContents_ = newDirContents;
  this.clearRescanTimeout_();

  if (this.pendingScan_)
    this.pendingScan_ = false;

  if (this.runningScan_) {
    if (this.runningScan_.isScanning())
      this.runningScan_.cancelScan();
    this.runningScan_ = null;
  }

  var sequence = this.changeDirectorySequence_;
  var cancelled = false;

  var onDone = function() {
    if (cancelled)
      return;

    cr.dispatchSimpleEvent(this, 'scan-completed');
    callback(true);
  }.bind(this);

  /** @param {DOMError} error error. */
  var onFailed = function(error) {
    if (cancelled)
      return;

    var event = new Event('scan-failed');
    event.error = error;
    this.dispatchEvent(event);
    callback(false);
  }.bind(this);

  var onUpdated = function() {
    if (cancelled)
      return;

    if (this.changeDirectorySequence_ !== sequence) {
      cancelled = true;
      cr.dispatchSimpleEvent(this, 'scan-cancelled');
      callback(false);
      return;
    }

    cr.dispatchSimpleEvent(this, 'scan-updated');
  }.bind(this);

  var onCancelled = function() {
    if (cancelled)
      return;

    cancelled = true;
    cr.dispatchSimpleEvent(this, 'scan-cancelled');
    callback(false);
  }.bind(this);

  // Clear metadata information for the old (no longer visible) items in the
  // file list.
  var fileList = this.getFileList();
  let removedUrls = [];
  for (var i = 0; i < fileList.length; i++) {
    removedUrls.push(fileList.item(i).toURL());
  }
  this.metadataModel_.notifyEntriesRemoved(removedUrls);

  // Retrieve metadata information for the newly selected directory.
  const currentEntry = this.currentDirContents_.getDirectoryEntry();
  if (currentEntry && !util.isFakeEntry(assert(currentEntry))) {
    this.metadataModel_.get(
        [currentEntry],
        constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES);
  }

  // Clear the table, and start scanning.
  cr.dispatchSimpleEvent(this, 'scan-started');
  fileList.splice(0, fileList.length);
  this.scan_(this.currentDirContents_, false,
             onDone, onFailed, onUpdated, onCancelled);
};

/**
 * Adds/removes/updates items of file list.
 * @param {Array<Entry>} changedEntries Entries of updated/added files.
 * @param {Array<string>} removedUrls URLs of removed files.
 * @private
 */
DirectoryModel.prototype.partialUpdate_ =
    function(changedEntries, removedUrls) {
  // This update should be included in the current running update.
  if (this.pendingScan_)
    return;

  if (this.runningScan_) {
    // Do update after the current scan is finished.
    var previousScan = this.runningScan_;
    var onPreviousScanCompleted = function() {
      previousScan.removeEventListener('scan-completed',
                                       onPreviousScanCompleted);
      // Run the update asynchronously.
      Promise.resolve().then(function() {
        this.partialUpdate_(changedEntries, removedUrls);
      }.bind(this));
    }.bind(this);
    previousScan.addEventListener('scan-completed', onPreviousScanCompleted);
    return;
  }

  var onFinish = function() {
    this.runningScan_ = null;

    this.currentDirContents_.removeEventListener(
        'scan-completed', onCompleted);
    this.currentDirContents_.removeEventListener('scan-failed', onFailure);
    this.currentDirContents_.removeEventListener(
        'scan-cancelled', onCancelled);
  }.bind(this);

  var onCompleted = function() {
    onFinish();
    cr.dispatchSimpleEvent(this, 'rescan-completed');
  }.bind(this);

  var onFailure = function() {
    onFinish();
  };

  var onCancelled = function() {
    onFinish();
  };

  this.runningScan_ = this.currentDirContents_;
  this.currentDirContents_.addEventListener('scan-completed', onCompleted);
  this.currentDirContents_.addEventListener('scan-failed', onFailure);
  this.currentDirContents_.addEventListener('scan-cancelled', onCancelled);
  this.currentDirContents_.update(changedEntries, removedUrls);
};

/**
 * Perform a directory contents scan. Should be called only from rescan() and
 * clearAndScan_().
 *
 * @param {DirectoryContents} dirContents DirectoryContents instance on which
 *     the scan will be run.
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 * @param {function()} successCallback Callback on success.
 * @param {function(DOMError)} failureCallback Callback on failure.
 * @param {function()} updatedCallback Callback on update. Only on the last
 *     update, {@code successCallback} is called instead of this.
 * @param {function()} cancelledCallback Callback on cancel.
 * @private
 */
DirectoryModel.prototype.scan_ = function(
    dirContents,
    refresh,
    successCallback, failureCallback, updatedCallback, cancelledCallback) {
  var self = this;

  /**
   * Runs pending scan if there is one.
   *
   * @return {boolean} Did pending scan exist.
   */
  var maybeRunPendingRescan = function() {
    if (this.pendingRescan_) {
      this.rescanSoon(refresh);
      this.pendingRescan_ = false;
      return true;
    }
    return false;
  }.bind(this);

  var onFinished = function() {
    dirContents.removeEventListener('scan-completed', onSuccess);
    dirContents.removeEventListener('scan-updated', updatedCallback);
    dirContents.removeEventListener('scan-failed', onFailure);
    dirContents.removeEventListener('scan-cancelled', cancelledCallback);
  };

  var onSuccess = function() {
    onFinished();

    // Record metric for Downloads directory.
    if (!dirContents.isSearch()) {
      var locationInfo =
          this.volumeManager_.getLocationInfo(
              assert(dirContents.getDirectoryEntry()));
      var volumeInfo = locationInfo.volumeInfo;
      if (volumeInfo &&
          volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS &&
          locationInfo.isRootEntry) {
        metrics.recordMediumCount('DownloadsCount',
                                  dirContents.fileList_.length);
      }
    }

    this.runningScan_ = null;
    successCallback();
    this.scanFailures_ = 0;
    maybeRunPendingRescan();
  }.bind(this);

  var onFailure = function(event) {
    onFinished();

    this.runningScan_ = null;
    this.scanFailures_++;
    failureCallback(event.error);

    if (maybeRunPendingRescan())
      return;

    // Do not rescan for crostini errors.
    if (event.error.name === DirectoryModel.CROSTINI_CONNECT_ERR)
      return;

    if (this.scanFailures_ <= 1)
      this.rescanLater(refresh);
  }.bind(this);

  var onCancelled = function() {
    onFinished();
    cancelledCallback();
  };

  this.runningScan_ = dirContents;

  dirContents.addEventListener('scan-completed', onSuccess);
  dirContents.addEventListener('scan-updated', updatedCallback);
  dirContents.addEventListener('scan-failed', onFailure);
  dirContents.addEventListener('scan-cancelled', onCancelled);
  dirContents.scan(refresh);
};

/**
 * @param {DirectoryContents} dirContents DirectoryContents instance. This must
 *     be a different instance from this.currentDirContents_.
 * @private
 */
DirectoryModel.prototype.replaceDirectoryContents_ = function(dirContents) {
  console.assert(this.currentDirContents_ !== dirContents,
      'Give directory contents instance must be different from current one.');
  cr.dispatchSimpleEvent(this, 'begin-update-files');
  this.updateSelectionAndPublishEvent_(this.fileListSelection_, function() {
    var selectedEntries = this.getSelectedEntries_();
    var selectedIndices = this.fileListSelection_.selectedIndexes;

    // Restore leadIndex in case leadName no longer exists.
    var leadIndex = this.fileListSelection_.leadIndex;
    var leadEntry = this.getLeadEntry_();

    var previousDirContents = this.currentDirContents_;
    this.currentDirContents_ = dirContents;
    this.currentDirContents_.replaceContextFileList();

    this.setSelectedEntries_(selectedEntries);
    this.fileListSelection_.leadIndex = leadIndex;
    this.setLeadEntry_(leadEntry);

    // If nothing is selected after update, then select file next to the
    // latest selection
    var forceChangeEvent = false;
    if (this.fileListSelection_.selectedIndexes.length == 0 &&
        selectedIndices.length != 0) {
      var maxIdx = Math.max.apply(null, selectedIndices);
      this.selectIndex(Math.min(maxIdx - selectedIndices.length + 2,
                                this.getFileList().length) - 1);
      forceChangeEvent = true;
    }
    return forceChangeEvent;
  }.bind(this));

  cr.dispatchSimpleEvent(this, 'end-update-files');
};

/**
 * Callback when an entry is changed.
 * @param {EntriesChangedEvent} event Entry change event.
 * @private
 */
DirectoryModel.prototype.onEntriesChanged_ = function(event) {
  var kind = event.kind;
  var entries = event.entries;
  // TODO(hidehiko): We should update directory model even the search result
  // is shown.
  var rootType = this.getCurrentRootType();
  if ((rootType === VolumeManagerCommon.RootType.DRIVE ||
       rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
       rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
       rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE) &&
      this.isSearching())
    return;

  switch (kind) {
    case util.EntryChangedKind.CREATED:
      var parentPromises = [];
      for (var i = 0; i < entries.length; i++) {
        parentPromises.push(new Promise(function(resolve, reject) {
          entries[i].getParent(resolve, reject);
        }));
      }
      Promise.all(parentPromises).then(function(parents) {
        var entriesToAdd = [];
        for (var i = 0; i < parents.length; i++) {
          if (!util.isSameEntry(parents[i], this.getCurrentDirEntry()))
            continue;
          var index = this.findIndexByEntry_(entries[i]);
          if (index >= 0) {
            this.getFileList().replaceItem(
                this.getFileList().item(index), entries[i]);
          } else {
            entriesToAdd.push(entries[i]);
          }
        }
        this.partialUpdate_(entriesToAdd, []);
      }.bind(this)).catch(function(error) {
        console.error(error.stack || error);
      });
      break;

    case util.EntryChangedKind.DELETED:
      // This is the delete event.
      this.partialUpdate_([], util.entriesToURLs(entries));
      break;

    default:
      console.error('Invalid EntryChangedKind: ' + kind);
      break;
  }
};

/**
 * @param {Entry} entry The entry to be searched.
 * @return {number} The index in the fileList, or -1 if not found.
 * @private
 */
DirectoryModel.prototype.findIndexByEntry_ = function(entry) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (util.isSameEntry(/** @type {Entry} */ (fileList.item(i)), entry))
      return i;
  }
  return -1;
};

/**
 * Called when rename is done successfully.
 * Note: conceptually, DirectoryModel should work without this, because entries
 * can be renamed by other systems anytime and the Files app should reflect it
 * correctly.
 * TODO(hidehiko): investigate more background, and remove this if possible.
 *
 * @param {!Entry} oldEntry The old entry.
 * @param {!Entry} newEntry The new entry.
 * @param {function()=} opt_callback Called on completion.
 */
DirectoryModel.prototype.onRenameEntry = function(
    oldEntry, newEntry, opt_callback) {
  this.currentDirContents_.prefetchMetadata([newEntry], true, function() {
    // If the current directory is the old entry, then quietly change to the
    // new one.
    if (util.isSameEntry(oldEntry, this.getCurrentDirEntry())) {
      this.changeDirectoryEntry(
          /** @type {!DirectoryEntry|!FilesAppDirEntry} */ (newEntry));
    }

    // Replace the old item with the new item. oldEntry instance itself may
    // have been removed/replaced from the list during the async process, we
    // find an entry which should be replaced by checking toURL().
    var list = this.getFileList();
    var oldEntryExist = false;
    var newEntryExist = false;
    var oldEntryUrl = oldEntry.toURL();
    var newEntryUrl = newEntry.toURL();

    for (var i = 0; i < list.length; i++) {
      var item = list.item(i);
      var url = item.toURL();
      if (url === oldEntryUrl) {
        list.replaceItem(item, newEntry);
        oldEntryExist = true;
        break;
      }

      if (url === newEntryUrl) {
        newEntryExist = true;
      }
    }

    // When both old and new entries don't exist, it may be in the middle of
    // update process. In DirectoryContent.update deletion is executed at first
    // and insertion is executed as a async call. There is a chance that this
    // method is called in the middle of update process.
    if (!oldEntryExist && !newEntryExist)
      list.push(newEntry);

    // Run callback, finally.
    if (opt_callback)
      opt_callback();
  }.bind(this));
};

/**
 * Updates data model and selects new directory.
 * @param {!DirectoryEntry} newDirectory Directory entry to be selected.
 * @return {Promise} A promise which is resolved when new directory is selected.
 *     If current directory has changed during the operation, this will be
 *     rejected.
 */
DirectoryModel.prototype.updateAndSelectNewDirectory = function(newDirectory) {
  // Refresh the cache.
  this.metadataModel_.notifyEntriesCreated([newDirectory]);
  var dirContents = this.currentDirContents_;

  return new Promise(function(onFulfilled, onRejected) {
    dirContents.prefetchMetadata(
        [newDirectory], false, onFulfilled);
  }).then(function(sequence) {
    // If current directory has changed during the prefetch, do not try to
    // select new directory.
    if (sequence !== this.changeDirectorySequence_)
      return Promise.reject();

    // If target directory is already in the list, just select it.
    var existing = this.getFileList().slice().filter(
        function(e) { return e.name === newDirectory.name; });
    if (existing.length) {
      this.selectEntry(newDirectory);
    } else {
      this.fileListSelection_.beginChange();
      this.getFileList().splice(0, 0, newDirectory);
      this.selectEntry(newDirectory);
      this.fileListSelection_.endChange();
    }
  }.bind(this, this.changeDirectorySequence_));
};

/**
 * Changes the current directory to the directory represented by
 * a DirectoryEntry or a fake entry.
 *
 * Dispatches the 'directory-changed' event when the directory is successfully
 * changed.
 *
 * Note : if this is called from UI, please consider to use DirectoryModel.
 * activateDirectoryEntry instead of this, which is higher-level function and
 * cares about the selection.
 *
 * @param {!DirectoryEntry|!FilesAppDirEntry} dirEntry The entry of the new
 *     directory to be opened.
 * @param {function()=} opt_callback Executed if the directory loads
 *     successfully.
 */
DirectoryModel.prototype.changeDirectoryEntry = function(
    dirEntry, opt_callback) {
  // If it's a VolumeEntry which wraps an actual entry, we should use the
  // unwrapped entry.
  if (dirEntry instanceof VolumeEntry)
    dirEntry = assert(dirEntry.rootEntry);

  // Increment the sequence value.
  this.changeDirectorySequence_++;
  this.clearSearch_();

  // If there is on-going scan, cancel it.
  if (this.currentDirContents_.isScanning())
    this.currentDirContents_.cancelScan();

  this.directoryChangeQueue_.run(function(sequence, queueTaskCallback) {
    this.fileWatcher_.changeWatchedDirectory(dirEntry)
        .then(function() {
          if (this.changeDirectorySequence_ !== sequence) {
            queueTaskCallback();
            return;
          }

          var newDirectoryContents = this.createDirectoryContents_(
              this.currentFileListContext_, dirEntry, '');
          if (!newDirectoryContents) {
            queueTaskCallback();
            return;
          }

          var previousDirEntry =
              this.currentDirContents_.getDirectoryEntry();
          this.clearAndScan_(
               newDirectoryContents,
               function(result) {
                 // Calls the callback of the method when successful.
                 if (result && opt_callback)
                   opt_callback();

                 // Notify that the current task of this.directoryChangeQueue_
                 // is completed.
                 setTimeout(queueTaskCallback, 0);
               });

          // For tests that open the dialog to empty directories, everything
          // is loaded at this point.
          util.testSendMessage('directory-change-complete');
          var previousVolumeInfo =
              previousDirEntry ?
              this.volumeManager_.getVolumeInfo(previousDirEntry) : null;
          // VolumeInfo for dirEntry.
          var currentVolumeInfo = this.getCurrentVolumeInfo();
          var event = new Event('directory-changed');
          event.previousDirEntry = previousDirEntry;
          event.newDirEntry = dirEntry;
          event.volumeChanged = previousVolumeInfo !== currentVolumeInfo;
          this.dispatchEvent(event);

          if (currentVolumeInfo && event.volumeChanged) {
            this.onVolumeChanged_(assert(currentVolumeInfo));
          }
        }.bind(this));
  }.bind(this, this.changeDirectorySequence_));
};

/**
 * Handles volume changed by sending an analytics appView event.
 *
 * @param {!VolumeInfo} volumeInfo The new volume info.
 * @return {!Promise} resolves once handling is done.
 * @private
 */
DirectoryModel.prototype.onVolumeChanged_ = function(volumeInfo) {
  // NOTE: That dynamic values, like volume name MUST NOT
  // be sent to GA as that value can contain PII.
  // VolumeType is an enum.
  // ...
  // But we can do stuff like figure out if this is a media device or vanilla
  // removable device.
  return Promise.resolve(undefined)
      .then(
          (/** @this {DirectoryModel} */
          function() {
            switch (volumeInfo.volumeType) {
              case VolumeManagerCommon.VolumeType.REMOVABLE:
                return importer.hasMediaDirectory(volumeInfo.fileSystem.root)
                    .then(
                        /**
                         * @param {boolean} hasMedia
                         * @return {string}
                         */
                        function(hasMedia) {
                          return hasMedia ?
                              volumeInfo.volumeType + ':with-media-dir' :
                              volumeInfo.volumeType;
                        });
              case VolumeManagerCommon.VolumeType.PROVIDED:
                var providerId = volumeInfo.providerId;
                var name = metrics.getFileSystemProviderName(providerId);
                // Make note of an unrecognized provider id. When we see
                // high counts for a particular id, we should add it to the
                // whitelist in metrics_events.js.
                if (providerId && name == 'unknown') {
                  this.tracker_.send(
                      metrics.Internals.UNRECOGNIZED_FILE_SYSTEM_PROVIDER.label(
                          providerId));
                }
                return volumeInfo.volumeType + ':' + name;
              default:
                return volumeInfo.volumeType;
            }
          }).bind(this))
      .then(this.tracker_.sendAppView.bind(this.tracker_));
};

/**
 * Activates the given directory.
 * This method:
 *  - Changes the current directory, if the given directory is not the current
 *    directory.
 *  - Clears the selection, if the given directory is the current directory.
 *
 * @param {!DirectoryEntry|!FilesAppDirEntry} dirEntry The entry of the new
 *     directory to be opened.
 * @param {function()=} opt_callback Executed if the directory loads
 *     successfully.
 */
DirectoryModel.prototype.activateDirectoryEntry = function(
    dirEntry, opt_callback) {
  var currentDirectoryEntry = this.getCurrentDirEntry();
  if (currentDirectoryEntry &&
      util.isSameEntry(dirEntry, currentDirectoryEntry)) {
    // On activating the current directory, clear the selection on the filelist.
    this.clearSelection();
  } else {
    // Otherwise, changes the current directory.
    this.changeDirectoryEntry(dirEntry, opt_callback);
  }
};

/**
 * Clears the selection in the file list.
 */
DirectoryModel.prototype.clearSelection = function() {
  this.setSelectedEntries_([]);
};

/**
 * Creates an object which could say whether directory has changed while it has
 * been active or not. Designed for long operations that should be cancelled
 * if the used change current directory.
 * @return {Object} Created object.
 */
DirectoryModel.prototype.createDirectoryChangeTracker = function() {
  var tracker = {
    dm_: this,
    active_: false,
    hasChanged: false,

    start: function() {
      if (!this.active_) {
        this.dm_.addEventListener('directory-changed',
                                  this.onDirectoryChange_);
        this.active_ = true;
        this.hasChanged = false;
      }
    },

    stop: function() {
      if (this.active_) {
        this.dm_.removeEventListener('directory-changed',
                                     this.onDirectoryChange_);
        this.active_ = false;
      }
    },

    onDirectoryChange_: function(event) {
      tracker.stop();
      tracker.hasChanged = true;
    }
  };
  return tracker;
};

/**
 * @param {Entry} entry Entry to be selected.
 */
DirectoryModel.prototype.selectEntry = function(entry) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (fileList.item(i).toURL() === entry.toURL()) {
      this.selectIndex(i);
      return;
    }
  }
};

/**
 * @param {Array<Entry>} entries Array of entries.
 */
DirectoryModel.prototype.selectEntries = function(entries) {
  // URLs are needed here, since we are comparing Entries by URLs.
  var urls = util.entriesToURLs(entries);
  var fileList = this.getFileList();
  this.fileListSelection_.beginChange();
  this.fileListSelection_.unselectAll();
  for (var i = 0; i < fileList.length; i++) {
    if (urls.indexOf(fileList.item(i).toURL()) >= 0)
      this.fileListSelection_.setIndexSelected(i, true);
  }
  this.fileListSelection_.endChange();
};

/**
 * @param {number} index Index of file.
 */
DirectoryModel.prototype.selectIndex = function(index) {
  // this.focusCurrentList_();
  if (index >= this.getFileList().length)
    return;

  // If a list bound with the model it will do scrollIndexIntoView(index).
  this.fileListSelection_.selectedIndex = index;
};

/**
 * Handles update of VolumeInfoList.
 * @param {Event} event Event of VolumeInfoList's 'splice'.
 * @private
 */
DirectoryModel.prototype.onVolumeInfoListUpdated_ = function(event) {
  // When the volume where we are is unmounted, fallback to the default volume's
  // root. If current directory path is empty, stop the fallback
  // since the current directory is initializing now.
  const entry = this.getCurrentDirEntry();
  if (entry && !this.volumeManager_.getVolumeInfo(entry)) {
    this.volumeManager_.getDefaultDisplayRoot((displayRoot) => {
      if (displayRoot)
        this.changeDirectoryEntry(displayRoot);
    });
  }

  // If a new file backed provided volume is mounted,
  // then redirect to it in the focused window.
  // If crostini is mounted, redirect even if window is not focused.
  // Note, that this is a temporary solution for https://crbug.com/427776.
  if (event.added.length !== 1)
    return;
  if ((window.isFocused() &&
       event.added[0].volumeType === VolumeManagerCommon.VolumeType.PROVIDED &&
       event.added[0].source === VolumeManagerCommon.Source.FILE) ||
      event.added[0].volumeType === VolumeManagerCommon.VolumeType.CROSTINI) {
    event.added[0].resolveDisplayRoot().then((displayRoot) => {
      // Resolving a display root on FSP volumes is instant, despite the
      // asynchronous call.
      this.changeDirectoryEntry(event.added[0].displayRoot);
    });
  }
};

/**
 * Creates directory contents for the entry and query.
 *
 * @param {FileListContext} context File list context.
 * @param {!DirectoryEntry|!FilesAppEntry} entry Current directory.
 * @param {string=} opt_query Search query string.
 * @return {DirectoryContents} Directory contents.
 * @private
 */
DirectoryModel.prototype.createDirectoryContents_ =
    function(context, entry, opt_query) {
  var query = (opt_query || '').trimLeft();
  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  var canUseDriveSearch = this.volumeManager_.getDriveConnectionState().type !==
          VolumeManagerCommon.DriveConnectionType.OFFLINE &&
      (locationInfo && locationInfo.isDriveBased);

  if (entry.rootType == VolumeManagerCommon.RootType.RECENT) {
    return DirectoryContents.createForRecent(
        context, /** @type {!FakeEntry} */ (entry), query);
  }
  if (entry.rootType == VolumeManagerCommon.RootType.CROSTINI) {
    return DirectoryContents.createForCrostiniMounter(
        context, /** @type {!FakeEntry} */ (entry));
  }
  if (entry.rootType == VolumeManagerCommon.RootType.MY_FILES) {
    return DirectoryContents.createForDirectory(
        context, /** @type {!FilesAppDirEntry} */ (entry));
  }
  if (query && canUseDriveSearch) {
    // Drive search.
    return DirectoryContents.createForDriveSearch(
        context, /** @type {!DirectoryEntry} */ (entry), query);
  }
  if (query) {
    // Local search.
    return DirectoryContents.createForLocalSearch(
        context, /** @type {!DirectoryEntry} */ (entry), query);
  }

  if (!locationInfo)
    return null;

  if (locationInfo.rootType == VolumeManagerCommon.RootType.MEDIA_VIEW) {
    return DirectoryContents.createForMediaView(
        context, /** @type {!DirectoryEntry} */ (entry));
  }

  if (locationInfo.isSpecialSearchRoot) {
    // Drive special search.
    var searchType;
    switch (locationInfo.rootType) {
      case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
        searchType =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_OFFLINE;
        break;
      case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
        searchType =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_SHARED_WITH_ME;
        break;
      case VolumeManagerCommon.RootType.DRIVE_RECENT:
        searchType =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_RECENT_FILES;
        break;
      default:
        // Unknown special search entry.
        throw new Error('Unknown special search type.');
    }
    return DirectoryContents.createForDriveMetadataSearch(
        context,
        /** @type {!FakeEntry} */ (entry),
        searchType);
  }
  // Local fetch or search.
  return DirectoryContents.createForDirectory(
      context, /** @type {!DirectoryEntry} */ (entry));
};

/**
 * Gets the last search query.
 * @return {string} the last search query.
 */
DirectoryModel.prototype.getLastSearchQuery = function() {
  return this.lastSearchQuery_;
};

/**
 * Clears the last search query with the empty string.
 */
DirectoryModel.prototype.clearLastSearchQuery = function() {
  this.lastSearchQuery_ = '';
};

/**
 * Performs search and displays results. The search type is dependent on the
 * current directory. If we are currently on drive, server side content search
 * over drive mount point. If the current directory is not on the drive, file
 * name search over current directory will be performed.
 *
 * @param {string} query Query that will be searched for.
 * @param {function(Event)} onSearchRescan Function that will be called when the
 *     search directory is rescanned (i.e. search results are displayed).
 * @param {function()} onClearSearch Function to be called when search state
 *     gets cleared.
 * TODO(olege): Change callbacks to events.
 */
DirectoryModel.prototype.search = function(query,
                                           onSearchRescan,
                                           onClearSearch) {
  this.lastSearchQuery_ = query;
  this.clearSearch_();
  var currentDirEntry = this.getCurrentDirEntry();
  if (!currentDirEntry) {
    // Not yet initialized. Do nothing.
    return;
  }

  this.changeDirectorySequence_++;
  this.directoryChangeQueue_.run(function(sequence, callback) {
    if (this.changeDirectorySequence_ !== sequence) {
      callback();
      return;
    }

    if (!(query || '').trimLeft()) {
      if (this.isSearching()) {
        var newDirContents = this.createDirectoryContents_(
            this.currentFileListContext_,
            assert(currentDirEntry));
        this.clearAndScan_(newDirContents,
                           callback);
      } else {
        callback();
      }
      return;
    }

    var newDirContents = this.createDirectoryContents_(
        this.currentFileListContext_, assert(currentDirEntry), query);
    if (!newDirContents) {
      callback();
      return;
    }

    this.onSearchCompleted_ = onSearchRescan;
    this.onClearSearch_ = onClearSearch;
    this.addEventListener('scan-completed', this.onSearchCompleted_);
    this.clearAndScan_(newDirContents,
                       callback);
  }.bind(this, this.changeDirectorySequence_));
};

/**
 * In case the search was active, remove listeners and send notifications on
 * its canceling.
 * @private
 */
DirectoryModel.prototype.clearSearch_ = function() {
  if (!this.isSearching())
    return;

  if (this.onSearchCompleted_) {
    this.removeEventListener('scan-completed', this.onSearchCompleted_);
    this.onSearchCompleted_ = null;
  }

  if (this.onClearSearch_) {
    this.onClearSearch_();
    this.onClearSearch_ = null;
  }
};

/**
 * DOMError type for crostini connection failure.
 * @const {string}
 */
DirectoryModel.CROSTINI_CONNECT_ERR = 'CrostiniConnectErr';
