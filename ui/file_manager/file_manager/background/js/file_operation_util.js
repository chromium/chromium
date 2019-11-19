// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utilities for file operations.
 */
const fileOperationUtil = {};

/**
 * Resolves a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param {DirectoryEntry} root The root of the filesystem to search.
 * @param {string} path The path to be resolved.
 * @return {Promise} Promise fulfilled with the resolved entry, or rejected with
 *     FileError.
 */
fileOperationUtil.resolvePath = (root, path) => {
  if (path === '' || path === '/') {
    return Promise.resolve(root);
  }
  return new Promise(root.getFile.bind(root, path, {create: false}))
      .catch(error => {
        if (error.name === util.FileError.TYPE_MISMATCH_ERR) {
          // Bah.  It's a directory, ask again.
          return new Promise(
              root.getDirectory.bind(root, path, {create: false}));
        } else {
          return Promise.reject(error);
        }
      });
};

/**
 * Checks if an entry exists at |relativePath| in |dirEntry|.
 * If exists, tries to deduplicate the path by inserting parenthesized number,
 * such as " (1)", before the extension. If it still exists, tries the
 * deduplication again by increasing the number.
 * For example, suppose "file.txt" is given, "file.txt", "file (1).txt",
 * "file (2).txt", ... will be tried.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)=} opt_successCallback Callback run with the
 *     deduplicated path on success.
 * @param {function(fileOperationUtil.Error)=} opt_errorCallback Callback run
 *     on error.
 * @return {Promise} Promise fulfilled with available path.
 */
fileOperationUtil.deduplicatePath =
    (dirEntry, relativePath, opt_successCallback, opt_errorCallback) => {
      // Crack the path into three part. The parenthesized number (if exists)
      // will be replaced by incremented number for retry. For example, suppose
      // |relativePath| is "file (10).txt", the second check path will be
      // "file (11).txt".
      const match = /^(.*?)(?: \((\d+)\))?(\.[^.]*?)?$/.exec(relativePath);
      const prefix = match[1];
      const ext = match[3] || '';

      // Check to see if the target exists.
      const resolvePath = (trialPath, copyNumber) => {
        return fileOperationUtil.resolvePath(dirEntry, trialPath)
            .then(
                () => {
                  const newTrialPath = prefix + ' (' + copyNumber + ')' + ext;
                  return resolvePath(newTrialPath, copyNumber + 1);
                },
                error => {
                  // We expect to be unable to resolve the target file, since
                  // we're going to create it during the copy.  However, if the
                  // resolve fails with anything other than NOT_FOUND, that's
                  // trouble.
                  if (error.name === util.FileError.NOT_FOUND_ERR) {
                    return trialPath;
                  } else {
                    return Promise.reject(error);
                  }
                });
      };

      const promise = resolvePath(relativePath, 1).catch(error => {
        if (error instanceof Error) {
          return Promise.reject(error);
        }
        return Promise.reject(new fileOperationUtil.Error(
            util.FileOperationErrorType.FILESYSTEM_ERROR, error));
      });
      if (opt_successCallback) {
        promise.then(opt_successCallback, opt_errorCallback);
      }
      return promise;
    };

/**
 * Traverses files/subdirectories of the given entry, and returns them.
 * In addition, this method annotate the size of each entry. The result will
 * include the entry itself.
 *
 * @param {Entry} entry The root Entry for traversing.
 * @param {function(Array<Entry>)} successCallback Called when the traverse
 *     is successfully done with the array of the entries.
 * @param {function(DOMError)} errorCallback Called on error with the first
 *     occurred error (i.e. following errors will just be discarded).
 * @private
 */
fileOperationUtil.resolveRecursively_ =
    (entry, successCallback, errorCallback) => {
      const result = [];
      let error = null;
      let numRunningTasks = 0;

      const maybeInvokeCallback = () => {
        // If there still remain some running tasks, wait their finishing.
        if (numRunningTasks > 0) {
          return;
        }

        if (error) {
          errorCallback(error);
        } else {
          successCallback(result);
        }
      };

      // The error handling can be shared.
      const onError = fileError => {
        // If this is the first error, remember it.
        if (!error) {
          error = fileError;
        }
        --numRunningTasks;
        maybeInvokeCallback();
      };

      // If this is recursing through a very large directory structure (1000's
      // of files), this will flood the browser process with IPC because each
      // getEntryMetadata call causes an IPC to the browser. This can cause the
      // browser to become unresponsive, and may even be killed by a watchdog
      // timer. To prevent this, limit the number of concurrent getEntryMetadata
      // calls to a reasonable level.
      const metadataFetchQueue = new AsyncUtil.ConcurrentQueue(64);

      const process = entry => {
        numRunningTasks++;
        result.push(entry);
        if (entry.isDirectory) {
          // The size of a directory is 1 bytes here, so that the progress bar
          // will work smoother.
          // TODO(hidehiko): Remove this hack.
          entry.size = 1;

          // Recursively traverse children.
          const reader = entry.createReader();
          reader.readEntries(function processSubEntries(subEntries) {
            if (error || subEntries.length == 0) {
              // If an error is found already, or this is the completion
              // callback, then finish the process.
              --numRunningTasks;
              maybeInvokeCallback();
              return;
            }

            for (let i = 0; i < subEntries.length; i++) {
              process(subEntries[i]);
            }

            // Continue to read remaining children.
            reader.readEntries(processSubEntries, onError);
          }, onError);
        } else {
          // For a file, annotate the file size.
          metadataFetchQueue.run(done => {
            metadataProxy.getEntryMetadata(entry)
                .then(
                    metadata => {
                      entry.size = metadata.size;
                      --numRunningTasks;
                      maybeInvokeCallback();
                    },
                    onError)
                .finally(done);
          });
        }
      };

      process(entry);
    };

