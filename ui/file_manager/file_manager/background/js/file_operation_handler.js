// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {startIOTask} from '../../common/js/api.js';
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
      case chrome.fileManagerPrivate.IOTaskState.PAUSED:
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
            /** @type {!Array<!Entry>} */
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
        break;
      case chrome.fileManagerPrivate.IOTaskState.NEED_PASSWORD:
        // Set state to canceled so notification doesn't display.
        item.state = ProgressItemState.CANCELED;
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
    /** @type {!Array<!Entry>} */
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
