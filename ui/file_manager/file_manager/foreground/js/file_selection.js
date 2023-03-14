// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {FileType} from '../../common/js/file_type.js';
import {util} from '../../common/js/util.js';
import {AllowedPaths} from '../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {Store} from '../../externs/ts/store.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {updateSelection} from '../../state/actions/current_directory.js';
import {getStore} from '../../state/store.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {ListContainer} from './ui/list_container.js';

/**
 * The current selection object.
 */
export class FileSelection {
  /**
   * @param {!Array<number>} indexes
   * @param {!Array<Entry>} entries
   * @param {!VolumeManager} volumeManager
   */
  constructor(indexes, entries, volumeManager) {
    /**
     * @public {!Array<number>}
     * @const
     */
    this.indexes = indexes;

    /**
     * @public {!Array<!Entry>}
     * @const
     */
    this.entries = entries;

    /**
     * @public {!Array<string>}
     */
    this.mimeTypes = [];

    /**
     * @public {number}
     */
    this.totalCount = 0;

    /**
     * @public {number}
     */
    this.fileCount = 0;

    /**
     * @public {number}
     */
    this.directoryCount = 0;

    /**
     * @public {boolean}
     */
    this.anyFilesNotInCache = true;

    /**
     * @public {boolean}
     */
    this.anyFilesHosted = true;

    /**
     * @public {boolean}
     */
    this.anyFilesEncrypted = true;

    /**
     * @private {Promise<boolean>}
     */
    this.additionalPromise_ = null;

    /** @private {boolean} If the current selection has any read-only entry. */
    this.hasReadOnlyEntry_ = false;

    entries.forEach(entry => {
      if (entry.isFile) {
        this.fileCount += 1;
      } else {
        this.directoryCount += 1;
      }
      this.totalCount++;

      if (!this.hasReadOnlyEntry_) {
        const locationInfo = volumeManager.getLocationInfo(entry);
        this.hasReadOnlyEntry_ = !!(locationInfo && locationInfo.isReadOnly);
      }
    });
  }

  /**
   * @return {boolean} True if there is any read-only entry in the current
   * selection.
   * @public
   */
  hasReadOnlyEntry() {
    return this.hasReadOnlyEntry_;
  }

  computeAdditional(metadataModel) {
    if (!this.additionalPromise_) {
      this.additionalPromise_ =
          metadataModel
              .get(
                  this.entries,
                  constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES)
              .then(props => {
                this.anyFilesNotInCache = props.some(p => {
                  // If no availableOffline property, then assume it's
                  // available.
                  return ('availableOffline' in p) && !p.availableOffline;
                });
                this.anyFilesHosted = props.some(p => {
                  return p.hosted;
                });
                this.anyFilesEncrypted = props.some((p, i) => {
                  return FileType.isEncrypted(
                      this.entries[i], p.contentMimeType);
                });
                this.mimeTypes = props.map(value => {
                  return value.contentMimeType || '';
                });
                return true;
              });
    }
    return this.additionalPromise_;
  }
}

/**
 * This object encapsulates everything related to current selection.
 */
export class FileSelectionHandler extends EventTarget {
  /**
   * @param {!DirectoryModel} directoryModel
   * @param {!FileOperationManager} fileOperationManager
   * @param {!ListContainer} listContainer
   * @param {!MetadataModel} metadataModel
   * @param {!VolumeManager} volumeManager
   * @param {!AllowedPaths} allowedPaths
   */
  constructor(
      directoryModel, fileOperationManager, listContainer, metadataModel,
      volumeManager, allowedPaths) {
    super();

    /**
     * @private {DirectoryModel}
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {ListContainer}
     * @const
     */
    this.listContainer_ = listContainer;

    /**
     * @private {MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @type {FileSelection}
     */
    this.selection = new FileSelection([], [], volumeManager);

    /**
     * @private {?number}
     */
    this.selectionUpdateTimer_ = 0;

    /** @private {!Store} */
    this.store_ = getStore();

    /**
     * The time, in ms since the epoch, when it is OK to post next throttled
     * selection event. Can be directly compared with Date.now().
     * @private {number}
     */
    this.nextThrottledEventTime_ = 0;

    /**
     * @private {AllowedPaths}
     * @const
     */
    this.allowedPaths_ = allowedPaths;

    // Listens to changes in the selection model to propagate to other parts.
    directoryModel.getFileListSelection().addEventListener(
        'change', this.onFileSelectionChanged.bind(this));

    // Register events to update file selections.
    directoryModel.addEventListener(
        'directory-changed', this.onFileSelectionChanged.bind(this));
  }

