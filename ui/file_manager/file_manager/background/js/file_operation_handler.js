// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {startIOTask} from '../../common/js/api.js';
import {PolicyErrorType, ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {State} from '../../externs/ts/state.js';
import {getStore} from '../../state/store.js';

/**
 * An event handler of the background page for file operations.
 */
export class FileOperationHandler {
  /**
   * @param {!ProgressCenter} progressCenter
   */
  constructor(progressCenter) {

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
      const state = /** @type {State} */ (getStore().getState());
      const volume = state.volumes[event.destinationVolumeId];
      item.isDestinationDrive =
          volume?.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
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
        // For scanning, the progress is the percentage of scanned items out of
        // the total count.
        item.progressMax = event.itemCount;
        item.progressValue = event.sourcesScanned;
        item.remainingTime = event.remainingSeconds;
        break;
      case chrome.fileManagerPrivate.IOTaskState.PAUSED:
        // Check if the task is paused because of warning level restrictions.
        if (event.pauseParams && event.pauseParams.policyParams) {
          item.state = ProgressItemState.PAUSED;
          item.policyFileCount = event.pauseParams.policyParams.policyFileCount;
          item.policyFileName = event.pauseParams.policyParams.fileName;
          const extraButtonText = getPolicyExtraButtonText_(event);
          if (event.pauseParams.policyParams.policyFileCount === 1) {
            item.setExtraButton(
                ProgressItemState.PAUSED, extraButtonText, () => {
                  // Proceed/cancel the action directly from the notification.
                  chrome.fileManagerPrivate.resumeIOTask(event.taskId, {
                    policyParams: {type: event.pauseParams.policyParams.type},
                    conflictParams: undefined,
                  });
                });
          } else {
            item.setExtraButton(
                ProgressItemState.PAUSED, extraButtonText, () => {
                  // Show the dialog to proceed/cancel.
                  chrome.fileManagerPrivate.showPolicyDialog(
                      event.taskId,
                      chrome.fileManagerPrivate.PolicyDialogType.WARNING);
                });
          }
          break;
        }
        // Otherwise same is in-progress - fall through
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
        } else {  // ERROR
          item.state = ProgressItemState.ERROR;
          // Check if there was a policy error.
          if (event.policyError) {
            item.policyError =
                getPolicyErrorFromIOTaskPolicyError_(event.policyError.type);
            item.policyFileCount = event.policyError.policyFileCount;
            item.policyFileName = event.policyError.fileName;
            const extraButtonText = getPolicyExtraButtonText_(event);
            if (event.policyError.type !==
                    PolicyErrorType.DLP_WARNING_TIMEOUT &&
                event.policyError.policyFileCount > 1) {
              item.setExtraButton(
                  ProgressItemState.ERROR, extraButtonText, () => {
                    chrome.fileManagerPrivate.showPolicyDialog(
                        event.taskId,
                        chrome.fileManagerPrivate.PolicyDialogType.ERROR);
                  });
            } else {
              item.setExtraButton(
                  ProgressItemState.ERROR, extraButtonText, () => {
                    util.visitURL(str('DLP_HELP_URL'));
                  });
            }
          }
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

/**
 * Converts fileManagerPrivate.PolicyErrorType to
 * ProgressCenterItem.PolicyErrorType.
 * @param {!chrome.fileManagerPrivate.PolicyErrorType|undefined} error
 * @return {?PolicyErrorType} corresponding to the error type, or null if not
 *     defined.
 * @private
 */
function getPolicyErrorFromIOTaskPolicyError_(error) {
  if (!error) {
    return null;
  }
  switch (error) {
    case chrome.fileManagerPrivate.PolicyErrorType.DLP:
      return PolicyErrorType.DLP;
    case chrome.fileManagerPrivate.PolicyErrorType.ENTERPRISE_CONNECTORS:
      return PolicyErrorType.ENTERPRISE_CONNECTORS;
    case chrome.fileManagerPrivate.PolicyErrorType.DLP_WARNING_TIMEOUT:
      return PolicyErrorType.DLP_WARNING_TIMEOUT;
    default:
      console.warn(`Unexpected policy error type: ${error}`);
      return null;
  }
}

/**
 * Returns the extra button text for policy panel items. Currently only
 * supported for PAUSED and ERROR states due to policy, and for COPY or MOVE
 * operation types.
 * @param {!chrome.fileManagerPrivate.ProgressStatus} event
 * @return {string} button text or empty string if not applicable.
 * @private
 */
function getPolicyExtraButtonText_(event) {
  if (event.state === chrome.fileManagerPrivate.IOTaskState.PAUSED &&
      event.pauseParams && event.pauseParams.policyParams) {
    if (event.pauseParams.policyParams.policyFileCount > 1) {
      return str('DLP_FILES_REVIEW_BUTTON');
    }
    // Single item:
    switch (event.type) {
      case chrome.fileManagerPrivate.IOTaskType.COPY:
        return str('DLP_FILES_COPY_WARN_CONTINUE_BUTTON');
      case chrome.fileManagerPrivate.IOTaskType.MOVE:
        return str('DLP_FILES_MOVE_WARN_CONTINUE_BUTTON');
      default:
        console.error('Unexpected operation type: ' + event.type);
        return '';
    }
  }
  if (event.state === chrome.fileManagerPrivate.IOTaskState.ERROR &&
      event.policyError) {
    if (event.policyError.type !== PolicyErrorType.DLP_WARNING_TIMEOUT &&
        event.policyError.policyFileCount > 1) {
      return str('DLP_FILES_REVIEW_BUTTON');
    } else {
      return str('LEARN_MORE_LABEL');
    }
  }
  return '';
}
