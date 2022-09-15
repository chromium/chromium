// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {startIOTask} from '../../common/js/api.js';
import {AsyncUtil} from '../../common/js/async_util.js';
import {FileOperationError, FileOperationProgressEvent} from '../../common/js/file_operation_common.js';
import {CombinedReaders} from '../../common/js/files_app_entry_types.js';
import {createTrashReaders, TrashEntry} from '../../common/js/trash.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {xfm} from '../../common/js/xfm.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {fileOperationUtil} from './file_operation_util.js';
import {metadataProxy} from './metadata_proxy.js';
import {Trash} from './trash.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * FileOperationManagerImpl: implementation of {FileOperationManager}.
 *
 * @implements {FileOperationManager}
 */
export class FileOperationManagerImpl {
  constructor() {
    /**
     * TODO(crbug.com/953256) Add closure annotation.
     * @private
     */
    this.fileManager_ = null;

    /**
     * @private {VolumeManager}
     */
    this.volumeManager_ = null;

    /**
     * List of pending copy tasks. The manager can execute tasks in arbitrary
     * order.
     * @private {!Array<!fileOperationUtil.Task>}
     */
    this.pendingCopyTasks_ = [];

    /**
     * Map of volume id and running copy task. The key is a volume id and the
     * value is a copy task.
     * @private {!Object<!fileOperationUtil.Task>}
     */
    this.runningCopyTasks_ = {};

    /**
     * @private {!Array<!fileOperationUtil.DeleteTask>}
     */
    this.deleteTasks_ = [];

    /**
     * @private {number}
     */
    this.taskIdCounter_ = 0;

    /**
     * @private {!fileOperationUtil.EventRouter}
     * @const
     */
    this.eventRouter_ = new fileOperationUtil.EventRouter();

    /**
     * @private {!Trash}
     * @const
     */
    this.trash_ = new Trash();
  }

  /**
   * Store a reference to our owning File Manager.
   * @param {Object} fileManager reference to the 'foreground' app.
   */
  setFileManager(fileManager) {
    this.fileManager_ = fileManager;
  }

  /**
   * Adds an event listener for the tasks.
   * @param {string} type The name of the event.
   * @param {EventListener|function(!Event):*} handler The handler for the
   *     event. This is called when the event is dispatched.
   * @override
   */
  addEventListener(type, handler) {
    this.eventRouter_.addEventListener(type, handler);
  }

  /**
   * Removes an event listener for the tasks.
   * @param {string} type The name of the event.
   * @param {EventListener|function(!Event):*} handler The handler to be
   *     removed.
   * @override
   */
  removeEventListener(type, handler) {
    this.eventRouter_.removeEventListener(type, handler);
  }

  /** @override */
  dispatchEvent() {
    // Not used. Just need this to satisfy the compiler @implements
    // FileOperationManager interface.
  }

  /**
   * Checks if there are any tasks in the queue.
   * @return {boolean} True, if there are any tasks.
   */
  hasQueuedTasks() {
    return Object.keys(this.runningCopyTasks_).length > 0 ||
        this.pendingCopyTasks_.length > 0 || this.deleteTasks_.length > 0;
  }

  /**
   * Returns pending copy tasks for testing.
   * @return {!Array<!fileOperationUtil.Task>} Pending copy tasks.
   */
  getPendingCopyTasksForTesting() {
    return this.pendingCopyTasks_;
  }

  /**
   * Fetches the root label of a volume from the volume manager.
   * @param {!Entry} entry The directory entry to get the volume label.
   * @return {string} identifying label for the volume.
   */
  getVolumeLabel_(entry) {
    const destinationLocationInfo = this.volumeManager_.getLocationInfo(entry);
    if (destinationLocationInfo) {
      return util.getRootTypeLabel(destinationLocationInfo);
    }
    return '';
  }

  /**
   * Returns status information for a running task.
   * @param {fileOperationUtil.Task} task The task we use to retrieve status
   *     from.
   * @return {!fileOperationUtil.Status} Status object with optional volume
   *     information.
   */
  getTaskStatus(task) {
    const status = task.getStatus();
    // If there's no target directory name, use the volume name for UI display.
    if (status.targetDirEntryName === '' && task.targetDirEntry) {
      const entry = /** {Entry} */ (task.targetDirEntry);
      if (this.volumeManager_) {
        status.targetDirEntryName = this.getVolumeLabel_(entry);
      }
    }
    return status;
  }