  /**
   * Update the UI when the selection model changes.
   */
  onFileSelectionChanged() {
    const indexes = this.listContainer_.selectionModel.selectedIndexes;
    const entries = indexes.map(index => {
      return /** @type {!Entry} */ (
          this.directoryModel_.getFileList().item(index));
    });
    this.selection = new FileSelection(indexes, entries, this.volumeManager_);

    if (this.selectionUpdateTimer_) {
      clearTimeout(this.selectionUpdateTimer_);
      this.selectionUpdateTimer_ = null;
    }

    // The rest of the selection properties are computed via (sometimes lengthy)
    // asynchronous calls. We initiate these calls after a timeout. If the
    // selection is changing quickly we only do this once when it slows down.
    let updateDelay = FileSelectionHandler.UPDATE_DELAY;
    const now = Date.now();

    if (now >= this.nextThrottledEventTime_ &&
        indexes.length <
            FileSelectionHandler.NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE) {
      // The previous selection change happened a while ago and there is few
      // selected items, so computation is lightweight. Update the UI with
      // 1 millisecond of delay.
      updateDelay = 1;
    }

    this.updateStore_();

    const selection = this.selection;
    this.selectionUpdateTimer_ = setTimeout(() => {
      this.selectionUpdateTimer_ = null;
      this.updateFileSelectionAsync_(selection);
    }, updateDelay);

    dispatchSimpleEvent(this, FileSelectionHandler.EventType.CHANGE);
  }

  /**
   * Calculates async selection stats and updates secondary UI elements.
   *
   * @param {FileSelection} selection The selection object.
   * @private
   */
  updateFileSelectionAsync_(selection) {
    if (this.selection !== selection) {
      return;
    }

    // Calculate all additional and heavy properties.
    selection.computeAdditional(this.metadataModel_).then(() => {
      if (this.selection !== selection) {
        return;
      }

      this.nextThrottledEventTime_ =
          Date.now() + FileSelectionHandler.UPDATE_DELAY;
      dispatchSimpleEvent(
          this, FileSelectionHandler.EventType.CHANGE_THROTTLED);
    });
  }

  /**
   * Sends the current selection to the Store.
   * @private
   */
  updateStore_() {
    const entries = this.selection.entries;
    this.store_.dispatch(updateSelection({
      selectedKeys: entries.map(e => e.toURL()),
      entries,
    }));
  }

  /**
   * Returns true if all files in the selection files are selectable.
   * @return {boolean}
   */
  isAvailable() {
    if (!this.directoryModel_.isOnDrive()) {
      return true;
    }

    return !(
        this.isOfflineWithUncachedFilesSelected_() ||
        this.isDialogWithHostedFilesSelected_() ||
        this.isDialogWithEncryptedFilesSelected_());
  }

  /**
   * Returns true if we're offline with any selected files absent from the
   * cache.
   * @return {boolean}
   * @private
   */
  isOfflineWithUncachedFilesSelected_() {
    return this.volumeManager_.getDriveConnectionState().type ===
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
        this.selection.anyFilesNotInCache;
  }

  /**
   * Returns true if we're a dialog requiring real files with hosted files
   * selected.
   * @return {boolean}
   * @private
   */
  isDialogWithHostedFilesSelected_() {
    return this.allowedPaths_ !== AllowedPaths.ANY_PATH_OR_URL &&
        this.selection.anyFilesHosted;
  }

  /**
   * Returns true if we're a dialog requiring real files with encrypted files
   * selected.
   * @return {boolean}
   * @private
   */
  isDialogWithEncryptedFilesSelected_() {
    return this.allowedPaths_ !== AllowedPaths.ANY_PATH_OR_URL &&
        this.selection.anyFilesEncrypted;
  }

  /**
   * Returns true if any file/directory in the selection is blocked by DLP
   * policy.
   * @return {boolean}
   */
  isDlpBlocked() {
    if (!util.isDlpEnabled()) {
      return false;
    }

    const selectedIndexes =
        this.directoryModel_.getFileListSelection().selectedIndexes;
    const selectedEntries = selectedIndexes.map(index => {
      return /** @type {!Entry} */ (
          this.directoryModel_.getFileList().item(index));
    });
    // Check if any of the selected entries are blocked by DLP:
    // a volume/directory in case of file-saveas (managed by the VolumeManager),
    // or a file in case file-open dialogs (stored in the metadata).
    for (const entry of selectedEntries) {
      const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
      if (volumeInfo && this.volumeManager_.isDisabled(volumeInfo.volumeType)) {
        return true;
      }
      const metadata = this.metadataModel_.getCache(
          [entry], ['isRestrictedForDestination'])[0];
      if (metadata && !!metadata.isRestrictedForDestination) {
        return true;
      }
    }
    return false;
  }
}

/**
 * @enum {string}
 */
FileSelectionHandler.EventType = {
  /**
   * Dispatched every time when selection is changed.
   */
  CHANGE: 'change',

  /**
   * Dispatched |UPDATE_DELAY| ms after the selecton is changed.
   * If multiple changes are happened during the term, only one CHANGE_THROTTLED
   * event is dispatched.
   */
  CHANGE_THROTTLED: 'changethrottled',
};

/**
 * Delay in milliseconds before recalculating the selection in case the
 * selection is changed fast, or there are many items. Used to avoid freezing
 * the UI.
 * @const {number}
 */
FileSelectionHandler.UPDATE_DELAY = 200;

/**
 * Number of items in the selection which triggers the update delay. Used to
 * let the Material Design animations complete before performing a heavy task
 * which would cause the UI freezing.
 * @const {number}
 */
FileSelectionHandler.NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE = 100;
