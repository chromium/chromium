// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {startIOTask} from '../../common/js/api.js';
import {PolicyErrorType, ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {getFileErrorString, str, strf} from '../../common/js/translations.js';
import {checkAPIError, visitURL} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {getStore} from '../../state/store.js';

/**
 * An event handler of the background page for file operations.
 */
export class FileOperationHandler {
  constructor(private progressCenter_: ProgressCenter) {
    chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(
        this.onIoTaskProgressStatus_.bind(this));
  }

  /**
   * Process the IO Task ProgressStatus events.
   */
  private onIoTaskProgressStatus_(
      event: chrome.fileManagerPrivate.ProgressStatus) {
    const taskId = String(event.taskId);
    let newItem = false;
    let item = this.progressCenter_.getItemById(taskId);
    if (!item) {
      item = new ProgressCenterItem();
      newItem = true;
      item.id = taskId;
      item.type = getTypeFromIOTaskType(event.type);
      item.itemCount = event.itemCount;
      const state = getStore().getState();
      const volume = state.volumes[event.destinationVolumeId];
      item.isDestinationDrive =
          volume?.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
      item.cancelCallback = () => {
        chrome.fileManagerPrivate.cancelIOTask(event.taskId);
      };
    }
    item.message = getMessageFromProgressEvent(event);
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
          const extraButtonText = getPolicyExtraButtonText(event);
          if (event.pauseParams.policyParams.policyFileCount === 1 &&
              !event.pauseParams.policyParams.alwaysShowReview) {
            item.setExtraButton(
                ProgressItemState.PAUSED, extraButtonText, () => {
                  // Proceed/cancel the action directly from the notification.
                  chrome.fileManagerPrivate.resumeIOTask(event.taskId, {
                    policyParams: {type: event.pauseParams!.policyParams!.type},
                    conflictParams: undefined,
                  });
                });
          } else {
            item.setExtraButton(
                ProgressItemState.PAUSED, extraButtonText, () => {
                  // Show the dialog to proceed/cancel.
                  chrome.fileManagerPrivate.showPolicyDialog(
                      event.taskId,
                      chrome.fileManagerPrivate.PolicyDialogType.WARNING,
                      checkAPIError);
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
            const infoEntries: Entry[] =
                (event.outputs ||
                 []).filter((o: Entry) => o.name.endsWith('.trashinfo'));
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
                getPolicyErrorFromIOTaskPolicyError(event.policyError.type);
            item.policyFileCount = event.policyError.policyFileCount;
            item.policyFileName = event.policyError.fileName;
            item.dismissCallback = () => {
              // For policy errors, we keep track of the task's info since it
              // might be required to review the details. Notify when dismissed
              // that this can be cleared.
              chrome.fileManagerPrivate.dismissIOTask(
                  event.taskId, checkAPIError);
            };
            const extraButtonText = getPolicyExtraButtonText(event);
            if (event.policyError.type !==
                    PolicyErrorType.DLP_WARNING_TIMEOUT &&
                (event.policyError.policyFileCount > 1 ||
                 event.policyError.alwaysShowReview)) {
              item.setExtraButton(
                  ProgressItemState.ERROR, extraButtonText, () => {
                    chrome.fileManagerPrivate.showPolicyDialog(
                        event.taskId,
                        chrome.fileManagerPrivate.PolicyDialogType.ERROR,
                        checkAPIError);
                  });
            } else {
              item.setExtraButton(
                  ProgressItemState.ERROR, extraButtonText, () => {
                    visitURL(str('DLP_HELP_URL'));
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
}

/**
 * Obtains ProgressItemType from OperationType of ProgressStatus.type.
 */
function getTypeFromIOTaskType(type: chrome.fileManagerPrivate.IOTaskType):
    ProgressItemType {
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
 */
function getMessageFromProgressEvent(
    event: chrome.fileManagerPrivate.ProgressStatus): string {
  // The non-error states text is managed directly in the
  // ProgressCenterPanel.
  if (event.state !== chrome.fileManagerPrivate.IOTaskState.ERROR) {
    return '';
  }
  // TODO(b/295438773): Remove this special case for the "in use" error once
  // the files app error strings are made consistent and an "in use" string is
  // properly added.
  if (event.errorName == 'InUseError' && event.itemCount == 1) {
    switch (event.type) {
      case chrome.fileManagerPrivate.IOTaskType.MOVE:
        return str('MOVE_IN_USE_ERROR');
      case chrome.fileManagerPrivate.IOTaskType.DELETE:
        return str('DELETE_IN_USE_ERROR');
    }
  }
  const detail = getFileErrorString(event.errorName);
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
      return str('FILE_ERROR_GENERIC');
  }
}

/**
 * Converts fileManagerPrivate.PolicyErrorType to
 * ProgressCenterItem.PolicyErrorType.
 */
function getPolicyErrorFromIOTaskPolicyError(
    error?: chrome.fileManagerPrivate.PolicyErrorType): PolicyErrorType|null {
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
 */
function getPolicyExtraButtonText(
    event: chrome.fileManagerPrivate.ProgressStatus): string {
  if (event.state === chrome.fileManagerPrivate.IOTaskState.PAUSED &&
      event.pauseParams && event.pauseParams.policyParams) {
    if (event.pauseParams.policyParams.policyFileCount > 1 ||
        event.pauseParams.policyParams.alwaysShowReview) {
      return str('DLP_FILES_REVIEW_BUTTON');
    }
    // Single item:
    switch (event.type) {
      case chrome.fileManagerPrivate.IOTaskType.COPY:
        return str('DLP_FILES_COPY_WARN_CONTINUE_BUTTON');
      case chrome.fileManagerPrivate.IOTaskType.MOVE:
      case chrome.fileManagerPrivate.IOTaskType.RESTORE_TO_DESTINATION:
        return str('DLP_FILES_MOVE_WARN_CONTINUE_BUTTON');
      default:
        console.error('Unexpected operation type: ' + event.type);
        return '';
    }
  }
  if (event.state === chrome.fileManagerPrivate.IOTaskState.ERROR &&
      event.policyError) {
    if (event.policyError.type !== PolicyErrorType.DLP_WARNING_TIMEOUT &&
        (event.policyError.policyFileCount > 1 ||
         event.policyError.alwaysShowReview)) {
      return str('DLP_FILES_REVIEW_BUTTON');
    } else {
      return str('LEARN_MORE_LABEL');
    }
  }
  return '';
}
