// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {AsyncQueue, RateLimiter} from '../../common/js/async_util.js';
import {notifications} from '../../common/js/notifications.js';
import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {getFilesAppIconURL, toFilesAppURL} from '../../common/js/url_constants.js';
import {str, strf, util} from '../../common/js/util.js';
import {DriveSyncHandler} from '../../externs/background/drive_sync_handler.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {DriveDialogControllerInterface} from '../../externs/drive_dialog_controller.js';
import {MetadataModelInterface} from '../../externs/metadata_model.js';

import {fileOperationUtil} from './file_operation_util.js';

/**
 * Keys in the metadata store related to individual sync status.
 * @const {!Array<!chrome.fileManagerPrivate.EntryPropertyName>}
 */
const METADATA_KEYS = [
  chrome.fileManagerPrivate.EntryPropertyName.SYNC_STATUS,
  chrome.fileManagerPrivate.EntryPropertyName.PROGRESS,
];

/**
 * @const {!chrome.fileManagerPrivate.EntryPropertyName}
 */
const SYNC_STATUS = chrome.fileManagerPrivate.EntryPropertyName.SYNC_STATUS;

/**
 * @const {!chrome.fileManagerPrivate.SyncStatus}
 */
const COMPLETED = chrome.fileManagerPrivate.SyncStatus.COMPLETED;

/**
 * Handler of the background page for the Drive sync events.
 * @implements {DriveSyncHandler}
 */
