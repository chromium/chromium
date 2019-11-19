// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

importer.MediaImportHandler = importer.MediaImportHandler || {};
importer.MediaImportHandler.ImportTask =
    importer.MediaImportHandler.ImportTask || {};

/**
 * Handler for importing media from removable devices into the user's Drive.
 *
 * @implements {importer.ImportRunner}
 * @implements {importer.MediaImportHandler}
 */
importer.MediaImportHandlerImpl = class {
  /**
   * @param {!ProgressCenter} progressCenter
   * @param {!importer.HistoryLoader} historyLoader
   * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
   * @param {!DriveSyncHandler} driveSyncHandler
   */
  constructor(
      progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {
    /** @private {!ProgressCenter} */
    this.progressCenter_ = progressCenter;

    /** @private {!importer.HistoryLoader} */
    this.historyLoader_ = historyLoader;

    /** @private {!importer.TaskQueue} */
    this.queue_ = new importer.TaskQueueImpl();

    // Prevent the system from sleeping while imports are active.
    this.queue_.setActiveCallback(() => {
      chrome.power.requestKeepAwake('system');
    });
    this.queue_.setIdleCallback(() => {
      chrome.power.releaseKeepAwake();
    });

    /** @private {number} */
    this.nextTaskId_ = 0;

    /** @private {!importer.DispositionChecker.CheckerFunction} */
    this.getDisposition_ = dispositionChecker;

    /** @private {!DriveSyncHandler} */
    this.driveSyncHandler_ = driveSyncHandler;

    if (this.driveSyncHandler_.getCompletedEventName() !== 'completed') {
      throw new Error('invalid drive sync completed event name');
    }
  }

  /** @override */
  importFromScanResult(scanResult, destination, directoryPromise) {
    const task = new importer.MediaImportHandler.ImportTaskImpl(
        this.generateTaskId_(), this.historyLoader_, scanResult,
        directoryPromise, destination, this.getDisposition_);

    task.addObserver(this.onTaskProgress_.bind(this, task));

    // Schedule the task when it is initialized.
    scanResult.whenFinal()
        .then(task.initialize_.bind(task))
        .then(function(scanResult, task) {
          task.importEntries = scanResult.getFileEntries();
          this.queue_.queueTask(task);
        }.bind(this, scanResult, task));

    return task;
  }

  /**
   * Generates unique task IDs.
   * @private
   */
  generateTaskId_() {
    return 'media-import' + this.nextTaskId_++;
  }

  /**
   * Sends updates to the ProgressCenter when an import is happening.
   *
   * @param {!importer.MediaImportHandler.ImportTaskImpl} task
   * @param {string} updateType
   * @private
   */
  onTaskProgress_(task, updateType) {
    const UpdateType = importer.TaskQueue.UpdateType;

    let item = this.progressCenter_.getItemById(task.taskId);
    if (!item) {
      item = new ProgressCenterItem();
      item.id = task.taskId;
      // TODO(kenobi): Might need a different progress item type here.
      item.type = ProgressItemType.COPY;
      item.progressMax = task.totalBytes;
      item.cancelCallback = () => {
        task.requestCancel();
      };
    }

    switch (updateType) {
      case UpdateType.PROGRESS:
        item.message =
            strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
        item.progressValue = task.processedBytes;
        item.state = ProgressItemState.PROGRESSING;
        break;

      case UpdateType.COMPLETE:
        // Remove the event handler that gets attached for retries.
        this.driveSyncHandler_.removeEventListener(
            this.driveSyncHandler_.getCompletedEventName(),
            task.driveListener_);

        if (task.failedEntries.length > 0 &&
            task.failedEntries.length < task.importEntries.length) {
          // If there are failed entries but at least one entry succeeded on the
          // last run, there is a chance of more succeeding when retrying.
          this.retryTaskFailedEntries_(task);
        } else {
          // Otherwise, finish progress bar.
          // Display all errors.
          let errorIdCounter = 0;
          task.failedEntries.forEach(entry => {
            const errorItem = new ProgressCenterItem();
            errorItem.id = task.taskId_ + '-' + (errorIdCounter++);
            errorItem.type = ProgressItemType.COPY;
            errorItem.quiet = true;
            errorItem.state = ProgressItemState.ERROR;
            errorItem.message = strf('CLOUD_IMPORT_ERROR_ITEM', entry.name);
            this.progressCenter_.updateItem(item);
          });

          // Complete progress bar.
          item.message = '';
          item.progressValue = item.progressMax;
          item.state = ProgressItemState.COMPLETED;

          task.sendImportStats_();
        }

        break;

      case UpdateType.CANCELED:
        item.message = '';
        item.state = ProgressItemState.CANCELED;
        break;
    }

    this.progressCenter_.updateItem(item);
  }

  /**
   * Restarts a task with failed entries.
   * @param {!importer.MediaImportHandler.ImportTaskImpl} task
   */
  retryTaskFailedEntries_(task) {
    // Reset the entry lists.
    task.importEntries = task.failedEntries;
    task.failedEntries = [];

    // When Drive is done syncing, it will mark the synced files as eligible
    // for cache eviction, enabling files that failed to be imported because
    // of not having enough local space to succeed on retry.
    task.driveListener_ = () => {
      this.queue_.queueTask(task);
    };
    this.driveSyncHandler_.addEventListener(
        this.driveSyncHandler_.getCompletedEventName(), task.driveListener_);
  }
};

/**
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @implements {importer.MediaImportHandler.ImportTask}
 */
importer.MediaImportHandler.ImportTaskImpl =
    class extends importer.TaskQueue.BaseTaskImpl {
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
      dispositionChecker) {
    super(taskId);

    /** @protected {string} */
    this.taskId_ = taskId;

    /** @private {!importer.Destination} */
    this.destination_ = destination;

    /** @private {!Promise<!DirectoryEntry>} */
    this.directoryPromise_ = directoryPromise;

    /** @private {!importer.ScanResult} */
    this.scanResult_ = scanResult;

    /** @private {!importer.HistoryLoader} */
    this.historyLoader_ = historyLoader;

    /** @private {number} */
    this.totalBytes_ = 0;

    /** @private {number} */
    this.processedBytes_ = 0;

    /**
     * Number of duplicate files found by the content hash check.
     * @private {number}
     */
    this.duplicateFilesCount_ = 0;

    /** @private {number} */
    this.remainingFilesCount_ = 0;

    /** @private {?function()} */
    this.cancelCallback_ = null;

    /** @private {boolean} Indicates whether this task was canceled. */
    this.canceled_ = false;

    /** @private {number} */
    this.errorCount_ = 0;

    /** @private {!importer.DispositionChecker.CheckerFunction} */
    this.getDisposition_ = dispositionChecker;

    /**
     * The entries to be imported.
     * @private {!Array<!FileEntry>}
     */
    this.importEntries_ = [];

    /**
     * The failed entries.
     * @private {!Array<!FileEntry>}
     */
    this.failedEntries_ = [];

    /**
     * A placeholder for identifying the appropriate retry function for a given
     * task.
     * @private {EventListener|function(!Event)}
     */
    this.driveListener_ = null;
  }

  /** @return {number} Number of imported bytes */
  get processedBytes() {
    return this.processedBytes_;
  }

  /** @return {number} Total number of bytes to import */
  get totalBytes() {
    return this.totalBytes_;
  }

  /** @return {number} Number of files left to import */
  get remainingFilesCount() {
    return this.remainingFilesCount_;
  }

  /** @return {!Array<!FileEntry>} The files to be imported */
  get importEntries() {
    return this.importEntries_;
  }

  /** @param {!Array<!FileEntry>} entries The files to be imported */
  set importEntries(entries) {
    this.importEntries_ = entries.slice();
  }

  /** @return {!Array<!FileEntry>} The files that couldn't be imported */
  get failedEntries() {
    return this.failedEntries_;
  }

  /** @param {!Array<!FileEntry>} entries The files that couldn't be imported */
  set failedEntries(entries) {
    this.failedEntries_ = entries.slice();
  }

  /** @override */
  run() {
    // Wait for the scan to finish, then get the destination entry, then start
    // the import.
    this.importScanEntries_()
        .then(this.markDuplicatesImported_.bind(this))
        .then(this.onSuccess_.bind(this))
        .catch(importer.getLogger().catcher('import-task-run'));
  }

  /**
   * Request cancellation of this task.  An update will be sent to observers
   * once the task is actually cancelled.
   */
  requestCancel() {
    this.canceled_ = true;
    setTimeout(() => {
      this.notify(importer.TaskQueue.UpdateType.CANCELED);
      this.sendImportStats_();
    });
    if (this.cancelCallback_) {
      // Reset the callback before calling it, as the callback might do anything
      // (including calling #requestCancel again).
      const cancelCallback = this.cancelCallback_;
      this.cancelCallback_ = null;
      cancelCallback();
    }
  }

  /** @private */
  initialize_() {
    const stats = this.scanResult_.getStatistics();
    this.remainingFilesCount_ = stats.newFileCount;
    this.totalBytes_ = stats.sizeBytes;

    metrics.recordBoolean('MediaImport.Started', true);
  }

  /**
   * Initiates an import to the given location.  This should only be called once
   * the scan result indicates that it is ready.
   *
   * @private
   */
  importScanEntries_() {
    const resolver = new importer.Resolver();
    this.directoryPromise_.then(destinationDirectory => {
      AsyncUtil.forEach(
          this.importEntries_, this.importOne_.bind(this, destinationDirectory),
          resolver.resolve);
    });
    return resolver.promise;
  }

  /**
   * Marks all duplicate entries as imported.
   *
   * @private
   */
  markDuplicatesImported_() {
    this.historyLoader_.getHistory()
        .then(
            /**
             * @param {!importer.ImportHistory} history
             */
            history => {
              this.scanResult_.getDuplicateFileEntries().forEach(
                  /**
                   * @param {!FileEntry} entry
                   * @this {importer.MediaImportHandler.ImportTask}
                   */
                  entry => {
                    history.markImported(entry, this.destination_);
                  });
            })
        .catch(importer.getLogger().catcher('import-task-mark-dupes-imported'));
  }

  /**
   * Imports one file. If the file already exist in Drive, marks as imported.
   *
   * @param {!DirectoryEntry} destinationDirectory
   * @param {function()} completionCallback Called after this operation is
   *     complete.
   * @param {!FileEntry} entry The entry to import.
   * @param {number} index The index of the entry.
   * @param {Array<!FileEntry>} all All the entries.
   * @private
   */
  importOne_(destinationDirectory, completionCallback, entry, index, all) {
    if (this.canceled_) {
      metrics.recordBoolean('MediaImport.Cancelled', true);
      return;
    }

    this.getDisposition_(
            entry, importer.Destination.GOOGLE_DRIVE, importer.ScanMode.CONTENT)
        .then(/**
               * @param {!importer.Disposition} disposition The disposition
               *     of the entry. Either some sort of dupe, or an original.
               */
              disposition => {
                if (disposition === importer.Disposition.ORIGINAL) {
                  return this.copy_(entry, destinationDirectory);
                }
                this.duplicateFilesCount_++;
                this.markAsImported_(entry);
              })
        // Regardless of the result of this copy, push on to the next file.
        .then(completionCallback)
        .catch(/** @param {*} error */
               error => {
                 importer.getLogger().catcher('import-task-import-one')(error);
                 // TODO(oka): Retry copies only when failed due to
                 // insufficient disk space. crbug.com/788692.
                 this.failedEntries_.push(entry);
                 completionCallback();
               });
  }

  /**
   * @param {!FileEntry} entry The file to copy.
   * @param {!DirectoryEntry} destinationDirectory The destination directory.
   * @return {!Promise<FileEntry>} Resolves to the destination file when the
   *     copy is complete.  The FileEntry may be null if the import was
   *     cancelled.  The promise will reject if an error occurs.
   * @private
   */
  copy_(entry, destinationDirectory) {
    // A count of the current number of processed bytes for this entry.
    let currentBytes = 0;

    const resolver = new importer.Resolver();

    /**
     * Updates the task when the copy code reports progress.
     * @param {string} sourceUrl
     * @param {number} processedBytes
     * @this {importer.MediaImportHandler.ImportTask}
     */
    const onProgress = (sourceUrl, processedBytes) => {
      // Update the running total, then send a progress update.
      this.processedBytes_ -= currentBytes;
      this.processedBytes_ += processedBytes;
      currentBytes = processedBytes;
      this.notify(importer.TaskQueue.UpdateType.PROGRESS);
    };

    /**
     * Updates the task when the new file has been created.
     * @param {string} sourceUrl
     * @param {Entry} destinationEntry
     * @this {importer.MediaImportHandler.ImportTask}
     */
    const onEntryChanged = (sourceUrl, destinationEntry) => {
      this.processedBytes_ -= currentBytes;
      this.processedBytes_ += entry.size;
      destinationEntry.size = entry.size;
      this.notify(
          /** @type {importer.TaskQueue.UpdateType} */
          (importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED), {
            sourceUrl: sourceUrl,
            destination: destinationEntry,
          });
      this.notify(importer.TaskQueue.UpdateType.PROGRESS);
    };

    /**
     * @param {Entry} destinationEntry The new destination entry.
     * @this {importer.MediaImportHandler.ImportTask}
     */
    const onComplete = destinationEntry => {
      this.cancelCallback_ = null;
      this.markAsCopied_(entry, /** @type {!FileEntry} */ (destinationEntry));
      this.notify(importer.TaskQueue.UpdateType.PROGRESS);
      resolver.resolve(destinationEntry);
    };

    /** @this {importer.MediaImportHandler.ImportTask} */
    const onError = error => {
      this.cancelCallback_ = null;
      if (error.name === util.FileError.ABORT_ERR) {
        // Task cancellations result in the error callback being triggered with
        // an ABORT_ERR, but we want to ignore these errors.
        this.notify(importer.TaskQueue.UpdateType.PROGRESS);
        resolver.resolve(null);
      } else {
        this.errorCount_++;
        this.notify(importer.TaskQueue.UpdateType.ERROR);
        resolver.reject(error);
      }
    };

    fileOperationUtil.deduplicatePath(destinationDirectory, entry.name)
        .then(
            /**
             * Performs the copy using the given deduped filename.
             * @param {string} destinationFilename
             */
            destinationFilename => {
              this.cancelCallback_ = fileOperationUtil.copyTo(
                  entry, destinationDirectory, destinationFilename,
                  onEntryChanged, onProgress, onComplete, onError);
            },
            resolver.reject)
        .catch(importer.getLogger().catcher('import-task-copy'));

    return resolver.promise;
  }

  /**
   * @param {!FileEntry} entry
   * @param {!FileEntry} destinationEntry
   */
  markAsCopied_(entry, destinationEntry) {
    this.remainingFilesCount_--;
    this.historyLoader_.getHistory()
        .then(history => {
          history.markCopied(
              entry, this.destination_, destinationEntry.toURL());
        })
        .catch(importer.getLogger().catcher('import-task-mark-as-copied'));
  }

  /**
   * @param {!FileEntry} entry
   * @private
   */
  markAsImported_(entry) {
    this.remainingFilesCount_--;
    this.historyLoader_.getHistory()
        .then(
            /** @param {!importer.ImportHistory} history */
            history => {
              history.markImported(entry, this.destination_);
            })
        .catch(importer.getLogger().catcher('import-task-mark-as-imported'));
  }

  /** @private */
  onSuccess_() {
    this.notify(importer.TaskQueue.UpdateType.COMPLETE);
  }

  /**
   * Sends import statistics to analytics.
   */
  sendImportStats_() {
    const scanStats = this.scanResult_.getStatistics();

    metrics.recordMediumCount(
        'MediaImport.ImportMB',
        Math.floor(this.processedBytes_ / (1024 * 1024)));

    metrics.recordMediumCount(
        'MediaImport.ImportCount',
        // Subtract the remaining files, in case the task was cancelled.
        scanStats.newFileCount - this.remainingFilesCount_);

    if (this.errorCount_ > 0) {
      metrics.recordMediumCount('MediaImport.ErrorCount', this.errorCount_);
    }

    // Finally we want to report on the number of duplicates
    // that were identified during scanning.
    let totalDeduped = 0;
    // The scan is run without content duplicate check.
    // Instead, report the number of duplicated files found at import.
    assert(scanStats.duplicates[importer.Disposition.CONTENT_DUPLICATE] === 0);
    scanStats.duplicates[importer.Disposition.CONTENT_DUPLICATE] =
        this.duplicateFilesCount_;

    Object.keys(scanStats.duplicates).forEach(disposition => {
      const count = scanStats.duplicates[
          /** @type {!importer.Disposition} */ (disposition)];
      totalDeduped += count;
    }, this);

    metrics.recordMediumCount('MediaImport.Duplicates', totalDeduped);
  }
};

/**
 * Update types that are specific to ImportTask.  Clients can add Observers to
 * ImportTask to listen for these kinds of updates.
 * @enum {string}
 */
importer.MediaImportHandler.ImportTask.UpdateType = {
  ENTRY_CHANGED: 'ENTRY_CHANGED'
};
