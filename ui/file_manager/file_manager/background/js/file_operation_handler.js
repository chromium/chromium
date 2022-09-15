// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {startIOTask} from '../../common/js/api.js';
import {FileOperationProgressEvent} from '../../common/js/file_operation_common.js';
import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {str, strf, util} from '../../common/js/util.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';

/**
 * An event handler of the background page for file operations.
 */
export class FileOperationHandler {
  /**
   * @param {!FileOperationManager} fileOperationManager
   * @param {!ProgressCenter} progressCenter
   */
  constructor(fileOperationManager, progressCenter) {
    /**
     * File operation manager.
     * @type {!FileOperationManager}
     * @private
     */
    this.fileOperationManager_ = fileOperationManager;

    /**
     * Progress center.
     * @type {!ProgressCenter}
     * @private
     */
    this.progressCenter_ = progressCenter;

    /**
     * Toast element to emit notifications.
     * @type {Object|null}
     * @private
     */
    this.toast_ = null;

    /**
     * Pending items of delete operation.
     *
     * Delete operations are usually complete quickly.
     * So we would not like to show the progress bar at first.
     * If the operation takes more than FileOperationHandler.PENDING_TIME_MS_,
     * we adds the item to the progress center.
     *
     * @type {Object<ProgressCenterItem>}}
     * @private
     */
    this.pendingItems_ = {};

    // Register event.
    this.fileOperationManager_.addEventListener(
        'copy-progress', this.onCopyProgress_.bind(this));
    this.fileOperationManager_.addEventListener(
        'delete', this.onDeleteProgress_.bind(this));

    chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(
        this.onIOTaskProgressStatus_.bind(this));
  }

  /**
   * Process the IO Task ProgressStatus events.
   * @param {!chrome.fileManagerPrivate.ProgressStatus} event
   * @private
   */
  onIOTaskProgressStatus_(event) {
    const taskId = String(event.taskId);
    let newItem = false;
    /** @type {ProgressCenterItem} */
    let item = this.progressCenter_.getItemById(taskId);
    if (!item) {
      item = new ProgressCenterItem();
      newItem = true;
      item.id = taskId;
      item.type = getTypeFromIOTaskType_(event.type);
      item.itemCount = event.itemCount;
      item.cancelCallback = () => {
        chrome.fileManagerPrivate.cancelIOTask(event.taskId);
      };
    }
    item.message = getMessageFromProgressEvent_(event);
    item.sourceMessage = event.sourceName;
    item.destinationMessage = event.destinationName;

    switch (event.state) {
      case chrome.fileManagerPrivate.IOTaskState.QUEUED:
        item.progressMax = event.totalBytes;
        item.progressValue = event.bytesTransferred;
        item.remainingTime = event.remainingSeconds;
        break;
      case chrome.fileManagerPrivate.IOTaskState.SCANNING:
        item.sourceMessage = event.sourceName;
        item.destinationMessage = event.destinationName;
        item.state = ProgressItemState.SCANNING;
        item.progressMax = event.totalBytes;
        item.progressValue = event.bytesTransferred;
        item.remainingTime = event.remainingSeconds;
        break;
      case chrome.fileManagerPrivate.IOTaskState.IN_PROGRESS:
        item.progressMax = event.totalBytes;
        item.progressValue = event.bytesTransferred;
        item.remainingTime = event.remainingSeconds;
        item.state = ProgressItemState.PROGRESSING;
        break;

      case chrome.fileManagerPrivate.IOTaskState.SUCCESS:
      case chrome.fileManagerPrivate.IOTaskState.CANCELLED:
      case chrome.fileManagerPrivate.IOTaskState.ERROR:
        if (newItem) {
          // ERROR events can be dispatched before BEGIN events.
          item.progressMax = 1;
        }
        if (event.state === chrome.fileManagerPrivate.IOTaskState.SUCCESS) {
          item.state = ProgressItemState.COMPLETED;
          item.progressValue = item.progressMax;
          item.remainingTime = event.remainingSeconds;
          if (item.type === ProgressItemType.TRASH) {
            const infoEntries =
                event.outputs.filter(o => o.name.endsWith('.trashinfo'));
            item.setExtraButton(
                ProgressItemState.COMPLETED, str('UNDO_DELETE_ACTION_LABEL'),
                () => {
                  startIOTask(
                      chrome.fileManagerPrivate.IOTaskType.RESTORE, infoEntries,
                      /*params=*/ {});
                });
          }
        } else if (
            event.state === chrome.fileManagerPrivate.IOTaskState.CANCELLED) {
          item.state = ProgressItemState.CANCELED;
        } else {
          item.state = ProgressItemState.ERROR;
        }
        // Extract IOTask details are only stored while an operation is active.
        if (item.type == ProgressItemType.EXTRACT) {
          this.fileOperationManager_.notifyExtractDone(event.taskId);
        }
        break;
      case chrome.fileManagerPrivate.IOTaskState.NEED_PASSWORD:
        // Set state to canceled so notification doesn't display.
        item.state = ProgressItemState.CANCELED;
        assert(item.type == ProgressItemType.EXTRACT);
        this.fileOperationManager_.handleMissingPassword(event.taskId);
        break;
      default:
        console.error(`Invalid IOTaskState: ${event.state}`);
    }
    if (!event.showNotification) {
      // Set state to canceled so notification doesn't display.
      item.state = ProgressItemState.CANCELED;
    }
    this.progressCenter_.updateItem(item);
  }

