// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isReadOnlyForDelete} from '../../common/js/entry_utils.js';
import {isEncrypted} from '../../common/js/file_type.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';
import {isDlpEnabled} from '../../common/js/flags.js';
import {AllowedPaths} from '../../common/js/volume_manager_types.js';
import {updateSelection} from '../../state/ducks/current_directory.js';
import {getStore} from '../../state/store.js';

import {FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES} from './constants.js';
import type {DirectoryModel} from './directory_model.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {ListContainer} from './ui/list_container.js';

/**
 * The current selection object.
 */
export class FileSelection {
  mimeTypes: string[] = [];
  totalCount = 0;
  fileCount = 0;
  directoryCount = 0;
  anyFilesNotInCache = true;
  anyFilesHosted = true;
  anyFilesEncrypted = true;

  private additionalPromise_: Promise<boolean>|null = null;

  /**
   * If the current selection has any read-only entry.
   */
  private hasReadOnlyEntry_ = false;

  constructor(
      public indexes: number[], public entries: Array<Entry|FilesAppEntry>,
      volumeManager: VolumeManager) {
    this.entries.forEach(entry => {
      if (!entry) {
        return;
      }
      if (entry.isFile) {
        this.fileCount += 1;
      } else {
        this.directoryCount += 1;
      }
      this.totalCount++;

      if (!this.hasReadOnlyEntry_ &&
          isReadOnlyForDelete(volumeManager, entry)) {
        this.hasReadOnlyEntry_ = true;
      }
    });
  }

  /**
   * @return True if there is any read-only entry in the current selection.
   */
  hasReadOnlyEntry(): boolean {
    return this.hasReadOnlyEntry_;
  }

  computeAdditional(metadataModel: MetadataModel): Promise<boolean> {
    if (!this.additionalPromise_) {
      this.additionalPromise_ =
          metadataModel
              .get(
                  this.entries, FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES)
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
                  return isEncrypted(this.entries[i]!, p.contentMimeType);
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

export type FileSelectionChangeEvent = CustomEvent<{}>;
export type FileSelectionChangeThrottledEvent = CustomEvent<{}>;

interface FileSelectionHandlerEventMap extends CustomEventMap {
  [EventType.CHANGE]: FileSelectionChangeEvent;
  [EventType.CHANGE_THROTTLED]: FileSelectionChangeThrottledEvent;
}

/**
 * This object encapsulates everything related to current selection.
 */
export class FileSelectionHandler extends
    FilesEventTarget<FileSelectionHandlerEventMap> {
  selection = new FileSelection([], [], this.volumeManager_);
  private selectionUpdateTimer_: number|null = 0;
  private store_ = getStore();
  /**
   * The time, in ms since the epoch, when it is OK to post next throttled
   * selection event. Can be directly compared with Date.now().
   */
  private nextThrottledEventTime_ = 0;

  constructor(
      private directoryModel_: DirectoryModel,
      private listContainer_: ListContainer,
      private metadataModel_: MetadataModel,
      private volumeManager_: VolumeManager,
      private allowedPaths_: AllowedPaths) {
    super();

    // Listens to changes in the selection model to propagate to other parts.
    this.directoryModel_.getFileListSelection().addEventListener(
        'change', this.onFileSelectionChanged.bind(this));

    // Register events to update file selections.
    this.directoryModel_.addEventListener(
        'directory-changed', this.onFileSelectionChanged.bind(this));
  }

  /**
   * Update the UI when the selection model changes.
   */
  onFileSelectionChanged() {
    const indexes = this.listContainer_.selectionModel?.selectedIndexes ?? [];
    const entries =
        indexes
            .map(index => this.directoryModel_.getFileList().item(index))
            // Filter out undefined for invalid index b/277232289.
            .filter((entry): entry is Entry|FilesAppEntry => !!entry);
    this.selection = new FileSelection(indexes, entries, this.volumeManager_);

    if (this.selectionUpdateTimer_) {
      clearTimeout(this.selectionUpdateTimer_);
      this.selectionUpdateTimer_ = null;
    }

    // The rest of the selection properties are computed via (sometimes lengthy)
    // asynchronous calls. We initiate these calls after a timeout. If the
    // selection is changing quickly we only do this once when it slows down.
    let updateDelay = UPDATE_DELAY;
    const now = Date.now();

    if (now >= this.nextThrottledEventTime_ &&
        indexes.length < NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE) {
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

    this.dispatchEvent(new CustomEvent(EventType.CHANGE));
  }

  /**
   * Calculates async selection stats and updates secondary UI elements.
   *
   * @param selection The selection object.
   */
  private updateFileSelectionAsync_(selection: FileSelection) {
    if (this.selection !== selection) {
      return;
    }

    // Calculate all additional and heavy properties.
    selection.computeAdditional(this.metadataModel_).then(() => {
      if (this.selection !== selection) {
        return;
      }

      this.nextThrottledEventTime_ = Date.now() + UPDATE_DELAY;
      this.dispatchEvent(new CustomEvent(EventType.CHANGE_THROTTLED));
    });
  }

  /**
   * Sends the current selection to the Store.
   */
  private updateStore_() {
    const entries = this.selection.entries;
    this.store_.dispatch(updateSelection({
      selectedKeys: entries.map(e => e.toURL()),
      entries,
    }));
  }

  /**
   * Returns true if all files in the selection files are selectable.
   */
  isAvailable(): boolean {
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
   */
  private isOfflineWithUncachedFilesSelected_(): boolean {
    return this.volumeManager_.getDriveConnectionState().type ===
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
        this.selection.anyFilesNotInCache;
  }

  /**
   * Returns true if we're a dialog requiring real files with hosted files
   * selected.
   */
  private isDialogWithHostedFilesSelected_(): boolean {
    return this.allowedPaths_ !== AllowedPaths.ANY_PATH_OR_URL &&
        this.selection.anyFilesHosted;
  }

  /**
   * Returns true if we're a dialog requiring real files with encrypted files
   * selected.
   */
  private isDialogWithEncryptedFilesSelected_(): boolean {
    return this.allowedPaths_ !== AllowedPaths.ANY_PATH_OR_URL &&
        this.selection.anyFilesEncrypted;
  }

  /**
   * Returns true if any file/directory in the selection is blocked by DLP
   * policy.
   */
  isDlpBlocked(): boolean {
    if (!isDlpEnabled()) {
      return false;
    }

    const selectedIndexes =
        this.directoryModel_.getFileListSelection().selectedIndexes;
    const selectedEntries = selectedIndexes.map(index => {
      return this.directoryModel_.getFileList().item(index)!;
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

export enum EventType {
  /**
   * Dispatched every time when selection is changed.
   */
  CHANGE = 'change',

  /**
   * Dispatched |UPDATE_DELAY| ms after the selection is changed.
   * If multiple changes are happened during the term, only one CHANGE_THROTTLED
   * event is dispatched.
   */
  CHANGE_THROTTLED = 'changethrottled',
}

/**
 * Delay in milliseconds before recalculating the selection in case the
 * selection is changed fast, or there are many items. Used to avoid freezing
 * the UI.
 */
const UPDATE_DELAY = 200;

/**
 * Number of items in the selection which triggers the update delay. Used to
 * let the Material Design animations complete before performing a heavy task
 * which would cause the UI freezing.
 */
const NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE = 100;
