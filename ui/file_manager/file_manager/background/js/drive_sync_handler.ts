// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles notifications supplied by drivefs.
 * Disable type checking for closure, as it is done by the typescript compiler.
 * @suppress {checkTypes}
 */

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {AsyncQueue, RateLimiter} from '../../common/js/async_util.js';
import {unwrapEntry, urlToEntry} from '../../common/js/entry_utils.js';
import {isInlineSyncStatusEnabled} from '../../common/js/flags.js';
import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {str, strf} from '../../common/js/translations.js';
import {toFilesAppURL} from '../../common/js/url_constants.js';
import {visitURL} from '../../common/js/util.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {MetadataModelInterface} from '../../externs/metadata_model.js';
import {getStore} from '../../state/store.js';

import {Speedometer} from './file_operation_util.js';

/**
 * Shorthand for metadata keys.
 */
const {
  SYNC_STATUS,
  PROGRESS,
  SYNC_COMPLETED_TIME,
  AVAILABLE_OFFLINE,
  PINNED,
  CAN_PIN,
} = chrome.fileManagerPrivate.EntryPropertyName;

/**
 * Shorthand for sync statuses.
 */
const {COMPLETED} = chrome.fileManagerPrivate.SyncStatus;

/**
 * Prefix for Out of Quota sync messages to ensure they reuse existing
 * notification messages instead of starting new ones.
 */
const enum DriveErrorId {
  OUT_OF_QUOTA = 1,
  SHARED_DRIVE_NO_STORAGE = 2,
  MAX_VALUE = SHARED_DRIVE_NO_STORAGE,
}

/**
 * The average length window in calculating moving average speed of task.
 */
const SPEED_BUFFER_WINDOW = 30;

/**
 * The completed event name.
 */
const DRIVE_SYNC_COMPLETED_EVENT = 'completed';

/**
 * A list of prefixes used to disambiguate errors that come from the same source
 * to ensure separate notifications are generated.
 */
enum ErrorPrefix {
  NORMAL = 'drive-sync-error-',
  ORGANIZATION = 'drive-sync-error-organization',
}

type FileTransferStatus = chrome.fileManagerPrivate.FileTransferStatus;
type DriveSyncErrorEvent = chrome.fileManagerPrivate.DriveSyncErrorEvent;

export class DriveSyncHandlerImpl extends EventTarget {
  private metadataModel_?: MetadataModelInterface;

  private errorIdCounter_ = DriveErrorId.MAX_VALUE + 1;

  /**
   * The static progress center item for syncing status.
   */
  private syncItem_ = new ProgressCenterItem();

  /**
   * The static progress center item for pinning status.
   */
  private pinItem_ = new ProgressCenterItem();

  /**
   * When true, this item is syncing.
   */
  private syncing_ = false;

  private queue_ = new AsyncQueue();

  /**
   * Speedometers to track speed and remaining time of sync.
   */
  private speedometers_: {[key: string]: Speedometer};

  /**
   * Drive sync messages for each id.
   */
  private statusMessages_: {[key: string]: {single: string, plural: string}};

  /**
   * Recently completed URLs whose metadata should be updated after 300ms.
   */
  private completedUrls_: string[] = [];

  /**
   * Rate limiter which is used to avoid sending update request for progress
   * bar too frequently.
   */
  private progressRateLimiter_ = new RateLimiter(() => {
    this.progressCenter_.updateItem(this.syncItem_);
    this.progressCenter_.updateItem(this.pinItem_);
  }, 2000);

  /**
   * With a rate limit of 200ms, update entries that have completed 300ms ago or
   * longer.
   */
  private updateCompletedRateLimiter_ = new RateLimiter(async () => {
    if (this.completedUrls_.length === 0) {
      return;
    }

    const entriesToUpdate: Entry[] = [];
    this.completedUrls_ = this.completedUrls_.filter(url => {
      const [entry, syncCompletedTime] =
          this.getEntryAndSyncCompletedTimeForUrl_(url);
      // Stop tracking URLs that are no longer in the store.
      if (!entry) {
        return false;
      }
      // Update URLs that have completed over 300ms and stop tracking them.
      if (Date.now() - syncCompletedTime > 300) {
        entriesToUpdate.push(entry);
        return false;
      }
      // Keep tracking URLs that are in the store and have completed <300ms ago.
      return true;
    });

    if (entriesToUpdate.length) {
      this.metadataModel_?.notifyEntriesChanged(entriesToUpdate);
      this.metadataModel_?.get(entriesToUpdate, [
        SYNC_STATUS,
        PROGRESS,
        AVAILABLE_OFFLINE,
        PINNED,
        CAN_PIN,
      ]);
    }

    this.updateCompletedRateLimiter_.run();
  }, 200);