/**
 * Recursively gathers files from the given entry, resolving with
 * the complete list of files when traversal is complete.
 *
 * <p>For real-time (as you scan) results use {@code findFilesRecursively}.
 *
 * @param {!DirectoryEntry} entry The DirectoryEntry to scan.
 * @return {!Promise<!Array<!Entry>>} Resolves when scanning is complete.
 */
fileOperationUtil.gatherEntriesRecursively = entry => {
  /** @type {!Array<!Entry>} */
  const gatheredFiles = [];

  return fileOperationUtil
      .findEntriesRecursively(
          entry,
          /** @param {!Entry} entry */
          entry => {
            gatheredFiles.push(entry);
          })
      .then(() => {
        return gatheredFiles;
      });
};

/**
 * Recursively discovers files from the given entry, emitting individual
 * results as they are found to {@code onResultCallback}.
 *
 * <p>For results gathered up in a tidy bundle, use
 * {@code gatherFilesRecursively}.
 *
 * @param {!DirectoryEntry} entry The DirectoryEntry to scan.
 * @param {function(!FileEntry)} onResultCallback called when
 *     a {@code FileEntry} is discovered.
 * @return {!Promise} Resolves when scanning is complete.
 */
fileOperationUtil.findFilesRecursively = (entry, onResultCallback) => {
  return fileOperationUtil.findEntriesRecursively(
      entry,
      /** @param {!Entry} entry */
      entry => {
        if (entry.isFile) {
          onResultCallback(/** @type {!FileEntry} */ (entry));
        }
      });
};

/**
 * Recursively discovers files and directories beneath the given entry,
 * emitting individual results as they are found to {@code onResultCallback}.
 *
 * <p>For results gathered up in a tidy bundle, use
 * {@code gatherEntriesRecursively}.
 *
 * @param {!DirectoryEntry} entry The DirectoryEntry to scan.
 * @param {function(!Entry)} onResultCallback called when
 *     an {@code Entry} is discovered.
 * @return {!Promise} Resolves when scanning is complete.
 */
fileOperationUtil.findEntriesRecursively = (entry, onResultCallback) => {
  return new Promise((resolve, reject) => {
    let numRunningTasks = 0;
    let scanError = null;

    /**
     * @param  {*=} opt_error If defined immediately
     *     terminates scanning.
     */
    const maybeSettlePromise = opt_error => {
      scanError = opt_error;

      if (scanError) {
        // Closure compiler currently requires an argument to reject.
        reject(undefined);
        return;
      }

      // If there still remain some running tasks, wait their finishing.
      if (numRunningTasks === 0) {
        // Closure compiler currently requires an argument to resolve.
        resolve(undefined);
      }
    };

    /** @param {!Entry} entry */
    const processEntry = entry => {
      // All scanning stops when an error is encountered.
      if (scanError) {
        return;
      }

      onResultCallback(entry);
      if (entry.isDirectory) {
        processDirectory(/** @type {!DirectoryEntry} */ (entry));
      }
    };

    /** @param {!DirectoryEntry} directory */
    var processDirectory = directory => {
      // All scanning stops when an error is encountered.
      if (scanError) {
        return;
      }

      numRunningTasks++;

      // Recursively traverse children.
      // reader.readEntries chunksResults resulting in the need
      // for us to call it multiple times.
      const reader = directory.createReader();
      reader.readEntries(function processSubEntries(subEntries) {
        if (subEntries.length === 0) {
          // If an error is found already, or this is the completion
          // callback, then finish the process.
          --numRunningTasks;
          maybeSettlePromise();
          return;
        }

        subEntries.forEach(processEntry);

        // Continue to read remaining children.
        reader.readEntries(processSubEntries, maybeSettlePromise);
      }, maybeSettlePromise);
    };

    processEntry(entry);
  });
};

/**
 * Calls {@code callback} for each child entry of {@code directory}.
 *
 * @param {!DirectoryEntry} directory
 * @param {function(!Entry)} callback
 * @return {!Promise} Resolves when listing is complete.
 */
fileOperationUtil.listEntries = (directory, callback) => {
  return new Promise((resolve, reject) => {
    const reader = directory.createReader();

    const readEntries = () => {
      reader.readEntries(
          /** @param {!Array<!Entry>} entries */
          entries => {
            if (entries.length === 0) {
              resolve(undefined);
              return;
            }
            entries.forEach(callback);
            readEntries();
          },
          reject);
    };

    readEntries();
  });
};

