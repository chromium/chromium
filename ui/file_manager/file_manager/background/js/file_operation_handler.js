// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An event handler of the background page for file operations.
 */
class FileOperationHandler {
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
  }

  /**
   * Handles the copy-progress event.
   * @param {Event} event The copy-progress event.
   * @private
   */
  onCopyProgress_(event) {
    const EventType = fileOperationUtil.EventRouter.EventType;
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
   * Handles the delete event.
   * @param {Event} event The delete event.
   * @private
   */
  onDeleteProgress_(event) {
    const EventType = fileOperationUtil.EventRouter.EventType;
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
        item.message = FileOperationHandler.getDeleteMessage_(event);
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
        item.message = FileOperationHandler.getDeleteMessage_(event);
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
        item.message = FileOperationHandler.getDeleteMessage_(event);
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
    if (event.reason === fileOperationUtil.EventRouter.EventType.ERROR) {
      switch (event.error.code) {
        case util.FileOperationErrorType.TARGET_EXISTS:
          var name = event.error.data.name;
          if (event.error.data.isDirectory) {
            name += '/';
          }
          switch (event.status.operationType) {
            case 'COPY':
              return strf('COPY_TARGET_EXISTS_ERROR', name);
            case 'MOVE':
              return strf('MOVE_TARGET_EXISTS_ERROR', name);
            case 'ZIP':
              return strf('ZIP_TARGET_EXISTS_ERROR', name);
            default:
              return strf('TRANSFER_TARGET_EXISTS_ERROR', name);
          }

        case util.FileOperationErrorType.FILESYSTEM_ERROR:
          const detail = util.getFileErrorString(event.error.data.name);
          switch (event.status.operationType) {
            case 'COPY':
              return strf('COPY_FILESYSTEM_ERROR', detail);
            case 'MOVE':
              return strf('MOVE_FILESYSTEM_ERROR', detail);
            case 'ZIP':
              return strf('ZIP_FILESYSTEM_ERROR', detail);
            default:
              return strf('TRANSFER_FILESYSTEM_ERROR', detail);
          }

        default:
          switch (event.status.operationType) {
            case 'COPY':
              return strf('COPY_UNEXPECTED_ERROR', event.error.code);
            case 'MOVE':
              return strf('MOVE_UNEXPECTED_ERROR', event.error.code);
            case 'ZIP':
              return strf('ZIP_UNEXPECTED_ERROR', event.error.code);
            default:
              return strf('TRANSFER_UNEXPECTED_ERROR', event.error.code);
          }
      }
    } else if (event.status.numRemainingItems === 1) {
      var name = event.status.processingEntryName;
      switch (event.status.operationType) {
        case 'COPY':
          return strf('COPY_FILE_NAME', name);
        case 'MOVE':
          return strf('MOVE_FILE_NAME', name);
        case 'ZIP':
          return strf('ZIP_FILE_NAME', name);
        default:
          return strf('TRANSFER_FILE_NAME', name);
      }
    } else {
      const remainNumber = event.status.numRemainingItems;
      switch (event.status.operationType) {
        case 'COPY':
          return strf('COPY_ITEMS_REMAINING', remainNumber);
        case 'MOVE':
          return strf('MOVE_ITEMS_REMAINING', remainNumber);
        case 'ZIP':
          return strf('ZIP_ITEMS_REMAINING', remainNumber);
        default:
          return strf('TRANSFER_ITEMS_REMAINING', remainNumber);
      }
    }
  }

  /**
   * Generates a delete message from the event.
   * @param {Event} event Progress event.
   * @return {string} message.
   * @private
   */
  static getDeleteMessage_(event) {
    event = /** @type {FileOperationProgressEvent} */ (event);
    if (event.reason === fileOperationUtil.EventRouter.EventType.ERROR) {
      return str('DELETE_ERROR');
    } else if (event.entries.length == 1) {
      const fileName = event.entries[0].name;
      return strf('DELETE_FILE_NAME', fileName);
    } else if (event.entries.length > 1) {
      return strf('DELETE_ITEMS_REMAINING', event.entries.length);
    } else {
      return '';
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
      case 'COPY':
        return ProgressItemType.COPY;
      case 'MOVE':
        return ProgressItemType.MOVE;
      case 'ZIP':
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