  get toast() {
    if (!this.toast_) {
      // The background page does not contain the requisite types to include the
      // FilesToast type without type checking the Polymer element.
      this.toast_ = /** @type {!Object} */ (document.getElementById('toast'));
    }

    return this.toast_;
  }

  /**
   * On a successful trash operation, show a toast notification on Files app
   * with an undo action to restore the files that were just trashed.
   * @param {!chrome.fileManagerPrivate.ProgressStatus} event
   * @private
   */
  showRestoreTrashToast_(event) {
    if (event.type !== chrome.fileManagerPrivate.IOTaskType.TRASH ||
        event.state !== chrome.fileManagerPrivate.IOTaskState.SUCCESS) {
      return;
    }

    const message = (event.itemCount === 1) ?
        strf('UNDO_DELETE_ONE', event.sourceName) :
        strf('UNDO_DELETE_SOME', event.itemCount);
    const infoEntries =
        event.outputs.filter(o => o.name.endsWith('.trashinfo'));
    this.toast.show(message, {
      text: str('UNDO_DELETE_ACTION_LABEL'),
      callback: () => {
        startIOTask(
            chrome.fileManagerPrivate.IOTaskType.RESTORE, infoEntries,
            /*params=*/ {});
      },
    });
  }

  /**
   * Handles the copy-progress event.
   * @param {Event} event The copy-progress event.
   * @private
   */
  onCopyProgress_(event) {
    const EventType = FileOperationProgressEvent.EventType;
    event = /** @type {FileOperationProgressEvent} */ (event);

    // Update progress center.
    const progressCenter = this.progressCenter_;
    let item;
    switch (event.reason) {
      case EventType.BEGIN:
        item = new ProgressCenterItem();
        item.id = event.taskId;
        item.type = FileOperationHandler.getType_(event.status.operationType);
        item.message = FileOperationHandler.getMessage_(event);
        item.itemCount = event.status.numRemainingItems;
        item.sourceMessage = event.status.processingEntryName;
        item.destinationMessage = event.status.targetDirEntryName;
        item.progressMax = event.status.totalBytes;
        item.progressValue = event.status.processedBytes;
        item.cancelCallback = this.fileOperationManager_.requestTaskCancel.bind(
            this.fileOperationManager_, event.taskId);
        item.remainingTime = event.status.remainingTime;
        progressCenter.updateItem(item);
        break;

      case EventType.PROGRESS:
        item = progressCenter.getItemById(event.taskId);
        if (!item) {
          console.error('Cannot find copying item.');
          return;
        }
        item.message = FileOperationHandler.getMessage_(event);
        item.progressMax = event.status.totalBytes;
        item.progressValue = event.status.processedBytes;
        item.remainingTime = event.status.remainingTime;
        progressCenter.updateItem(item);
        break;

      case EventType.SUCCESS:
      case EventType.CANCELED:
      case EventType.ERROR:
        item = progressCenter.getItemById(event.taskId);
        if (!item) {
          // ERROR events can be dispatched before BEGIN events.
          item = new ProgressCenterItem();
          item.type = FileOperationHandler.getType_(event.status.operationType);
          item.id = event.taskId;
          item.progressMax = 1;
        }
        if (event.reason === EventType.SUCCESS) {
          item.message = '';
          item.state = ProgressItemState.COMPLETED;
          item.progressValue = item.progressMax;
          item.remainingTime = event.status.remainingTime;
        } else if (event.reason === EventType.CANCELED) {
          item.message = '';
          item.state = ProgressItemState.CANCELED;
        } else {
          item.message = FileOperationHandler.getMessage_(event);
          item.state = ProgressItemState.ERROR;
        }
        progressCenter.updateItem(item);
        break;
    }
  }