/**
 * Copies source to parent with the name newName recursively.
 * This should work very similar to FileSystem API's copyTo. The difference is;
 * - The progress callback is supported.
 * - The cancellation is supported.
 *
 * @param {!Entry} source The entry to be copied.
 * @param {!DirectoryEntry} parent The entry of the destination directory.
 * @param {string} newName The name of copied file.
 * @param {function(string, Entry)} entryChangedCallback
 *     Callback invoked when an entry is created with the source URL and
 *     the destination Entry.
 * @param {function(string, number)} progressCallback Callback invoked
 *     periodically during the copying. It takes the source URL and the
 *     processed bytes of it.
 * @param {function(Entry)} successCallback Callback invoked when the copy
 *     is successfully done with the Entry of the created entry.
 * @param {function(DOMError)} errorCallback Callback invoked when an error
 *     is found.
 * @return {function()} Callback to cancel the current file copy operation.
 *     When the cancel is done, errorCallback will be called. The returned
 *     callback must not be called more than once.
 */
fileOperationUtil.copyTo =
    (source, parent, newName, entryChangedCallback, progressCallback,
     successCallback, errorCallback) => {
      /** @type {number|undefined} */
      let copyId;
      const pendingCallbacks = [];

      // Makes the callback called in order they were invoked.
      const callbackQueue = new AsyncUtil.Queue();

      const onCopyProgress = (progressCopyId, status) => {
        callbackQueue.run(callback => {
          if (copyId === null) {
            // If the copyId is not yet available, wait for it.
            pendingCallbacks.push(
                onCopyProgress.bind(null, progressCopyId, status));
            callback();
            return;
          }

          // This is not what we're interested in.
          if (progressCopyId != copyId) {
            callback();
            return;
          }

          switch (status.type) {
            case 'begin_copy_entry':
              callback();
              break;

            case 'end_copy_entry':
              // TODO(mtomasz): Convert URL to Entry in custom bindings.
              (source.isFile ? parent.getFile : parent.getDirectory)
                  .call(
                      parent, newName, null,
                      entry => {
                        entryChangedCallback(status.sourceUrl, entry);
                        callback();
                      },
                      () => {
                        entryChangedCallback(status.sourceUrl, null);
                        callback();
                      });
              break;

            case 'progress':
              progressCallback(status.sourceUrl, status.size);
              callback();
              break;

            case 'success':
              chrome.fileManagerPrivate.onCopyProgress.removeListener(
                  onCopyProgress);
              // TODO(mtomasz): Convert URL to Entry in custom bindings.
              util.URLsToEntries(
                  [status.destinationUrl], destinationEntries => {
                    successCallback(destinationEntries[0] || null);
                    callback();
                  });
              break;

            case 'error':
              console.error(
                  'copy failed. sourceUrl: ' + source.toURL() +
                  ' error: ' + status.error);
              chrome.fileManagerPrivate.onCopyProgress.removeListener(
                  onCopyProgress);
              errorCallback(util.createDOMError(status.error));
              callback();
              break;

            default:
              // Found unknown state. Cancel the task, and return an error.
              console.error('Unknown progress type: ' + status.type);
              chrome.fileManagerPrivate.onCopyProgress.removeListener(
                  onCopyProgress);
              chrome.fileManagerPrivate.cancelCopy(
                  assert(copyId), util.checkAPIError);
              errorCallback(
                  util.createDOMError(util.FileError.INVALID_STATE_ERR));
              callback();
          }
        });
      };

      // Register the listener before calling startCopy. Otherwise some events
      // would be lost.
      chrome.fileManagerPrivate.onCopyProgress.addListener(onCopyProgress);

      // Then starts the copy.
      chrome.fileManagerPrivate.startCopy(
          source, parent, newName, startCopyId => {
            // last error contains the FileError code on error.
            if (chrome.runtime.lastError) {
              // Unsubscribe the progress listener.
              chrome.fileManagerPrivate.onCopyProgress.removeListener(
                  onCopyProgress);
              errorCallback(
                  util.createDOMError(chrome.runtime.lastError.message || ''));
              return;
            }

            copyId = startCopyId;
            for (let i = 0; i < pendingCallbacks.length; i++) {
              pendingCallbacks[i]();
            }
          });

      return () => {
        // If copyId is not yet available, wait for it.
        if (copyId === undefined) {
          pendingCallbacks.push(() => {
            chrome.fileManagerPrivate.cancelCopy(
                assert(copyId), util.checkAPIError);
          });
          return;
        }

        chrome.fileManagerPrivate.cancelCopy(copyId, util.checkAPIError);
      };
    };

/**
 * Thin wrapper of chrome.fileManagerPrivate.zipSelection to adapt its
 * interface similar to copyTo().
 *
 * @param {!Array<!Entry>} sources The array of entries to be archived.
 * @param {!DirectoryEntry} parent The entry of the destination directory.
 * @param {string} newName The name of the archive to be created.
 * @param {function(FileEntry)} successCallback Callback invoked when the
 *     operation is successfully done with the entry of the created archive.
 * @param {function(DOMError)} errorCallback Callback invoked when an error
 *     is found.
 */