  constructor(private progressCenter_: ProgressCenter) {
    super();

    this.syncItem_.id = 'drive-sync';
    // Set to canceled so that it starts out hidden when sent to ProgressCenter.
    this.syncItem_.state = ProgressItemState.CANCELED;

    this.pinItem_.id = 'drive-pin';
    // Set to canceled so that it starts out hidden when sent to ProgressCenter.
    this.pinItem_.state = ProgressItemState.CANCELED;

    this.speedometers_ = {
      [this.syncItem_.id]: new Speedometer(SPEED_BUFFER_WINDOW),
      [this.pinItem_.id]: new Speedometer(SPEED_BUFFER_WINDOW),
    };
    Object.freeze(this.speedometers_);

    this.statusMessages_ = {
      [this.syncItem_.id]:
          {single: 'SYNC_FILE_NAME', plural: 'SYNC_FILE_NUMBER'},
      [this.pinItem_.id]: {
        single: 'OFFLINE_PROGRESS_MESSAGE',
        plural: 'OFFLINE_PROGRESS_MESSAGE_PLURAL',
      },
    };
    Object.freeze(this.statusMessages_);

    // Register events.
    if (isInlineSyncStatusEnabled()) {
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
    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
        this.onDriveConnectionStatusChanged_.bind(this));
  }

  /**
   * Whether the handler is syncing items or not.
   */
  get syncing() {
    return this.syncing_;
  }

  /**
   * Sets the MetadataModel on the DriveSyncHandler.
   */
  set metadataModel(model: MetadataModelInterface) {
    this.metadataModel_ = model;
  }

  /**
   * Returns the completed event name.
   */
  getCompletedEventName() {
    return DRIVE_SYNC_COMPLETED_EVENT;
  }