  /**
   * Handles the delete event, and also the restore event which is similar to
   * delete in that as items complete, they are removed from the containing
   * directory.
   * @param {Event} event The delete or restore event.
   * @private
   */
  onDeleteProgress_(event) {
    const EventType = FileOperationProgressEvent.EventType;
    event = /** @type {FileOperationProgressEvent} */ (event);

    // Update progress center.
    const progressCenter = this.progressCenter_;
    let item;
    let pending;
    switch (event.reason) {
      case EventType.BEGIN:
        item = new ProgressCenterItem();
        item.id = event.taskId;
        item.type = ProgressItemType.DELETE;
        item.message = FileOperationHandler.getMessage_(event);
        item.itemCount = event.status.numRemainingItems;
        item.progressMax = event.totalBytes;
        item.progressValue = event.processedBytes;
        item.cancelCallback = this.fileOperationManager_.requestTaskCancel.bind(
            this.fileOperationManager_, event.taskId);
        this.pendingItems_[item.id] = item;
        setTimeout(
            this.showPendingItem_.bind(this, item),
            FileOperationHandler.PENDING_TIME_MS_);
        break;

      case EventType.PROGRESS:
        pending = event.taskId in this.pendingItems_;
        item = this.pendingItems_[event.taskId] ||
            progressCenter.getItemById(event.taskId);
        if (!item) {
          console.error('Cannot find deleting item.');
          return;
        }
        item.message = FileOperationHandler.getMessage_(event);
        item.progressMax = event.totalBytes;
        item.progressValue = event.processedBytes;
        if (!pending) {
          progressCenter.updateItem(item);
        }
        break;

      case EventType.SUCCESS:
      case EventType.CANCELED:
      case EventType.ERROR:
        // Obtain working variable.
        pending = event.taskId in this.pendingItems_;
        item = this.pendingItems_[event.taskId] ||
            progressCenter.getItemById(event.taskId);
        if (!item) {
          console.error('Cannot find deleting item.');
          return;
        }

        // Update the item.
        item.message = FileOperationHandler.getMessage_(event);
        if (event.reason === EventType.SUCCESS) {
          item.state = ProgressItemState.COMPLETED;
          item.progressValue = item.progressMax;
        } else if (event.reason === EventType.CANCELED) {
          item.state = ProgressItemState.CANCELED;
        } else {
          item.state = ProgressItemState.ERROR;
        }

        // Apply the change.
        if (!pending || event.reason === EventType.ERROR) {
          progressCenter.updateItem(item);
        }
        if (pending) {
          delete this.pendingItems_[event.taskId];
        }
        break;
    }
  }

  /**
   * Shows the pending item.
   *
   * @param {ProgressCenterItem} item Pending item.
   * @private
   */
  showPendingItem_(item) {
    // The item is already gone.
    if (!this.pendingItems_[item.id]) {
      return;
    }
    delete this.pendingItems_[item.id];
    this.progressCenter_.updateItem(item);
  }