  /**
   * Requests the specified task to be canceled.
   * @param {string} taskId ID of task to be canceled.
   */
  requestTaskCancel(taskId) {
    let task = null;

    // If the task is not on progress, remove it immediately.
    for (let i = 0; i < this.pendingCopyTasks_.length; i++) {
      task = this.pendingCopyTasks_[i];
      if (task.taskId !== taskId) {
        continue;
      }
      task.requestCancel();
      this.eventRouter_.sendProgressEvent(
          FileOperationProgressEvent.EventType.CANCELED,
          this.getTaskStatus(task), task.taskId);
      this.pendingCopyTasks_.splice(i, 1);
    }

    for (const volumeId in this.runningCopyTasks_) {
      task = this.runningCopyTasks_[volumeId];
      if (task.taskId === taskId) {
        task.requestCancel();
      }
    }

    for (let i = 0; i < this.deleteTasks_.length; i++) {
      task = this.deleteTasks_[i];
      if (task.taskId !== taskId) {
        continue;
      }
      task.cancelRequested = true;
      // If the task is not on progress, remove it immediately.
      if (i !== 0) {
        this.eventRouter_.sendDeleteEvent(
            FileOperationProgressEvent.EventType.CANCELED, task);
        this.deleteTasks_.splice(i, 1);
      }
    }
  }

  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FakeEntry} targetEntry The destination entry of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {!Promise<Array<Entry>>} Promise fulfilled with the filtered entry.
   *     This is not rejected.
   */
  async filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {
    if (!isMove) {
      return sourceEntries;
    }

    // Check all file entries and keeps only those need sharing operation.
    const processEntry = entry => {
      return new Promise(resolve => {
        entry.getParent(
            inParentEntry => {
              if (!util.isSameEntry(inParentEntry, targetEntry)) {
                resolve(entry);
              } else {
                resolve(null);
              }
            },
            error => {
              console.warn(error.stack || error);
              resolve(null);
            });
      });
    };

    // Call processEntry for each item of sourceEntries.
    const result = await Promise.all(sourceEntries.map(processEntry));

    // Remove null entries.
    return result.filter(entry => !!entry);
  }

  /**
   * Service all pending tasks, as well as any that might appear during the
   * copy. We allow to run tasks in parallel when destinations are different
   * volumes.
   *
   * @private
   */
  serviceAllTasks_() {
    if (this.pendingCopyTasks_.length === 0 &&
        Object.keys(this.runningCopyTasks_).length === 0) {
      // All tasks have been serviced, clean up and exit.
      xfm.power.releaseKeepAwake();
      return;
    }

    if (!this.volumeManager_) {
      volumeManagerFactory.getInstance().then(volumeManager => {
        this.volumeManager_ = volumeManager;
        this.serviceAllTasks_();
      });
      return;
    }

    // Prevent the system from sleeping while copy is in progress.
    xfm.power.requestKeepAwake('system');

    // Find next task which can run at now.
    let nextTask = null;
    let nextTaskVolumeId = null;
    for (let i = 0; i < this.pendingCopyTasks_.length; i++) {
      const task = this.pendingCopyTasks_[i];

      // Fails a copy task of which it fails to get volume info. The destination
      // volume might be already unmounted.
      const volumeInfo = this.volumeManager_.getVolumeInfo(
          /** @type {!DirectoryEntry} */ (task.targetDirEntry));
      if (volumeInfo === null) {
        this.eventRouter_.sendProgressEvent(
            FileOperationProgressEvent.EventType.ERROR,
            this.getTaskStatus(task), task.taskId,
            new FileOperationError(
                util.FileOperationErrorType.FILESYSTEM_ERROR,
                util.createDOMError(util.FileError.NOT_FOUND_ERR)));

        this.pendingCopyTasks_.splice(i, 1);
        i--;

        continue;
      }

      // When no task is running for the volume, run the task.
      if (!this.runningCopyTasks_[volumeInfo.volumeId]) {
        nextTask = this.pendingCopyTasks_.splice(i, 1)[0];
        nextTaskVolumeId = volumeInfo.volumeId;
        break;
      }
    }

    // There is no task which can run at now.
    if (nextTask === null) {
      return;
    }

    const onTaskProgress = function(task) {
      this.eventRouter_.sendProgressEvent(
          FileOperationProgressEvent.EventType.PROGRESS,
          this.getTaskStatus(task), task.taskId);
    }.bind(this, nextTask);

    const onEntryChanged = (kind, entry) => {
      this.eventRouter_.sendEntryChangedEvent(kind, entry);
    };

    // Since getVolumeInfo of targetDirEntry might not be available when this
    // callback is called, bind volume id here.
    const onTaskError = function(volumeId, err) {
      const task = this.runningCopyTasks_[volumeId];
      delete this.runningCopyTasks_[volumeId];

      const reason = err.data.name === util.FileError.ABORT_ERR ?
          FileOperationProgressEvent.EventType.CANCELED :
          FileOperationProgressEvent.EventType.ERROR;
      this.eventRouter_.sendProgressEvent(
          reason, this.getTaskStatus(task), task.taskId, err);
      this.serviceAllTasks_();
    }.bind(this, nextTaskVolumeId);

    const onTaskSuccess = function(volumeId) {
      const task = this.runningCopyTasks_[volumeId];
      delete this.runningCopyTasks_[volumeId];

      this.eventRouter_.sendProgressEvent(
          FileOperationProgressEvent.EventType.SUCCESS,
          this.getTaskStatus(task), task.taskId);
      this.serviceAllTasks_();
    }.bind(this, nextTaskVolumeId);

    // Add to running tasks and run it.
    this.runningCopyTasks_[nextTaskVolumeId] = nextTask;

    this.eventRouter_.sendProgressEvent(
        FileOperationProgressEvent.EventType.PROGRESS,
        this.getTaskStatus(nextTask), nextTask.taskId);
    nextTask.run(onEntryChanged, onTaskProgress, onTaskSuccess, onTaskError);
  }

  /**
   * Returns true if all entries will use trash for delete.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries.
   * @return {boolean}
   */
  willUseTrash(volumeManager, entries) {
    return entries.every(
        entry => this.trash_.shouldMoveToTrash(volumeManager, entry));
  }

  /**
   * Schedules the files deletion.
   *
   * @param {!Array<!Entry>} entries The entries.
   * @param {boolean=} permanentlyDelete if true, entries will be deleted rather
   *     than moved to trash.
   */
  deleteEntries(entries, permanentlyDelete = false) {
    if (permanentlyDelete) {
      startIOTask(chrome.fileManagerPrivate.IOTaskType.DELETE, entries, {});
      return;
    }
    this.deleteOrRestore_(
        util.FileOperationType.DELETE, entries, permanentlyDelete);
  }

  /**
   * Schedule delete or restore.
   *
   * @param {!util.FileOperationType} operationType DELETE or RESTORE.
   * @param {!Array<!Entry|!TrashEntry>} entries The entries.
   * @param {boolean=} permanentlyDelete if true, entries will be deleted rather
   *     than moved to trash. Only applies to operationType DELETE.
   * @private
   */
  deleteOrRestore_(operationType, entries, permanentlyDelete = false) {
    const task =
        /** @type {!fileOperationUtil.DeleteTask} */ (Object.preventExtensions({
          operationType: operationType,
          entries: entries,
          taskId: this.generateTaskId(),
          entrySize: {},
          totalBytes: 0,
          processedBytes: 0,
          cancelRequested: false,
          trashedEntries: [],
          permanentlyDelete,
        }));

    // Obtains entry size and sum them up.
    const group = new AsyncUtil.Group();
    for (let i = 0; i < task.entries.length; i++) {
      group.add(function(entry, callback) {
        metadataProxy.getEntryMetadata(entry).then(
            metadata => {
              task.entrySize[entry.toURL()] = metadata.size;
              task.totalBytes += metadata.size;
              callback();
            },
            () => {
              // Fail to obtain the metadata. Use fake value 1.
              task.entrySize[entry.toURL()] = 1;
              task.totalBytes += 1;
              callback();
            });
      }.bind(this, task.entries[i]));
    }

    // Add a delete task.
    group.run(() => {
      this.deleteTasks_.push(task);
      this.eventRouter_.sendDeleteEvent(
          FileOperationProgressEvent.EventType.BEGIN, task);
      if (this.deleteTasks_.length === 1) {
        this.serviceAllDeleteTasks_();
      }
    });
  }

  /**
   * Service all pending delete/restore tasks, as well as any that might appear
   * during the deletion.
   *
   * Must not be called if there is an in-flight delete/restore task.
   *
   * @private
   */
  serviceAllDeleteTasks_() {
    if (!this.volumeManager_) {
      volumeManagerFactory.getInstance().then(volumeManager => {
        this.volumeManager_ = volumeManager;
        this.serviceAllDeleteTasks_();
      });
      return;
    }

    this.serviceDeleteTask_(this.deleteTasks_[0], () => {
      this.deleteTasks_.shift();
      if (this.deleteTasks_.length) {
        this.serviceAllDeleteTasks_();
      }
    });
  }

  /**
   * Performs the deletion or restore.
   *
   * @param {!Object} task The delete task (see deleteOrRestore_()).
   * @param {function()} callback Callback run on task end.
   * @private
   */
  serviceDeleteTask_(task, callback) {
    const queue = new AsyncUtil.Queue();

    // Delete or restore each entry.
    let error = null;
    const deleteOneEntry = inCallback => {
      if (!task.entries.length || task.cancelRequested || error) {
        inCallback();
        return;
      }
      this.eventRouter_.sendDeleteEvent(
          FileOperationProgressEvent.EventType.PROGRESS, task);

      // Operation will be either delete, or restore.
      let operation;
      switch (task.operationType) {
        case util.FileOperationType.DELETE:
          operation = this.trash_
                          .removeFileOrDirectory(
                              assert(this.volumeManager_), task.entries[0],
                              task.permanentlyDelete)
                          .then(trashEntry => {
                            if (trashEntry) {
                              task.trashedEntries.push(trashEntry);
                            }
                          });
          break;

        case util.FileOperationType.RESTORE:
          operation =
              this.trash_.restore(assert(this.volumeManager_), task.entries[0]);
          break;

        default:
          operation =
              Promise.reject('Unkonwn operation type ' + task.operationType);
      }

      operation
          .then(() => {
            this.eventRouter_.sendEntryChangedEvent(
                util.EntryChangedKind.DELETED, task.entries[0]);
            task.processedBytes += task.entrySize[task.entries[0].toURL()];
            task.entries.shift();
            deleteOneEntry(inCallback);
          })
          .catch(inError => {
            error = inError.message;
            inCallback();
          });
    };
    queue.run(deleteOneEntry);

    // Send an event and finish the async steps.
    queue.run(inCallback => {
      const EventType = FileOperationProgressEvent.EventType;
      let reason;
      if (error) {
        reason = EventType.ERROR;
      } else if (task.cancelRequested) {
        reason = EventType.CANCELED;
      } else {
        reason = EventType.SUCCESS;
      }
      this.eventRouter_.sendDeleteEvent(
          reason, task,
          new FileOperationError(
              util.FileOperationErrorType.FILESYSTEM_ERROR, error));
      inCallback();
      callback();
    });
  }

  /**
   * Notifies File Manager that an extraction operation has finished.
   *
   * @param {number} taskId The unique task id for the IO operation.
   * @suppress {missingProperties}
   */
  notifyExtractDone(taskId) {
    // TODO(crbug.com/953256) Add closure annotation.
    // taskController is set asynchronously, this can be called on startup
    // if another SWA window is finishing an extract (crbug.com/1348432).
    if (this.fileManager_.taskController) {
      this.fileManager_.taskController.deleteExtractTaskDetails(taskId);
    }
  }

  /**
   * Called when an IOTask finished with a NEED_PASSWORD status.
   * Delegate it to the task controller to deal with it.
   *
   * @param {number} taskId The unique task id for the IO operation.
   * @suppress {missingProperties}
   */
  handleMissingPassword(taskId) {
    // TODO(crbug.com/953256) Add closure annotation.
    // null check is unlikely to be needed, but there's no guarantee
    // that taskController has been initialized on a password event.
    if (this.fileManager_.taskController) {
      this.fileManager_.taskController.handleMissingPassword(taskId);
    }
  }

  /**
   * Writes file to destination dir. This function is called when an image is
   * dragged from a web page. In this case there is no FileSystem Entry to copy
   * or move, just the JS File object with attached Blob. This operation does
   * not use EventRouter or queue the task since it is not possible to track
   * progress of the FileWriter.write().
   *
   * @param {!File} file The file entry to be written.
   * @param {!DirectoryEntry} dir The destination directory to write to.
   * @return {!Promise<!FileEntry>}
   */
  async writeFile(file, dir) {
    const name = await fileOperationUtil.deduplicatePath(dir, file.name);
    return new Promise((resolve, reject) => {
      dir.getFile(name, {create: true, exclusive: true}, f => {
        f.createWriter(writer => {
          writer.onwriteend = () => resolve(f);
          writer.onerror = reject;
          writer.write(file);
        }, reject);
      }, reject);
    });
  }

  /**
   * Generates new task ID.
   *
   * @return {string} New task ID.
   */
  generateTaskId() {
    return 'file-operation-' + this.taskIdCounter_++;
  }
}