  /**
   * Handles file transfer status updates and updates the given item
   * accordingly.
   */
  private async onFileTransfersStatusReceived_(
      item: ProgressCenterItem, status: FileTransferStatus) {
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

  private getEntryAndSyncCompletedTimeForUrl_(url: string):
      [Entry|null, number] {
    const entry = getStore().getState().allEntries[url]?.entry;

    if (!entry) {
      return [null, 0];
    }

    const metadata =
        this.metadataModel_?.getCache([entry], [SYNC_COMPLETED_TIME])[0];

    return [
      unwrapEntry(entry) as Entry,
      metadata?.syncCompletedTime || 0,
    ];
  }

  /**
   * Handles file transfer status updates for individual files, updating their
   * sync status metadata.
   */
  private async updateSyncStateMetadata_(
      syncStates: chrome.fileManagerPrivate.SyncState[]) {
    const urlsToUpdate = [];
    const valuesToUpdate = [];

    for (const {fileUrl, syncStatus, progress} of syncStates) {
      if (syncStatus !== COMPLETED) {
        urlsToUpdate.push(fileUrl);
        valuesToUpdate.push([syncStatus, progress, 0]);
        continue;
      }

      // Only update status to completed if the previous status was different.
      // Note: syncCompletedTime is 0 if the last event wasn't completed.
      if (!this.getEntryAndSyncCompletedTimeForUrl_(fileUrl)[1]) {
        urlsToUpdate.push(fileUrl);
        valuesToUpdate.push([syncStatus, progress, Date.now()]);
        this.completedUrls_.push(fileUrl);
      }
    }

    this.metadataModel_?.update(
        urlsToUpdate, [SYNC_STATUS, PROGRESS, SYNC_COMPLETED_TIME],
        valuesToUpdate);
    this.updateCompletedRateLimiter_.run();
  }

  /**
   * Updates the given progress status item using a transfer status update.
   */
  private async updateItem_(
      item: ProgressCenterItem, status: FileTransferStatus) {
    const unlock = await this.queue_.lock();
    try {
      item.state = ProgressItemState.PROGRESSING;
      item.type = ProgressItemType.SYNC;
      item.quiet = true;
      this.syncing_ = true;
      if (status.numTotalJobs > 1) {
        item.message =
            strf(this.statusMessages_[item.id]!.plural, status.numTotalJobs);
      } else {
        try {
          const entry = await urlToEntry(status.fileUrl);
          item.message =
              strf(this.statusMessages_[item.id]!.single, entry.name);
        } catch (error) {
          console.warn('Resolving URL ' + status.fileUrl + ' failed: ', error);
          return;
        }
      }
      item.progressValue = status.processed || 0;
      item.progressMax = status.total || 0;

      const speedometer = this.speedometers_[item.id];
      if (speedometer) {
        speedometer.setTotalBytes(item.progressMax);
        speedometer.update(item.progressValue);
        item.remainingTime = speedometer.getRemainingTime();
      }

      this.progressRateLimiter_.run();
    } finally {
      unlock();
    }
  }

  /**
   * Removes an item due to the given transfer status update.
   */
  private async removeItem_(
      item: ProgressCenterItem, status: FileTransferStatus) {
    const unlock = await this.queue_.lock();
    try {
      item.state = status.transferState === 'completed' ?
          ProgressItemState.COMPLETED :
          ProgressItemState.CANCELED;
      this.speedometers_[item.id]!.reset();
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
   */
  isProcessableEvent(event: (FileTransferStatus|DriveSyncErrorEvent)) {
    const fileUrl = event.fileUrl;
    if (fileUrl) {
      return fileUrl.startsWith(`filesystem:${toFilesAppURL()}`);
    }
    return true;
  }

  /**
   * Handles drive's sync errors.
   */
  private async onDriveSyncError_(event: DriveSyncErrorEvent) {
    if (!this.isProcessableEvent(event)) {
      return;
    }
    const postError = (name: string) => {
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
          item.message = str('SYNC_NO_SERVER_SPACE');
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () => visitURL(str('GOOGLE_DRIVE_MANAGE_STORAGE_URL')));

          // This error will reappear every time sync is retried, so we use
          // a fixed ID to avoid spamming the user.
          item.id = ErrorPrefix.NORMAL + DriveErrorId.OUT_OF_QUOTA;
          break;
        case 'no_server_space_organization':
          item.message = str('SYNC_NO_SERVER_SPACE_ORGANIZATION');
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () => visitURL(str('GOOGLE_DRIVE_MANAGE_STORAGE_URL')));

          // This error will reappear every time sync is retried, so we use
          // a fixed ID to avoid spamming the user.
          item.id = ErrorPrefix.ORGANIZATION + DriveErrorId.OUT_OF_QUOTA;
          break;
        case 'no_local_space':
          item.message = strf('DRIVE_OUT_OF_SPACE_HEADER', name);
          break;
        case 'no_shared_drive_space':
          item.message =
              strf('SYNC_ERROR_SHARED_DRIVE_OUT_OF_SPACE', event.sharedDrive);
          item.setExtraButton(
              ProgressItemState.ERROR, str('LEARN_MORE_LABEL'),
              () =>
                  visitURL(str('GOOGLE_DRIVE_ENTERPRISE_MANAGE_STORAGE_URL')));

          // Shared drives will keep trying to sync the file until it is either
          // removed or available storage is increased. This ensures each
          // subsequent error message only ever shows once for each individual
          // shared drive.
          item.id = `${ErrorPrefix.NORMAL}${
              DriveErrorId.SHARED_DRIVE_NO_STORAGE}${event.sharedDrive}`;
          break;
        case 'misc':
          item.message = strf('SYNC_MISC_ERROR', name);
          break;
      }
      if (!item.id) {
        item.id = ErrorPrefix.NORMAL + (this.errorIdCounter_++);
      }
      this.progressCenter_.updateItem(item);
    };

    if (!event.fileUrl) {
      postError('');
      return;
    }

    try {
      if (isInlineSyncStatusEnabled()) {
        this.updateSyncStateMetadata_([
          {
            fileUrl: event.fileUrl,
            syncStatus: chrome.fileManagerPrivate.SyncStatus.QUEUED,
            progress: 0,
          },
        ]);
      }
      const entry = await urlToEntry(event.fileUrl);
      postError(entry.name);
    } catch (error) {
      postError('');
    }
  }

  /**
   * Handles connection state change.
   */
  private onDriveConnectionStatusChanged_() {
    chrome.fileManagerPrivate.getDriveConnectionState((state) => {
      // If offline, hide any sync progress notifications. When online again,
      // the Drive sync client may retry syncing and trigger
      // onFileTransfersUpdated events, causing it to be shown again.
      if (state.type ==
              chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
          state.reason ==
              chrome.fileManagerPrivate.DriveOfflineReason.NO_NETWORK &&
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
}