  /**
   * Generate a progress message from the event.
   * @param {FileOperationProgressEvent} event Progress event.
   * @return {string} message.
   * @private
   */
  static getMessage_(event) {
    if (event.reason === FileOperationProgressEvent.EventType.ERROR) {
      switch (event.error.code) {
        case util.FileOperationErrorType.TARGET_EXISTS:
          let name = event.error.data.name;
          if (event.error.data.isDirectory) {
            name += '/';
          }
          switch (event.status.operationType) {
            case util.FileOperationType.COPY:
              return strf('COPY_TARGET_EXISTS_ERROR', name);
            case util.FileOperationType.MOVE:
              return strf('MOVE_TARGET_EXISTS_ERROR', name);
            case util.FileOperationType.ZIP:
              return strf('ZIP_TARGET_EXISTS_ERROR', name);
            default:
              console.warn(
                  `Unexpected operation type: ${event.status.operationType}`);
              return strf('FILE_ERROR_GENERIC');
          }

        case util.FileOperationErrorType.FILESYSTEM_ERROR:
          const detail = util.getFileErrorString(event.error.data.name);
          switch (event.status.operationType) {
            case util.FileOperationType.COPY:
              return strf('COPY_FILESYSTEM_ERROR', detail);
            case util.FileOperationType.MOVE:
              return strf('MOVE_FILESYSTEM_ERROR', detail);
            case util.FileOperationType.ZIP:
              return strf('ZIP_FILESYSTEM_ERROR', detail);
            case util.FileOperationType.DELETE:
              return str('DELETE_ERROR');
            case util.FileOperationType.RESTORE:
            case util.FileOperationType.RESTORE_TO_DESTINATION:
              return str('RESTORE_FROM_TRASH_ERROR');
            default:
              console.warn(
                  `Unexpected operation type: ${event.status.operationType}`);
              return strf('FILE_ERROR_GENERIC');
          }

        default:
          switch (event.status.operationType) {
            case util.FileOperationType.COPY:
              return strf('COPY_UNEXPECTED_ERROR', event.error.code);
            case util.FileOperationType.MOVE:
              return strf('MOVE_UNEXPECTED_ERROR', event.error.code);
            case util.FileOperationType.ZIP:
              return strf('ZIP_UNEXPECTED_ERROR', event.error.code);
            case util.FileOperationType.DELETE:
              return str('DELETE_ERROR');
            case util.FileOperationType.RESTORE:
            case util.FileOperationType.RESTORE_TO_DESTINATION:
              return str('RESTORE_FROM_TRASH_ERROR');
            default:
              console.warn(
                  `Unexpected operation type: ${event.status.operationType}`);
              return strf('FILE_ERROR_GENERIC');
          }
      }
    } else if (event.status.numRemainingItems === 1) {
      const name = event.status.processingEntryName;
      switch (event.status.operationType) {
        case util.FileOperationType.COPY:
          return strf('COPY_FILE_NAME', name);
        case util.FileOperationType.MOVE:
          return strf('MOVE_FILE_NAME', name);
        case util.FileOperationType.ZIP:
          return strf('ZIP_FILE_NAME', name);
        case util.FileOperationType.DELETE:
          return strf('DELETE_FILE_NAME', name);
        case util.FileOperationType.RESTORE:
        case util.FileOperationType.RESTORE_TO_DESTINATION:
          return strf('RESTORE_FROM_TRASH_FILE_NAME', name);
        default:
          console.warn(
              `Unexpected operation type: ${event.status.operationType}`);
          return strf('FILE_ERROR_GENERIC');
      }
    } else {
      const remainNumber = event.status.numRemainingItems;
      switch (event.status.operationType) {
        case util.FileOperationType.COPY:
          return strf('COPY_ITEMS_REMAINING', remainNumber);
        case util.FileOperationType.MOVE:
          return strf('MOVE_ITEMS_REMAINING', remainNumber);
        case util.FileOperationType.ZIP:
          return strf('ZIP_ITEMS_REMAINING', remainNumber);
        case util.FileOperationType.DELETE:
          return strf('DELETE_ITEMS_REMAINING', remainNumber);
        case util.FileOperationType.RESTORE:
        case util.FileOperationType.RESTORE_TO_DESTINATION:
          return strf('RESTORE_FROM_TRASH_ITEMS_REMAINING', remainNumber);
        default:
          console.warn(
              `Unexpected operation type: ${event.status.operationType}`);
          return strf('FILE_ERROR_GENERIC');
      }
    }
  }