fileOperationUtil.zipSelection =
    (sources, parent, newName, successCallback, errorCallback) => {
      chrome.fileManagerPrivate.zipSelection(
          sources, parent, newName, success => {
            if (!success) {
              // Failed to create a zip archive.
              errorCallback(
                  util.createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
              return;
            }

            // Returns the created entry via callback.
            parent.getFile(
                newName, {create: false}, successCallback, errorCallback);
          });
    };

/**
 * A record of a queued copy operation.
 *
 * Multiple copy operations may be queued at any given time.  Additional
 * Tasks may be added while the queue is being serviced.  Though a
 * cancel operation cancels everything in the queue.
 */
fileOperationUtil.Task = class {
  /**
   * @param {string} taskId A unique ID for identifying this task.
   * @param {util.FileOperationType} operationType The type of this operation.
   * @param {Array<Entry>} sourceEntries Array of source entries.
   * @param {DirectoryEntry} targetDirEntry Target directory.
   */
  constructor(taskId, operationType, sourceEntries, targetDirEntry) {
    /** @type {string} */
    this.taskId = taskId;

    /** @type {util.FileOperationType} */
    this.operationType = operationType;

    /** @type {Array<Entry>} */
    this.sourceEntries = sourceEntries;

    /** @type {DirectoryEntry} */
    this.targetDirEntry = targetDirEntry;

    /**
     * An array of map from url to Entry being processed.
     * @type {Array<Object<Entry>>}
     */
    this.processingEntries = null;

    /**
     * Total number of bytes to be processed. Filled in initialize().
     * Use 1 as an initial value to indicate that the task is not completed.
     * @type {number}
     */
    this.totalBytes = 1;

    /**
     * Total number of already processed bytes. Updated periodically.
     * @type {number}
     */
    this.processedBytes = 0;

    /**
     * Total number of remaining items. Updated periodically.
     * @type {number}
     */
    this.numRemainingItems = this.sourceEntries.length;

    /**
     * Index of the progressing entry in sourceEntries.
     * @private {number}
     */
    this.processingSourceIndex_ = 0;

    /**
     * Set to true when cancel is requested.
     * @private {boolean}
     */
    this.cancelRequested_ = false;

    /**
     * Callback to cancel the running process.
     * @private {?function()}
     */
    this.cancelCallback_ = null;

    // TODO(hidehiko): After we support recursive copy, we don't need this.
    // If directory already exists, we try to make a copy named 'dir (X)',
    // where X is a number. When we do this, all subsequent copies from
    // inside the subtree should be mapped to the new directory name.
    // For example, if 'dir' was copied as 'dir (1)', then 'dir/file.txt' should
    // become 'dir (1)/file.txt'.
    this.renamedDirectories_ = [];
  }


  /**
   * @param {function()} callback When entries resolved.
   */
  initialize(callback) {}

  /**
   * Requests cancellation of this task.
   * When the cancellation is done, it is notified via callbacks of run().
   */
  requestCancel() {
    this.cancelRequested_ = true;
    if (this.cancelCallback_) {
      const callback = this.cancelCallback_;
      this.cancelCallback_ = null;
      callback();
    }
  }

  /**
   * Runs the task. Sub classes must implement this method.
   *
   * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
   *     Callback invoked when an entry is changed.
   * @param {function()} progressCallback Callback invoked periodically during
   *     the operation.
   * @param {function()} successCallback Callback run on success.
   * @param {function(fileOperationUtil.Error)} errorCallback Callback run on
   *     error.
   */
  run(entryChangedCallback, progressCallback, successCallback, errorCallback) {}

  /**
   * Get states of the task.
   * TODO(hirono): Removes this method and sets a task to progress events.
   * @return {Object} Status object.
   */
  getStatus() {
    const processingEntry = this.sourceEntries[this.processingSourceIndex_];
    return {
      operationType: this.operationType,
      numRemainingItems: this.numRemainingItems,
      totalBytes: this.totalBytes,
      processedBytes: this.processedBytes,
      processingEntryName: processingEntry ? processingEntry.name : '',
      targetDirEntryName: this.targetDirEntry.name
    };
  }

  /**
   * Obtains the number of total processed bytes.
   * @return {number} Number of total processed bytes.
   * @private
   */
  calcProcessedBytes_() {
    let bytes = 0;
    for (let i = 0; i < this.processingSourceIndex_ + 1; i++) {
      const entryMap = this.processingEntries[i];
      if (!entryMap) {
        break;
      }
      for (const name in entryMap) {
        bytes += i < this.processingSourceIndex_ ?
            entryMap[name].size :
            entryMap[name].processedBytes;
      }
    }
    return bytes;
  }

  /**
   * Obtains the number of remaining items.
   * @return {number} Number of remaining items.
   * @private
   */
  calcNumRemainingItems_() {
    let numRemainingItems = 0;

    let resolvedEntryMap;
    if (this.processingEntries && this.processingEntries.length > 0) {
      resolvedEntryMap = this.processingEntries[this.processingSourceIndex_];
    }

    if (resolvedEntryMap) {
      for (const key in resolvedEntryMap) {
        if (resolvedEntryMap.hasOwnProperty(key) &&
            resolvedEntryMap[key].processedBytes === 0) {
          numRemainingItems++;
        }
      }
      for (let i = this.processingSourceIndex_ + 1;
           i < this.processingEntries.length; i++) {
        numRemainingItems +=
            Object.keys(this.processingEntries[i] || {}).length;
      }
    } else {
      numRemainingItems =
          this.sourceEntries.length - this.processingSourceIndex_;
    }

    return numRemainingItems;
  }
};

/**
 * Task to copy entries.
 */
fileOperationUtil.CopyTask = class extends fileOperationUtil.Task {
  /**
   * @param {string} taskId A unique ID for identifying this task.
   * @param {Array<Entry>} sourceEntries Array of source entries.
   * @param {DirectoryEntry} targetDirEntry Target directory.
   * @param {boolean} deleteAfterCopy Whether the delete original files after
   *     copy.
   */
  constructor(taskId, sourceEntries, targetDirEntry, deleteAfterCopy) {
    super(
        taskId,
        deleteAfterCopy ? util.FileOperationType.MOVE :
                          util.FileOperationType.COPY,
        sourceEntries, targetDirEntry);
    this.deleteAfterCopy = deleteAfterCopy;

    /**
     * Rate limiter which is used to avoid sending update request for progress
     * bar too frequently.
     * @type {AsyncUtil.RateLimiter}
     * @private
     */
    this.updateProgressRateLimiter_ = null;
  }

  /**
   * Initializes the CopyTask.
   * @param {function()} callback Called when the initialize is completed.
   */
  initialize(callback) {
    const group = new AsyncUtil.Group();
    // Correct all entries to be copied for status update.
    this.processingEntries = [];
    for (let i = 0; i < this.sourceEntries.length; i++) {
      group.add(function(index, callback) {
        fileOperationUtil.resolveRecursively_(
            this.sourceEntries[index],
            resolvedEntries => {
              const resolvedEntryMap = {};
              for (let j = 0; j < resolvedEntries.length; ++j) {
                const entry = resolvedEntries[j];
                entry.processedBytes = 0;
                resolvedEntryMap[entry.toURL()] = entry;
              }
              this.processingEntries[index] = resolvedEntryMap;
              callback();
            },
            error => {
              console.error('Failed to resolve for copy: %s', error.name);
              callback();
            });
      }.bind(this, i));
    }

    group.run(() => {
      // Fill totalBytes.
      this.totalBytes = 0;
      for (let i = 0; i < this.processingEntries.length; i++) {
        for (const entryURL in this.processingEntries[i]) {
          this.totalBytes += this.processingEntries[i][entryURL].size;
        }
      }

      callback();
    });
  }

  /**
   * Copies all entries to the target directory.
   * Note: this method contains also the operation of "Move" due to historical
   * reason.
   *
   * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
   *     Callback invoked when an entry is changed.
   * @param {function()} progressCallback Callback invoked periodically during
   *     the copying.
   * @param {function()} successCallback On success.
   * @param {function(fileOperationUtil.Error)} errorCallback On error.
   * @override
   */
  run(entryChangedCallback, progressCallback, successCallback, errorCallback) {
    // TODO(hidehiko): We should be able to share the code to iterate on entries
    // with serviceMoveTask_().
    if (this.sourceEntries.length == 0) {
      successCallback();
      return;
    }

    // TODO(hidehiko): Delete after copy is the implementation of Move.
    // Migrate the part into MoveTask.run().
    const deleteOriginals = () => {
      let count = this.sourceEntries.length;

      const onEntryDeleted = entry => {
        entryChangedCallback(util.EntryChangedKind.DELETED, entry);
        count--;
        if (!count) {
          successCallback();
        }
      };

      const onFilesystemError = err => {
        errorCallback(new fileOperationUtil.Error(
            util.FileOperationErrorType.FILESYSTEM_ERROR, err));
      };

      for (let i = 0; i < this.sourceEntries.length; i++) {
        const entry = this.sourceEntries[i];
        util.removeFileOrDirectory(
            entry, onEntryDeleted.bind(null, entry), onFilesystemError);
      }
    };

    /**
     * Accumulates processed bytes and call |progressCallback| if needed.
     *
     * @param {number} index The index of processing source.
     * @param {string} sourceEntryUrl URL of the entry which has been processed.
     * @param {number=} opt_size Processed bytes of the |sourceEntry|. If it is
     *     dropped, all bytes of the entry are considered to be processed.
     */
    let updateProgress = function(index, sourceEntryUrl, opt_size) {
      if (!sourceEntryUrl) {
        return;
      }

      const processedEntry = this.processingEntries[index][sourceEntryUrl];
      if (!processedEntry) {
        return;
      }

      const alreadyCompleted =
          processedEntry.processedBytes === processedEntry.size;

      // Accumulates newly processed bytes.
      const size = opt_size !== undefined ? opt_size : processedEntry.size;
      this.processedBytes += size - processedEntry.processedBytes;
      processedEntry.processedBytes = size;

      // updateProgress can be called multiple times for a single file copy, and
      // it might not be called for a small file.
      // The following prevents multiple call for a single file to decrement
      // numRemainingItems multiple times.
      // For small files, it will be decremented by next
      // calcNumRemainingItems_().
      if (!alreadyCompleted &&
          processedEntry.processedBytes === processedEntry.size) {
        this.numRemainingItems--;
      }

      // Updates progress bar in limited frequency so that intervals between
      // updates have at least 200ms.
      this.updateProgressRateLimiter_.run();
    };
    updateProgress = updateProgress.bind(this);

    this.updateProgressRateLimiter_ =
        new AsyncUtil.RateLimiter(progressCallback);

    this.numRemainingItems = this.calcNumRemainingItems_();

    // Number of consecutive errors. Increases while failing and resets to zero
    // when one of them succeeds.
    let errorCount = 0;
    let lastError;

    AsyncUtil.forEach(
        this.sourceEntries,
        (callback, entry, index) => {
          if (this.cancelRequested_) {
            errorCallback(new fileOperationUtil.Error(
                util.FileOperationErrorType.FILESYSTEM_ERROR,
                util.createDOMError(util.FileError.ABORT_ERR)));
            return;
          }
          progressCallback();
          this.processEntry_(
              assert(entry), assert(this.targetDirEntry),
              (sourceEntryUrl, destinationEntry) => {
                updateProgress(index, sourceEntryUrl);
                // The destination entry may be null, if the copied file got
                // deleted just after copying.
                if (destinationEntry) {
                  entryChangedCallback(
                      util.EntryChangedKind.CREATED, destinationEntry);
                }
              },
              (sourceEntryUrl, size) => {
                updateProgress(index, sourceEntryUrl, size);
              },
              () => {
                // Finishes off delayed updates if necessary.
                this.updateProgressRateLimiter_.runImmediately();
                // Update current source index and processing bytes.
                this.processingSourceIndex_ = index + 1;
                this.processedBytes = this.calcProcessedBytes_();
                this.numRemainingItems = this.calcNumRemainingItems_();
                errorCount = 0;
                callback();
              },
              error => {
                // Finishes off delayed updates if necessary.
                this.updateProgressRateLimiter_.runImmediately();
                // Update current source index and processing bytes.
                this.processingSourceIndex_ = index + 1;
                this.processedBytes = this.calcProcessedBytes_();
                this.numRemainingItems = this.calcNumRemainingItems_();
                errorCount++;
                lastError = error;
                if (errorCount <
                    fileOperationUtil.CopyTask.CONSECUTIVE_ERROR_LIMIT_) {
                  callback();
                } else {
                  errorCallback(error);
                }
              });
        },
        () => {
          if (lastError) {
            errorCallback(lastError);
          } else if (this.deleteAfterCopy) {
            deleteOriginals();
          } else {
            successCallback();
          }
        });
  }

  /**
   * Copies the source entry to the target directory.
   *
   * @param {!Entry} sourceEntry An entry to be copied.
   * @param {!DirectoryEntry} destinationEntry The entry which will contain the
   *     copied entry.
   * @param {function(string, Entry)} entryChangedCallback
   *     Callback invoked when an entry is created with the source URL and
   *     the destination Entry.
   * @param {function(string, number)} progressCallback Callback invoked
   *     periodically during the copying.
   * @param {function()} successCallback On success.
   * @param {function(fileOperationUtil.Error)} errorCallback On error.
   * @private
   */
  processEntry_(
      sourceEntry, destinationEntry, entryChangedCallback, progressCallback,
      successCallback, errorCallback) {
    fileOperationUtil.deduplicatePath(
        destinationEntry, sourceEntry.name, destinationName => {
          if (this.cancelRequested_) {
            errorCallback(new fileOperationUtil.Error(
                util.FileOperationErrorType.FILESYSTEM_ERROR,
                util.createDOMError(util.FileError.ABORT_ERR)));
            return;
          }
          this.cancelCallback_ = fileOperationUtil.copyTo(
              sourceEntry, destinationEntry, destinationName,
              entryChangedCallback, progressCallback,
              entry => {
                this.cancelCallback_ = null;
                successCallback();
              },
              error => {
                this.cancelCallback_ = null;
                errorCallback(new fileOperationUtil.Error(
                    util.FileOperationErrorType.FILESYSTEM_ERROR, error));
              });
        }, errorCallback);
  }
};

/**
 * Number of consecutive errors to stop CopyTask.
 * @const {number}
 * @private
 */
fileOperationUtil.CopyTask.CONSECUTIVE_ERROR_LIMIT_ = 100;

/**
 * Task to move entries.
 */
fileOperationUtil.MoveTask = class extends fileOperationUtil.Task {
  /**
   * @param {string} taskId A unique ID for identifying this task.
   * @param {Array<Entry>} sourceEntries Array of source entries.
   * @param {DirectoryEntry} targetDirEntry Target directory.
   */
  constructor(taskId, sourceEntries, targetDirEntry) {
    super(taskId, util.FileOperationType.MOVE, sourceEntries, targetDirEntry);
  }

  /**
   * Initializes the MoveTask.
   * @param {function()} callback Called when the initialize is completed.
   */
  initialize(callback) {
    // This may be moving from search results, where it fails if we
    // move parent entries earlier than child entries. We should
    // process the deepest entry first. Since move of each entry is
    // done by a single moveTo() call, we don't need to care about the
    // recursive traversal order.
    this.sourceEntries.sort((entry1, entry2) => {
      return entry2.toURL().length - entry1.toURL().length;
    });

    this.processingEntries = [];
    for (let i = 0; i < this.sourceEntries.length; i++) {
      const processingEntryMap = {};
      const entry = this.sourceEntries[i];

      // The move should be done with updating the metadata. So here we assume
      // all the file size is 1 byte. (Avoiding 0, so that progress bar can
      // move smoothly).
      // TODO(hidehiko): Remove this hack.
      entry.size = 1;
      processingEntryMap[entry.toURL()] = entry;
      this.processingEntries[i] = processingEntryMap;
    }

    callback();
  }

  /**
   * Moves all entries in the task.
   *
   * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
   *     Callback invoked when an entry is changed.
   * @param {function()} progressCallback Callback invoked periodically during
   *     the moving.
   * @param {function()} successCallback On success.
   * @param {function(fileOperationUtil.Error)} errorCallback On error.
   * @override
   */
  run(entryChangedCallback, progressCallback, successCallback, errorCallback) {
    if (this.sourceEntries.length == 0) {
      successCallback();
      return;
    }

    AsyncUtil.forEach(
        this.sourceEntries,
        (callback, entry, index) => {
          if (this.cancelRequested_) {
            errorCallback(new fileOperationUtil.Error(
                util.FileOperationErrorType.FILESYSTEM_ERROR,
                util.createDOMError(util.FileError.ABORT_ERR)));
            return;
          }
          progressCallback();
          fileOperationUtil.MoveTask.processEntry_(
              assert(entry), assert(this.targetDirEntry),
              entryChangedCallback, () => {
                // Update current source index.
                this.processingSourceIndex_ = index + 1;
                this.processedBytes = this.calcProcessedBytes_();
                this.numRemainingItems = this.calcNumRemainingItems_();
                callback();
              }, errorCallback);
        },
        () => {
          successCallback();
        });
  }

  /**
   * Moves the sourceEntry to the targetDirEntry in this task.
   *
   * @param {Entry} sourceEntry An entry to be moved.
   * @param {!DirectoryEntry} destinationEntry The entry of the destination
   *     directory.
   * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
   *     Callback invoked when an entry is changed.
   * @param {function()} successCallback On success.
   * @param {function(fileOperationUtil.Error)} errorCallback On error.
   * @private
   */
  static processEntry_(
      sourceEntry, destinationEntry, entryChangedCallback, successCallback,
      errorCallback) {
    const destination =
        /** @type{!DirectoryEntry} */ (
            assert(util.unwrapEntry(destinationEntry)));
    fileOperationUtil.deduplicatePath(
        destination, sourceEntry.name, destinationName => {
          sourceEntry.moveTo(
              destination, destinationName,
              movedEntry => {
                entryChangedCallback(util.EntryChangedKind.CREATED, movedEntry);
                entryChangedCallback(
                    util.EntryChangedKind.DELETED, sourceEntry);
                successCallback();
              },
              error => {
                errorCallback(new fileOperationUtil.Error(
                    util.FileOperationErrorType.FILESYSTEM_ERROR, error));
              });
        }, errorCallback);
  }
};

/**
 * Task to create a zip archive.
 */
fileOperationUtil.ZipTask = class extends fileOperationUtil.Task {
  /**
   * @param {string} taskId A unique ID for identifying this task.
   * @param {!Array<!Entry>} sourceEntries Array of source entries.
   * @param {!DirectoryEntry} targetDirEntry Target directory.
   * @param {!DirectoryEntry} zipBaseDirEntry Base directory dealt as a root
   *     in ZIP archive.
   */
  constructor(taskId, sourceEntries, targetDirEntry, zipBaseDirEntry) {
    super(taskId, util.FileOperationType.ZIP, sourceEntries, targetDirEntry);
    this.zipBaseDirEntry = zipBaseDirEntry;

    /** @type {boolean} */
    this.zip = true;
  }


  /**
   * Initializes the ZipTask.
   * @param {function()} callback Called when the initialize is completed.
   */
  initialize(callback) {
    const resolvedEntryMap = {};
    const group = new AsyncUtil.Group();
    for (let i = 0; i < this.sourceEntries.length; i++) {
      group.add(function(index, callback) {
        fileOperationUtil.resolveRecursively_(
            this.sourceEntries[index], entries => {
              for (let j = 0; j < entries.length; j++) {
                resolvedEntryMap[entries[j].toURL()] = entries[j];
              }
              callback();
            }, callback);
      }.bind(this, i));
    }

    group.run(() => {
      // For zip archiving, all the entries are processed at once.
      this.processingEntries = [resolvedEntryMap];

      this.totalBytes = 0;
      for (const url in resolvedEntryMap) {
        this.totalBytes += resolvedEntryMap[url].size;
      }

      callback();
    });
  }

  /**
   * Runs a zip file creation task.
   *
   * @param {function(util.EntryChangedKind, Entry)} entryChangedCallback
   *     Callback invoked when an entry is changed.
   * @param {function()} progressCallback Callback invoked periodically during
   *     the moving.
   * @param {function()} successCallback On complete.
   * @param {function(fileOperationUtil.Error)} errorCallback On error.
   * @override
   */
  run(entryChangedCallback, progressCallback, successCallback, errorCallback) {
    // TODO(hidehiko): we should localize the name.
    let destName = 'Archive';
    if (this.sourceEntries.length == 1) {
      const entryName = this.sourceEntries[0].name;
      const i = entryName.lastIndexOf('.');
      destName = ((i < 0) ? entryName : entryName.substr(0, i));
    }

    fileOperationUtil.deduplicatePath(
        this.targetDirEntry, destName + '.zip', destPath => {
          // TODO: per-entry zip progress update with accurate byte count.
          // For now just set completedBytes to 0 so that it is not full until
          // the zip operatoin is done.
          this.processedBytes = 0;
          progressCallback();

          // The number of elements in processingEntries is 1. See also
          // initialize().
          const entries = [];
          for (const url in this.processingEntries[0]) {
            entries.push(this.processingEntries[0][url]);
          }

          fileOperationUtil.zipSelection(
              entries, this.zipBaseDirEntry, destPath,
              entry => {
                this.processedBytes = this.totalBytes;
                entryChangedCallback(util.EntryChangedKind.CREATED, entry);
                successCallback();
              },
              error => {
                errorCallback(new fileOperationUtil.Error(
                    util.FileOperationErrorType.FILESYSTEM_ERROR, error));
              });
        }, errorCallback);
  }
};

/**
 * @typedef {{
 *  entries: Array<Entry>,
 *  taskId: string,
 *  entrySize: Object,
 *  totalBytes: number,
 *  processedBytes: number,
 *  cancelRequested: boolean
 * }}
 */
fileOperationUtil.DeleteTask;

/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 *
 * @param {util.FileOperationErrorType} code Error type.
 * @param {string|Entry|DOMError} data Additional data.
 * @constructor
 */
fileOperationUtil.Error = function(code, data) {
  this.code = code;
  this.data = data;
};

/**
 * Manages Event dispatching.
 * Currently this can send three types of events: "copy-progress",
 * "copy-operation-completed" and "delete".
 *
 * TODO(hidehiko): Reorganize the event dispatching mechanism.
 */
fileOperationUtil.EventRouter = class extends cr.EventTarget {
  constructor() {
    super();
    this.pendingDeletedEntries_ = {};
    this.pendingCreatedEntries_ = {};
    this.entryChangedEventRateLimiter_ = new AsyncUtil.RateLimiter(
        this.dispatchEntryChangedEvent_.bind(this), 500);
  }

  /**
   * Dispatches a simple "copy-progress" event with reason and current
   * FileOperationManager status. If it is an ERROR event, error should be set.
   *
   * @param {fileOperationUtil.EventRouter.EventType} type Event type.
   * @param {Object} status Current FileOperationManager's status. See also
   *     FileOperationManager.Task.getStatus().
   * @param {string} taskId ID of task related with the event.
   * @param {fileOperationUtil.Error=} opt_error The info for the error. This
   *     should be set iff the reason is "ERROR".
   */
  sendProgressEvent(type, status, taskId, opt_error) {
    const EventType = fileOperationUtil.EventRouter.EventType;
    // Before finishing operation, dispatch pending entries-changed events.
    if (type === EventType.SUCCESS || type === EventType.CANCELED) {
      this.entryChangedEventRateLimiter_.runImmediately();
    }

    const event = /** @type {FileOperationProgressEvent} */
        (new Event('copy-progress'));
    event.reason = type;
    event.status = status;
    event.taskId = taskId;
    if (opt_error) {
      event.error = opt_error;
    }
    this.dispatchEvent(event);
  }

  /**
   * Stores changed (created or deleted) entry temporarily, and maybe dispatch
   * entries-changed event with stored entries.
   * @param {util.EntryChangedKind} kind The enum to represent if the entry is
   *     created or deleted.
   * @param {Entry} entry The changed entry.
   */
  sendEntryChangedEvent(kind, entry) {
    if (kind === util.EntryChangedKind.DELETED) {
      this.pendingDeletedEntries_[entry.toURL()] = entry;
    }
    if (kind === util.EntryChangedKind.CREATED) {
      this.pendingCreatedEntries_[entry.toURL()] = entry;
    }

    this.entryChangedEventRateLimiter_.run();
  }

  /**
   * Dispatches an event to notify that entries are changed (created or
   * deleted).
   * @private
   */
  dispatchEntryChangedEvent_() {
    const deletedEntries = [];
    const createdEntries = [];
    for (var url in this.pendingDeletedEntries_) {
      deletedEntries.push(this.pendingDeletedEntries_[url]);
    }
    for (var url in this.pendingCreatedEntries_) {
      createdEntries.push(this.pendingCreatedEntries_[url]);
    }
    if (deletedEntries.length > 0) {
      var event = new Event('entries-changed');
      event.kind = util.EntryChangedKind.DELETED;
      event.entries = deletedEntries;
      this.dispatchEvent(event);
      this.pendingDeletedEntries_ = {};
    }
    if (createdEntries.length > 0) {
      var event = new Event('entries-changed');
      event.kind = util.EntryChangedKind.CREATED;
      event.entries = createdEntries;
      this.dispatchEvent(event);
      this.pendingCreatedEntries_ = {};
    }
  }

  /**
   * Dispatches an event to notify entries are changed for delete task.
   *
   * @param {fileOperationUtil.EventRouter.EventType} reason Event type.
   * @param {!Object} task Delete task related with the event.
   */
  sendDeleteEvent(reason, task) {
    const event =
        /** @type {FileOperationProgressEvent} */ (new Event('delete'));
    event.reason = reason;
    event.taskId = task.taskId;
    event.entries = task.entries;
    event.totalBytes = task.totalBytes;
    event.processedBytes = task.processedBytes;
    this.dispatchEvent(event);
  }
};

/**
 * Types of events emitted by the EventRouter.
 * @enum {string}
 */
fileOperationUtil.EventRouter.EventType = {
  BEGIN: 'BEGIN',
  CANCELED: 'CANCELED',
  ERROR: 'ERROR',
  PROGRESS: 'PROGRESS',
  SUCCESS: 'SUCCESS'
};