export class DriveSyncHandlerImpl extends EventTarget {
  /** @param {ProgressCenter} progressCenter */
  constructor(progressCenter) {
    super();

    /**
     * Progress center to submit the progressing item.
     * @type {ProgressCenter}
     * @const
     * @private
     */
    this.progressCenter_ = progressCenter;

    /**
     * The metadata model to notify entries have changed.
     * @type {?MetadataModelInterface}
     * @private
     */
    this.metadataModel_ = null;

    /**
     * Predefined error ID for out of quota messages.
     * @type {number}
     * @const
     * @private
     */
    this.driveErrorIdOutOfQuota_ = 1;

    /**
     * Predefined error ID for shared drive out of storage messages.
     * @type {number}
     * @const
     * @private
     */
    this.driveErrorIdSharedDriveNoStorage_ = 2;

    /**
     * Maximum reserved ID for predefined errors.
     * @type {number}
     * @const
     * @private
     */
    this.driveErrorIdMax_ = this.driveErrorIdSharedDriveNoStorage_;

    /**
     * Counter for error ID.
     * @type {number}
     * @private
     */
    this.errorIdCounter_ = this.driveErrorIdMax_ + 1;

    /**
     * Progress center item for sync status.
     * @type {ProgressCenterItem}
     * @const
     * @private
     */
    this.syncItem_ = new ProgressCenterItem();
    this.syncItem_.id = 'drive-sync';
    // Set to canceled so that it starts out hidden when sent to ProgressCenter.
    this.syncItem_.state = ProgressItemState.CANCELED;

    /**
     * Progress center item for pinning status.
     * @type {ProgressCenterItem}
     * @const
     * @private
     */
    this.pinItem_ = new ProgressCenterItem();
    this.pinItem_.id = 'drive-pin';
    // Set to canceled so that it starts out hidden when sent to ProgressCenter.
    this.pinItem_.state = ProgressItemState.CANCELED;

    /**
     * If the property is true, this item is syncing.
     * @type {boolean}
     * @private
     */
    this.syncing_ = false;

    /**
     * Whether the sync is disabled on cellular network or not.
     * @type {boolean}
     * @private
     */
    this.cellularDisabled_ = false;

    /**
     * Async queue.
     * @type {AsyncQueue}
     * @const
     * @private
     */
    this.queue_ = new AsyncQueue();

    /**
     * The length average window in calculating moving average speed of task.
     * @private {number}
     */
    this.SPEED_BUFFER_WINDOW_ = 30;

    /**
     * Speedometers to track speed and remaining time of sync.
     * @const {Object<string, fileOperationUtil.Speedometer>}
     * @private
     */
    this.speedometers_ = {
      [this.syncItem_.id]:
          new fileOperationUtil.Speedometer(this.SPEED_BUFFER_WINDOW_),
      [this.pinItem_.id]:
          new fileOperationUtil.Speedometer(this.SPEED_BUFFER_WINDOW_),
    };
    Object.freeze(this.speedometers_);

    /**
     * Drive sync messages for each id.
     * @const {Object<string, {single: string, plural: string}>}
     * @private
     */
    this.statusMessages_ = {
      [this.syncItem_.id]:
          {single: 'SYNC_FILE_NAME', plural: 'SYNC_FILE_NUMBER'},
      [this.pinItem_.id]: {
        single: 'OFFLINE_PROGRESS_MESSAGE',
        plural: 'OFFLINE_PROGRESS_MESSAGE_PLURAL',
      },
    };
    Object.freeze(this.statusMessages_);

    /**
     * Rate limiter which is used to avoid sending update request for progress
     * bar too frequently.
     * @private {RateLimiter}
     */
    this.progressRateLimiter_ = new RateLimiter(() => {
      this.progressCenter_.updateItem(this.syncItem_);
      this.progressCenter_.updateItem(this.pinItem_);
    }, 2000);

    /**
     * Saved dialog event to be sent to the next launched Files App UI window.
     * @private {?chrome.fileManagerPrivate.DriveConfirmDialogEvent}
     */
    this.savedDialogEvent_ = null;

    /**
     * A mapping of App IDs to dialogs.
     * @private {Map<string, DriveDialogControllerInterface>}
     */
    this.dialogs_ = new Map();

    // Register events.
    if (util.isInlineSyncStatusEnabled()) {
      chrome.fileManagerPrivate.onIndividualFileTransfersUpdated.addListener(
          this.updateSyncStateMetadata_.bind(this));
    } else {
      chrome.fileManagerPrivate.onFileTransfersUpdated.addListener(
          this.onFileTransfersStatusReceived_.bind(this, this.syncItem_));
      chrome.fileManagerPrivate.onPinTransfersUpdated.addListener(
          this.onFileTransfersStatusReceived_.bind(this, this.pinItem_));
    }
    chrome.fileManagerPrivate.onDriveSyncError.addListener(
        this.onDriveSyncError_.bind(this));
    notifications.onButtonClicked.addListener(
        this.onNotificationButtonClicked_.bind(this));
    notifications.onClosed.addListener(this.onNotificationClosed_.bind(this));
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
        this.onDriveConnectionStatusChanged_.bind(this));
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));

    // Set initial values.
    this.onPreferencesChanged_();
  }

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {
    return this.syncing_;
  }

  /**
   * @param {!MetadataModelInterface} model
   */
  set metadataModel(model) {
    this.metadataModel_ = model;
  }

  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {
    return DriveSyncHandlerImpl.DRIVE_SYNC_COMPLETED_EVENT;
  }

  /**
   * Returns whether the Drive sync is currently suppressed or not.
   * @return {boolean}
   */
  isSyncSuppressed() {
    return navigator.connection.type === 'cellular' && this.cellularDisabled_;
  }

  /**
   * Shows a notification that Drive sync is disabled on cellular networks.
   */
  showDisabledMobileSyncNotification() {
    notifications.create(
        DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_, {
          type: 'basic',
          title: str('FILEMANAGER_APP_NAME'),
          message: str('DISABLED_MOBILE_SYNC_NOTIFICATION_MESSAGE'),
          iconUrl: getFilesAppIconURL().toString(),
          buttons:
              [{title: str('DISABLED_MOBILE_SYNC_NOTIFICATION_ENABLE_BUTTON')}],
        },
        () => {});
  }

  /**
   * Handles file transfer status updates and updates the given item
   * accordingly.
   * @param {ProgressCenterItem} item Item to update.
   * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer
   *     status.
   * @private
   */
  async onFileTransfersStatusReceived_(item, status) {
    if (!this.isProcessableEvent(status)) {
      return;
    }
    if (!status.showNotification) {
      // Hide the notification by settings its state to Canceled.
      item.state = ProgressItemState.CANCELED;
      this.progressCenter_.updateItem(item);
      return;
    }

    switch (status.transferState) {
      case 'in_progress':
        await this.updateItem_(item, status);
        break;
      case 'queued':
      case 'completed':
      case 'failed':
        if ((status.hideWhenZeroJobs && status.numTotalJobs === 0) ||
            (!status.hideWhenZeroJobs && status.numTotalJobs === 1)) {
          await this.removeItem_(item, status);
        }
        break;
      default:
        throw new Error(
            'Invalid transfer state: ' + status.transferState + '.');
    }
  }

  /**
   * Handles file transfer status updates for individual files, updating their
   * sync status metadata.
   * @param {!Array<!chrome.fileManagerPrivate.SyncState>}
   *     syncStates Updated file transfer statuses.
   * @private
   */
  updateSyncStateMetadata_(syncStates) {
    if (!this.metadataModel_) {
      // Files app is still loading. This should have no user visible impact
      // since sync status update events are constantly emitted.
      return;
    }

    const completedUrls = [];
    const valuesToUpdate = [];
    const urlsToUpdate = [];

    for (const {fileUrl, syncStatus, progress} of syncStates) {
      valuesToUpdate.push([syncStatus, progress]);
      urlsToUpdate.push(fileUrl);

      if (syncStatus === COMPLETED) {
        completedUrls.push(fileUrl);
      }
    }

    this.metadataModel_.update(urlsToUpdate, METADATA_KEYS, valuesToUpdate);

    // Update filtered states that are completed now and, in 300ms, are still
    // completed (i.e., haven't started syncing again) to "not_found".
    if (completedUrls.length > 0) {
      setTimeout(() => this.dismissCompletedEntries_(completedUrls), 300);
    }
  }

  /**
   * Updates fileUrls that are still "completed" to "not_found".
   * @param {!Array<!string>} fileUrls
   * @private
   */
  dismissCompletedEntries_(fileUrls) {
    const stillCompletedUrls = [];
    const valuesToUpdate = [];

    const metadata =
        this.metadataModel_.getCacheByUrls(fileUrls, [SYNC_STATUS]);
    for (let i = 0; i < metadata.length; i++) {
      if (metadata[i].syncStatus === COMPLETED) {
        stillCompletedUrls.push(fileUrls[i]);
        valuesToUpdate.push(
            [chrome.fileManagerPrivate.SyncStatus.NOT_FOUND, 0]);
      }
    }

    this.metadataModel_.update(
        stillCompletedUrls, METADATA_KEYS, valuesToUpdate);
  }

  /**
   * Updates the given progress status item using a transfer status update.
   * @param {ProgressCenterItem} item Item to update.
   * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer
   *     status.
   * @private
   */
  async updateItem_(item, status) {
    const unlock = await this.queue_.lock();
    try {
      item.state = ProgressItemState.PROGRESSING;
      item.type = ProgressItemType.SYNC;
      item.quiet = true;
      this.syncing_ = true;
      if (status.numTotalJobs > 1) {
        item.message =
            strf(this.statusMessages_[item.id].plural, status.numTotalJobs);
      } else {
        try {
          const entry = await util.urlToEntry(status.fileUrl);
          item.message = strf(this.statusMessages_[item.id].single, entry.name);
        } catch (error) {
          console.warn('Resolving URL ' + status.fileUrl + ' failed: ', error);
          return;
        }
      }
      item.progressValue = status.processed || 0;
      item.progressMax = status.total || 0;

      const speedometer = this.speedometers_[item.id];
      speedometer.setTotalBytes(item.progressMax);
      speedometer.update(item.progressValue);
      item.remainingTime = speedometer.getRemainingTime();

      this.progressRateLimiter_.run();
    } finally {
      unlock();
    }
  }

  /**
   * Removes an item due to the given transfer status update.
   * @param {ProgressCenterItem} item Item to remove.
   * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer
   *     status.
   * @private
   */
  async removeItem_(item, status) {
    const unlock = await this.queue_.lock();
    try {
      item.state = status.transferState === 'completed' ?
          ProgressItemState.COMPLETED :
          ProgressItemState.CANCELED;
      this.speedometers_[item.id].reset();
      this.progressCenter_.updateItem(item);
      this.syncing_ = false;
      this.dispatchEvent(new Event(this.getCompletedEventName()));
    } finally {
      unlock();
    }
  }

  /**
   * Attempts to infer of the given event is processable by the drive sync
   * handler. It uses fileUrl to make a decision. It
   * errs on the side of 'yes', when passing the judgement.
   * @param {!Object} event
   * @return {boolean} Whether or not the event should be processed.
   */
  isProcessableEvent(event) {
    const fileUrl = event.fileUrl;
    if (fileUrl) {
      return fileUrl.startsWith(`filesystem:${toFilesAppURL()}`);
    }
    return true;
  }

  /**
   * Handles drive's sync errors.
   * @param {chrome.fileManagerPrivate.DriveSyncErrorEvent} event Drive sync
   * error event.
   * @private
   */
  async onDriveSyncError_(event) {
    if (!this.isProcessableEvent(event)) {
      return;
    }
    const postError = name => {
      const item = new ProgressCenterItem();
      item.type = ProgressItemType.SYNC;
      item.quiet = true;
      item.state = ProgressItemState.ERROR;
      switch (event.type) {
        case 'delete_without_permission':
          item.message = strf('SYNC_DELETE_WITHOUT_PERMISSION_ERROR', name);
          break;
        case 'service_unavailable':
          item.message = str('SYNC_SERVICE_UNAVAILABLE_ERROR');
          break;
        case 'no_server_space':
          item.message = strf('SYNC_NO_SERVER_SPACE');
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () => util.visitURL(str('GOOGLE_DRIVE_MANAGE_STORAGE_URL')));

          // This error will reappear every time sync is retried, so we use
          // a fixed ID to avoid spamming the user.
          item.id = DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX +
              this.driveErrorIdOutOfQuota_;
          break;
        case 'no_server_space_organization':
          item.message = strf('SYNC_NO_SERVER_SPACE_ORGANIZATION');
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () => util.visitURL(str('GOOGLE_DRIVE_MANAGE_STORAGE_URL')));

          // This error will reappear every time sync is retried, so we use
          // a fixed ID to avoid spamming the user.
          item.id = DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_ORGANIZATION_PREFIX +
              this.driveErrorIdOutOfQuota_;
          break;
        case 'no_local_space':
          item.message = strf('DRIVE_OUT_OF_SPACE_HEADER', name);
          break;
        case 'no_shared_drive_space':
          item.message =
              strf('SYNC_ERROR_SHARED_DRIVE_OUT_OF_SPACE', event.sharedDrive);
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () => util.visitURL(
                  str('GOOGLE_DRIVE_ENTERPRISE_MANAGE_STORAGE_URL')));

          // Shared drives will keep trying to sync the file until it is either
          // removed or available storage is increased. This ensures each
          // subsequent error message only ever shows once for each individual
          // shared drive.
          item.id = `${DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX}${
              this.driveErrorIdSharedDriveNoStorage_}${event.sharedDrive}`;
          break;
        case 'misc':
          item.message = strf('SYNC_MISC_ERROR', name);
          break;
      }
      if (!item.id) {
        item.id = DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX +
            (this.errorIdCounter_++);
      }
      this.progressCenter_.updateItem(item);
    };

    if (!event.fileUrl) {
      postError('');
      return;
    }

    try {
      if (util.isInlineSyncStatusEnabled()) {
        this.updateSyncStateMetadata_([
          {
            fileUrl: event.fileUrl,
            syncStatus: chrome.fileManagerPrivate.SyncStatus.ERROR,
            progress: 0,
          },
        ]);
      }
      const entry = await util.urlToEntry(event.fileUrl);
      postError(entry.name);
    } catch (error) {
      postError('');
    }
  }

  /**
   * Adds a dialog to be controlled by DriveSyncHandler.
   * @param {string} appId App ID of window containing the dialog.
   * @param {DriveDialogControllerInterface} dialog Dialog to be controlled.
   */
  addDialog(appId, dialog) {
    this.dialogs_.set(appId, dialog);
    if (this.savedDialogEvent_) {
      notifications.clear(
          DriveSyncHandlerImpl.ENABLE_DOCS_OFFLINE_NOTIFICATION_ID_, () => {});
      dialog.showDialog(this.savedDialogEvent_);
      this.savedDialogEvent_ = null;
    }
  }

  /**
   * Removes a dialog from being controlled by DriveSyncHandler.
   * @param {string} appId App ID of window containing the dialog.
   */
  removeDialog(appId) {
    if (this.dialogs_.has(appId) && this.dialogs_.get(appId).open) {
      chrome.fileManagerPrivate.notifyDriveDialogResult(
          chrome.fileManagerPrivate.DriveDialogResult.DISMISS);
      this.dialogs_.delete(appId);
    }
  }

  /**
   * Handles notification's button click.
   * @param {string} notificationId Notification ID.
   * @param {number} buttonIndex Index of the button.
   * @private
   */
  onNotificationButtonClicked_(notificationId, buttonIndex) {
    switch (notificationId) {
      case DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_:
        notifications.clear(notificationId, () => {});
        chrome.fileManagerPrivate.setPreferences({cellularDisabled: false});
        break;
      case DriveSyncHandlerImpl.ENABLE_DOCS_OFFLINE_NOTIFICATION_ID_:
        notifications.clear(notificationId, () => {});
        this.savedDialogEvent_ = null;
        chrome.fileManagerPrivate.notifyDriveDialogResult(
            buttonIndex == 1 ?
                chrome.fileManagerPrivate.DriveDialogResult.ACCEPT :
                chrome.fileManagerPrivate.DriveDialogResult.REJECT);
        break;
    }
  }

  /**
   * Handles notifications being closed by user or system action.
   * @param {string} notificationId Notification ID.
   * @param {boolean} byUser True if the notification was closed by user action.
   */
  onNotificationClosed_(notificationId, byUser) {
    switch (notificationId) {
      case DriveSyncHandlerImpl.ENABLE_DOCS_OFFLINE_NOTIFICATION_ID_:
        this.savedDialogEvent_ = null;
        if (byUser) {
          chrome.fileManagerPrivate.notifyDriveDialogResult(
              chrome.fileManagerPrivate.DriveDialogResult.DISMISS);
        }
        break;
    }
  }

  /**
   * Handles preferences change.
   * @private
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(pref => {
      this.cellularDisabled_ = pref.cellularDisabled;
    });
  }

  /**
   * Handles connection state change.
   * @private
   */
  onDriveConnectionStatusChanged_() {
    chrome.fileManagerPrivate.getDriveConnectionState((state) => {
      // If offline, hide any sync progress notifications. When online again,
      // the Drive sync client may retry syncing and trigger
      // onFileTransfersUpdated events, causing it to be shown again.
      if (state.type == 'offline' && state.reason == 'no_network' &&
          this.syncing_) {
        this.syncing_ = false;
        this.syncItem_.state = ProgressItemState.CANCELED;
        this.pinItem_.state = ProgressItemState.CANCELED;
        this.progressCenter_.updateItem(this.syncItem_);
        this.progressCenter_.updateItem(this.pinItem_);
        this.dispatchEvent(new Event(this.getCompletedEventName()));
      }
    });
  }

  /**
   * Handles mount events to handle Drive mounting and unmounting.
   * @param {chrome.fileManagerPrivate.MountCompletedEvent} event Mount
   *     completed event.
   * @private
   */
  onMountCompleted_(event) {
    if (event.eventType ===
            chrome.fileManagerPrivate.MountCompletedEventType.UNMOUNT &&
        event.volumeMetadata.volumeType ===
            chrome.fileManagerPrivate.VolumeType.DRIVE) {
      notifications.clear(
          DriveSyncHandlerImpl.ENABLE_DOCS_OFFLINE_NOTIFICATION_ID_, () => {});
    }
  }
}

/**
 * Completed event name.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DRIVE_SYNC_COMPLETED_EVENT = 'completed';

/**
 * Notification ID of the disabled mobile sync notification.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_ =
    'disabled-mobile-sync';

/**
 * Notification ID of the enable Docs Offline notification.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.ENABLE_DOCS_OFFLINE_NOTIFICATION_ID_ =
    'enable-docs-offline';


/**
 * Drive sync error prefix for expired individual quotas.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX = 'drive-sync-error-';

/**
 * Drive sync error prefix for expired organization quotas.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_ORGANIZATION_PREFIX =
    'drive-sync-error-organization';