  /**
   * Obtains ProgressItemType from OperationType of FileTransferManager.
   * @param {string} operationType OperationType of FileTransferManager.
   * @return {ProgressItemType} corresponding to the specified operation type.
   * @private
   */
  static getType_(operationType) {
    switch (operationType) {
      case util.FileOperationType.COPY:
        return ProgressItemType.COPY;
      case util.FileOperationType.MOVE:
        return ProgressItemType.MOVE;
      case util.FileOperationType.ZIP:
        return ProgressItemType.ZIP;
      default:
        console.error('Unknown operation type.');
        return ProgressItemType.TRANSFER;
    }
  }
}

/**
 * Pending time before a delete item is added to the progress center.
 *
 * @type {number}
 * @const
 * @private
 */
FileOperationHandler.PENDING_TIME_MS_ = 500;

/**
 * Obtains ProgressItemType from OperationType of ProgressStatus.type.
 * @param {!chrome.fileManagerPrivate.IOTaskType} type Operation type
 *     ProgressStatus.
 * @return {!ProgressItemType} corresponding to the
 *     specified operation type.
 * @private
 */
function getTypeFromIOTaskType_(type) {
  switch (type) {
    case chrome.fileManagerPrivate.IOTaskType.COPY:
      return ProgressItemType.COPY;
    case chrome.fileManagerPrivate.IOTaskType.DELETE:
      return ProgressItemType.DELETE;
    case chrome.fileManagerPrivate.IOTaskType.EMPTY_TRASH:
      return ProgressItemType.EMPTY_TRASH;
    case chrome.fileManagerPrivate.IOTaskType.EXTRACT:
      return ProgressItemType.EXTRACT;
    case chrome.fileManagerPrivate.IOTaskType.MOVE:
      return ProgressItemType.MOVE;
    case chrome.fileManagerPrivate.IOTaskType.RESTORE:
      return ProgressItemType.RESTORE;
    case chrome.fileManagerPrivate.IOTaskType.RESTORE_TO_DESTINATION:
      return ProgressItemType.RESTORE_TO_DESTINATION;
    case chrome.fileManagerPrivate.IOTaskType.TRASH:
      return ProgressItemType.TRASH;
    case chrome.fileManagerPrivate.IOTaskType.ZIP:
      return ProgressItemType.ZIP;
    default:
      console.error('Unknown operation type: ' + type);
      return ProgressItemType.TRANSFER;
  }
}

/**
 * Generate a progress message from the event.
 * @param {!chrome.fileManagerPrivate.ProgressStatus} event Progress event.
 * @return {string} message.
 * @private
 */
function getMessageFromProgressEvent_(event) {
  // All the non-error states text is managed directly in the
  // ProgressCenterPanel.
  if (event.state === chrome.fileManagerPrivate.IOTaskState.ERROR) {
    const detail = util.getFileErrorString(event.errorName);
    switch (event.type) {
      case chrome.fileManagerPrivate.IOTaskType.COPY:
        return strf('COPY_FILESYSTEM_ERROR', detail);
      case chrome.fileManagerPrivate.IOTaskType.EMPTY_TRASH:
        return str('EMPTY_TRASH_UNEXPECTED_ERROR');
      case chrome.fileManagerPrivate.IOTaskType.EXTRACT:
        return strf('EXTRACT_FILESYSTEM_ERROR', detail);
      case chrome.fileManagerPrivate.IOTaskType.MOVE:
        return strf('MOVE_FILESYSTEM_ERROR', detail);
      case chrome.fileManagerPrivate.IOTaskType.ZIP:
        return strf('ZIP_FILESYSTEM_ERROR', detail);
      case chrome.fileManagerPrivate.IOTaskType.DELETE:
        return str('DELETE_ERROR');
      case chrome.fileManagerPrivate.IOTaskType.RESTORE:
      case chrome.fileManagerPrivate.IOTaskType.RESTORE_TO_DESTINATION:
        return str('RESTORE_FROM_TRASH_ERROR');
      case chrome.fileManagerPrivate.IOTaskType.TRASH:
        return str('TRASH_UNEXPECTED_ERROR');
      default:
        console.warn(`Unexpected operation type: ${event.type}`);
        return strf('FILE_ERROR_GENERIC');
    }
  }

  return '';
}
