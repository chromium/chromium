// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {SpliceEvent} from '../../common/js/array_data_model.js';
import {Aggregator, AsyncQueue} from '../../common/js/async_util.js';
import {convertURLsToEntries, entriesToURLs, getRootType, isFakeEntry, isGuestOs, isNativeEntry, isOneDrive, isOneDriveId, isOneDrivePlaceholder, isRecentRootType, isSameEntry, urlToEntry} from '../../common/js/entry_utils.js';
import type {FakeEntry, FilesAppDirEntry, FilesAppEntry, GuestOsPlaceholder, UniversalDirectory} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled, isSkyvaultV2Enabled} from '../../common/js/flags.js';
import {recordMediumCount} from '../../common/js/metrics.js';
import {getEntryLabel} from '../../common/js/translations.js';
import {testSendMessage} from '../../common/js/util.js';
import {FileSystemType, getVolumeTypeFromRootType, isNative, RootType, Source, VolumeType} from '../../common/js/volume_manager_types.js';
import {getMyFiles} from '../../state/ducks/all_entries.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {clearSearch, getDefaultSearchOptions, updateSearch} from '../../state/ducks/search.js';
import type {FileData, FileKey, SearchData} from '../../state/state.js';
import {EntryType, PropStatus, SearchLocation, type SearchOptions, type State, type Volume, type VolumeId} from '../../state/state.js';
import {getFileData, getStore, getVolume, type Store} from '../../state/store.js';

import {CROSTINI_CONNECT_ERR, DLP_METADATA_PREFETCH_PROPERTY_NAMES, LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES} from './constants.js';
import type {ContentScanner, DirContentsScanFailedEvent, DirContentsScanUpdatedEvent, FileFilter} from './directory_contents.js';
import {CrostiniMounter, DirectoryContents, DirectoryContentScanner, DriveMetadataSearchContentScanner, EmptyContentScanner, FileListContext, GuestOsMounter, MediaViewContentScanner, RecentContentScanner, SearchV2ContentScanner, StoreScanner, TrashContentScanner} from './directory_contents.js';
import {FileListModel} from './file_list_model.js';
import {FileWatcher, type WatcherDirectoryChangedEvent} from './file_watcher.js';
import type {MetadataKey} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {FileListSelectionModel, FileListSingleSelectionModel} from './ui/file_list_selection_model.js';
import type {ListSelectionModel} from './ui/list_selection_model.js';
import type {ListSingleSelectionModel} from './ui/list_single_selection_model.js';

/**
 * Used to track asynchronous directory change use like:
 * const tracker = directoryModel.createDirectoryChangeTracker();
 * tracker.start();
 * try {
 *    ... async code here ...
 *    if (tracker.hasChanged) {
 *      // This code shouldn't continue anymore.
 *    }
 * } finally {
 *     tracker.stop();
 * }
 */
export interface DirectoryChangeTracker {
  start(): void;
  stop(): void;
  // Indicates that another directory change has occurred since start(). The
  // change tracker that sees a `hasChanged=true` should not proceed to change
  // the directory, because they became stale.
  hasChanged: boolean;
}

// If directory files changes too often, don't rescan directory more than once
// per specified interval
const SIMULTANEOUS_RESCAN_INTERVAL = 500;
// Used for operations that require almost instant rescan.
const SHORT_RESCAN_INTERVAL = 100;

/**
 * Helper function that can decide if the scan of the given entry should be
 * performed by recent scanner or other (search) scanner. In transition period
 * between V1 and V2 versions of search, when the user searches in Recent, and
 * uses Recent as location, we reuse the Recent scanner. Otherwise, the true
 * search scanner is used.
 */
function isRecentScan(
    entry: DirectoryEntry|FilesAppEntry,
    options?: SearchOptions): entry is FakeEntry {
  if (isRecentRootType(getRootType(entry))) {
    // Potential search in Recents. However, if options are present and are
    // indicating that the user wishes to scan current entry, still use Recent
    // scanner.
    if (!options || options.location === SearchLocation.THIS_FOLDER) {
      return true;
    }
  }
  return false;
}

/**
 * Helper function that determines the category of files we are looking for
 * based on the fake entry, query and options.
 */
function getFileCategory(
    entry: FakeEntry, query: string|undefined,
    options: SearchOptions|undefined) {
  if (query) {
    if (options) {
      return options.fileCategory;
    }
  }
  return entry.fileCategory;
}

export type DirectoryChangeEvent = CustomEvent<{
  previousDirEntry?: DirectoryEntry | FilesAppDirEntry,
  previousFileKey?: FileKey,
  newDirEntry?: DirectoryEntry | FilesAppDirEntry,
  newFileKey?: FileKey, volumeChanged: boolean,
}>;

export type CurDirScanFailedEvent = CustomEvent<{error: DOMError}>;
export type CurDirScanUpdatedEvent = CustomEvent<{
  /** Whether the content scanner was based in the store. */
  isStoreBased: boolean,
}>;
export type EmptyEvent = CustomEvent<undefined>;

interface DirectoryModelEventMap extends CustomEventMap {
  'directory-changed': DirectoryChangeEvent;
  'cur-dir-rescan-completed': EmptyEvent;
  'cur-dir-scan-completed': EmptyEvent;
  'cur-dir-scan-started': EmptyEvent;

  'cur-dir-scan-failed': CurDirScanFailedEvent;
  'cur-dir-scan-canceled': EmptyEvent;
  'cur-dir-scan-updated': CurDirScanUpdatedEvent;
}

/**
 * Data model for the current directory for Files app.
 *
 * It encapsulates the current directory, the file selection, directory scanner,
 * etc.
 */
export class DirectoryModel extends FilesEventTarget<DirectoryModelEventMap> {
  private fileListSelection_: FileListSingleSelectionModel|
      FileListSelectionModel;
  private runningScan_: DirectoryContents|null = null;
  private pendingScan_: boolean|null = null;
  private pendingRescan_: boolean|null = null;
  private rescanTime_: number|null = null;
  private rescanTimeoutId_?: number;
  private changeDirectorySequence_ = 0;
  private cachedSearch_: SearchData|undefined|{} = {};
  private scanFailures_ = 0;
  private onSearchCompleted_: ((e: Event) => void)|null = null;
  private ignoreCurrentDirectoryDeletion_ = false;
  private directoryChangeQueue_ = new AsyncQueue();
  /**
   * Number of running directory change trackers.
   */
  private numChangeTrackerRunning_ = 0;
  private rescanAggregator_ =
      new Aggregator(this.rescanSoon.bind(this, true), 500);
  private currentFileListContext_: FileListContext;
  private currentDirContents_: DirectoryContents;
  private emptyFileList_: FileListModel;
  private fileWatcher_ = new FileWatcher();
  private lastSearchQuery_ = '';
  private volumes_: (Record<VolumeId, Volume>)|null = null;
  private store_: Store;

  /**
   * @param singleSelection True if only one file could be selected at the time.
   */
  constructor(
      singleSelection: boolean, private fileFilter_: FileFilter,
      private metadataModel_: MetadataModel,
      private volumeManager_: VolumeManager) {
    super();

    this.fileListSelection_ = singleSelection ?
        new FileListSingleSelectionModel() :
        new FileListSelectionModel();

    this.fileFilter_.addEventListener(
        'changed', this.onFilterChanged_.bind(this));

    this.currentFileListContext_ = new FileListContext(
        this.fileFilter_, this.metadataModel_, this.volumeManager_);
    this.currentDirContents_ = new DirectoryContents(
        this.currentFileListContext_, false, undefined, undefined, () => {
          return new DirectoryContentScanner(undefined);
        });

    /**
     * Empty file list which is used as a dummy for inactive view of file list.
     */
    this.emptyFileList_ = new FileListModel(this.metadataModel_);

    this.volumeManager_.volumeInfoList.addEventListener(
        'splice', this.onVolumeInfoListUpdated_.bind(this) as EventListener);

    this.fileWatcher_.addEventListener(
        'watcher-directory-changed',
        this.onWatcherDirectoryChanged_.bind(this));
    // For non-watchable directories (e.g. FakeEntry) and volumes (MTP) we need
    // to subscribe to the IOTask and manually refresh.
    chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(
        this.updateFileListAfterIoTask_.bind(this));

    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  onStateChanged(state: State) {
    this.handleDirectoryState_(state);
    this.handleSearchState_(state);
  }

  /**
   * Handles the current directory slice of the store's state.
   * @param state latest state from the store.
   */
  private handleDirectoryState_(state: State) {
    const currentURL = this.getCurrentFileKey();
    const newURL = state.currentDirectory ? state.currentDirectory.key : null;

    // Observe volume changes.
    if (this.volumes_ !== state.volumes) {
      this.onStateVolumeChanged_(state);
      this.volumes_ = state.volumes;
    }

    // If the directory is the same or the newURL is null, ignore it.
    if (currentURL === newURL || !newURL) {
      return;
    }

    // When something changed the current directory status to STARTED, Here we
    // initiate the actual change and will update to SUCCESS at the end.
    if (state.currentDirectory?.status === PropStatus.STARTED) {
      const fileData = getFileData(state, newURL);
      if (fileData?.type === EntryType.MATERIALIZED_VIEW) {
        this.changeDirectoryFileData(fileData);
        return;
      }

      const entry = fileData?.entry;
      if (!entry) {
        // TODO(lucmult): Fix potential race condition in this await/then.
        urlToEntry(newURL)
            .then((entry) => {
              if (!entry) {
                throw new Error(
                    `Failed to find the new directory key ${newURL}`);
              }
              // Initiate the directory change.
              this.changeDirectoryEntry(entry as DirectoryEntry);
            })
            .catch((error) => {
              console.warn(error);
              this.store_.dispatch(
                  changeDirectory({toKey: newURL, status: PropStatus.ERROR}));
            });
        return;
      }

      // Initiate the directory change.
      this.changeDirectoryEntry(entry as DirectoryEntry);
    }
  }

  /**
   * Reacts to changes in the search state of the store. If the search changed
   * and the query is not empty, this method triggers a new directory search.
   */
  private handleSearchState_(state: State) {
    const currentEntry = this.getCurrentDirEntry();
    // Do not handle any search state until we have the current directory set.
    // Requests to handle current search state may be triggered by the files app
    // before it is fully started.
    if (!currentEntry) {
      return;
    }
    const search = state.search;
    if (this.cachedSearch_ === search) {
      // Bail out early if the search part of the state has not changed.
      return;
    }

    // Cache the last received search state for future comparisons.
    const lastSearch = this.cachedSearch_;
    this.cachedSearch_ = search;

    // We change the search state (STARTED, SUCCESS, etc.) so only trigger
    // a new search if the query or the options have changed.
    if (!search) {
      return;
    }
    if (!lastSearch || (lastSearch as SearchData).query !== search.query ||
        (lastSearch as SearchData).options !== search.options) {
      this.search_(
          search.query || '', search.options || getDefaultSearchOptions());
    }
  }

  /**
   * Disposes the directory model by removing file watchers.
   */
  dispose(): void {
    this.fileWatcher_.dispose();
  }

  /**
   * @return Files in the current directory.
   */
  getFileList(): FileListModel {
    return this.currentFileListContext_.fileList;
  }

  /**
   * @return File list which is always empty.
   */
  getEmptyFileList(): FileListModel {
    return this.emptyFileList_;
  }

  /**
   * @return Selection in the fileList.
   */
  getFileListSelection(): FileListSelectionModel|FileListSingleSelectionModel {
    return this.fileListSelection_;
  }

  /**
   * Obtains current volume information.
   */
  getCurrentVolumeInfo(): VolumeInfo|null {
    const entry = this.getCurrentDirEntry();
    if (!entry) {
      return null;
    }
    return this.volumeManager_.getVolumeInfo(entry);
  }

  /**
   * @return Root type of current root, or null if not found.
   */
  getCurrentRootType(): RootType|null {
    const entry = this.currentDirContents_.getDirectoryEntry();
    if (!entry) {
      return null;
    }

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (!locationInfo) {
      return null;
    }

    return locationInfo.rootType;
  }

  /**
   * Metadata property names that are expected to be Prefetched.
   */
  getPrefetchPropertyNames(): MetadataKey[] {
    return this.currentFileListContext_.prefetchPropertyNames;
  }

  /**
   * @return True if the current directory is read only. If there is no entry
   *     set, then returns true.
   */
  isReadOnly(): boolean {
    const currentDirEntry = this.getCurrentDirEntry();
    if (currentDirEntry) {
      const locationInfo = this.volumeManager_.getLocationInfo(currentDirEntry);
      if (locationInfo) {
        return locationInfo.isReadOnly;
      }
    }
    return true;
  }

  /**
   * @return True if the a scan is active.
   */
  isScanning(): boolean {
    return this.currentDirContents_.isScanning();
  }

  /**
   * @return True if search is in progress.
   */
  isSearching(): boolean {
    return this.currentDirContents_.isSearch();
  }

  /**
   * @return True if it's on Drive.
   */
  isOnDrive(): boolean {
    return this.isCurrentRootVolumeType_(VolumeType.DRIVE);
  }

  /**
   * @return True if the current volume is provided by FuseBox.
   */
  isOnFuseBox(): boolean {
    const info = this.getCurrentVolumeInfo();
    return info ? info.diskFileSystemType === FileSystemType.FUSEBOX : false;
  }

  /**
   * @return True if it's on a Linux native volume.
   */
  isOnNative(): boolean {
    const rootType = this.getCurrentRootType();
    return rootType !== null && !isRecentRootType(rootType) &&
        isNative(getVolumeTypeFromRootType(rootType));
  }

  /**
   * @return True if the current volume is blocked by DLP.
   */
  isDlpBlocked(): boolean {
    if (!isDlpEnabled()) {
      return false;
    }
    const info = this.getCurrentVolumeInfo();
    return info ? this.volumeManager_.isDisabled(info.volumeType) : false;
  }

  /**
   * @param volumeType Volume Type
   * @return True if current root volume type is equal to specified volume type.
   */
  private isCurrentRootVolumeType_(volumeType: VolumeType): boolean {
    const rootType = this.getCurrentRootType();
    return rootType !== null && !isRecentRootType(rootType) &&
        getVolumeTypeFromRootType(rootType) === volumeType;
  }

  /**
   * Updates the selection by using the updateFunc and publish the change event.
   * If updateFunc returns true, it force to dispatch the change event even if
   * the selection index is not changed.
   *
   * @param selection Selection to be updated.
   * @param updateFunc Function updating the selection.
   */
  private updateSelectionAndPublishEvent_(
      selection: ListSelectionModel|ListSingleSelectionModel,
      updateFunc: () => boolean) {
    // Begin change.
    selection.beginChange();

    // If dispatchNeeded is true, we should ensure the change event is
    // dispatched.
    let dispatchNeeded = updateFunc();

    // Check if the change event is dispatched in the endChange function
    // or not.
    const eventDispatched = () => {
      dispatchNeeded = false;
    };
    selection.addEventListener('change', eventDispatched);
    selection.endChange();
    selection.removeEventListener('change', eventDispatched);

    // If the change event have been already dispatched, dispatchNeeded is
    // false.
    if (dispatchNeeded) {
      // The selection status (selected or not) is not changed because
      // this event is caused by the change of selected item.
      const event = new CustomEvent('change', {detail: {changes: []}});
      selection.dispatchEvent(event);
    }
  }

  /**
   * Sets to ignore current directory deletion. This method is used to prevent
   * going up to the volume root with the deletion of current directory by
   * rename operation in directory tree.
   * @param value True to ignore current directory deletion.
   */
  setIgnoringCurrentDirectoryDeletion(value: boolean) {
    this.ignoreCurrentDirectoryDeletion_ = value;
  }

  /**
   * Invoked when a change in the directory is detected by the watcher.
   * @param event Event object.
   */
  private onWatcherDirectoryChanged_(event: WatcherDirectoryChangedEvent) {
    const directoryEntry = this.getCurrentDirEntry();

    if (!this.ignoreCurrentDirectoryDeletion_ && directoryEntry) {
      // If the change is deletion of currentDir, move up to its parent
      // directory.
      directoryEntry.getDirectory(
          directoryEntry.fullPath, {create: false}, () => {}, async () => {
            assert(directoryEntry);
            const volumeInfo =
                this.volumeManager_.getVolumeInfo(directoryEntry);
            if (volumeInfo) {
              const displayRoot = await volumeInfo.resolveDisplayRoot();
              this.changeDirectoryEntry(displayRoot);
            }
          });
    }

    if (event.detail?.changedFiles) {
      const addedOrUpdatedFileUrls: string[] = [];
      let deletedFileUrls: string[] = [];
      event.detail.changedFiles.forEach(change => {
        if (change.changes.length === 1 && change.changes[0] === 'delete') {
          deletedFileUrls.push(change.url);
        } else {
          addedOrUpdatedFileUrls.push(change.url);
        }
      });

      convertURLsToEntries(addedOrUpdatedFileUrls)
          .then(result => {
            deletedFileUrls = deletedFileUrls.concat(result.failureUrls);

            // Passing the resolved entries and failed URLs as the removed
            // files. The URLs are removed files and they chan't be resolved.
            this.partialUpdate_(result.entries, deletedFileUrls);
          })
          .catch(error => {
            console.warn(
                'Error in proceeding the changed event.', error,
                'Fallback to force-refresh');
            this.rescanAggregator_.run();
          });
    } else {
      // Invokes force refresh if the detailed information isn't provided.
      // This can occur very frequently (e.g. when copying files into Downloads)
      // and rescan is heavy operation, so we keep some interval for each
      // rescan.
      this.rescanAggregator_.run();
    }
  }

  /**
   * Invoked when filters are changed.
   */
  private async onFilterChanged_() {
    const currentDirectory = this.getCurrentDirEntry();
    if (currentDirectory && isNativeEntry(currentDirectory) &&
        !this.fileFilter_.filter(currentDirectory)) {
      // If the current directory should be hidden in the new filter setting,
      // change the current directory to the current volume's root.
      const volumeInfo = this.volumeManager_.getVolumeInfo(currentDirectory);
      if (volumeInfo) {
        const displayRoot = await volumeInfo.resolveDisplayRoot();
        this.changeDirectoryEntry(displayRoot);
      }
    } else {
      this.rescanSoon(false);
    }
  }

  /**
   * Invoked when volumes have been modified in the state.
   * @param state latest state from the store.
   */
  private onStateVolumeChanged_(state: State) {
    if (!state.currentDirectory) {
      return;
    }
    for (const volume of Object.values(state.volumes)) {
      // Navigate out of ODFS if it got disabled and the current directory is
      // under ODFS.
      const isOdfs = isOneDriveId(volume.providerId);
      if (!(isOdfs && volume.isDisabled)) {
        continue;
      }
      const currentDirectoryFileData =
          getFileData(state, state.currentDirectory.key);
      const currentDirectoryOnOdfs =
          isOneDriveId(getVolume(state, currentDirectoryFileData)?.providerId);
      if (currentDirectoryOnOdfs) {
        const tracker = this.createDirectoryChangeTracker();
        tracker.start();
        // Normally the default root is MyFiles, however with SkyVault, this
        // is the volume in the Cloud (OneDrive or GoogleDrive).
        this.volumeManager_.getDefaultDisplayRoot().then((displayRoot) => {
          if (displayRoot && !tracker.hasChanged) {
            this.changeDirectoryEntry(displayRoot);
          }
        });
        tracker.stop();
      }
    }
  }

  getFileFilter(): FileFilter {
    return this.fileFilter_;
  }

  getCurrentDirEntry(): DirectoryEntry|FakeEntry|FilesAppDirEntry|undefined {
    return this.currentDirContents_.getDirectoryEntry();
  }

  getCurrentDirName(): string {
    const fileData = this.getCurrentFileData();
    if (fileData) {
      return fileData.label;
    }

    const dirEntry = this.getCurrentDirEntry();
    if (!dirEntry) {
      return '';
    }

    const locationInfo = this.volumeManager_.getLocationInfo(dirEntry);
    return getEntryLabel(locationInfo, dirEntry);
  }

  getCurrentFileKey(): FileKey|undefined {
    return this.currentDirContents_.getFileKey();
  }

  getCurrentFileData(): FileData|undefined {
    const fileKey = this.getCurrentFileKey();
    if (!fileKey) {
      return;
    }

    return getFileData(this.store_.getState(), fileKey) ?? undefined;
  }

  /**
   * @return Array of selected entries.
   */
  private getSelectedEntries_(): Array<Entry|FilesAppEntry> {
    const indexes = this.fileListSelection_.selectedIndexes;
    const fileList = this.getFileList();
    if (fileList) {
      return indexes.map(i => fileList.item(i)!);
    }
    return [];
  }

  /**
   * @param value List of selected entries.
   */
  private setSelectedEntries_(value: Array<Entry|FilesAppEntry>) {
    const indexes = [];
    const fileList = this.getFileList();
    const urls = entriesToURLs(value);

    for (let i = 0; i < fileList.length; i++) {
      if (urls.indexOf(fileList.item(i)!.toURL()) !== -1) {
        indexes.push(i);
      }
    }
    this.fileListSelection_.selectedIndexes = indexes;
  }

  /**
   * @return Lead entry.
   */
  private getLeadEntry_(): Entry|FilesAppEntry|null {
    const index = this.fileListSelection_.leadIndex;
    return index >= 0 ? this.getFileList().item(index)! : null;
  }

  /**
   * @param value The new lead entry.
   */
  private setLeadEntry_(value: Entry|FilesAppEntry|null) {
    const fileList = this.getFileList();
    for (let i = 0; i < fileList.length; i++) {
      if (isSameEntry(fileList.item(i), value)) {
        this.fileListSelection_.leadIndex = i;
        return;
      }
    }
  }

  /**
   * Schedule rescan with short delay.
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate the backend scanning result
   *     cache. This param only works if the corresponding backend scanning
   *     supports cache.
   */
  rescanSoon(refresh: boolean, invalidateCache: boolean = false) {
    this.scheduleRescan(SHORT_RESCAN_INTERVAL, refresh, invalidateCache);
  }

  /**
   * Schedule rescan with delay. Designed to handle directory change
   * notification.
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate the backend scanning result
   *     cache. This param only works if the corresponding backend scanning
   *     supports cache.
   */
  rescanLater(refresh: boolean, invalidateCache: boolean = false) {
    this.scheduleRescan(SIMULTANEOUS_RESCAN_INTERVAL, refresh, invalidateCache);
  }

  /**
   * Schedule rescan with delay. If another rescan has been scheduled does
   * nothing. File operation may cause a few notifications what should cause
   * a single refresh.
   * @param delay Delay in ms after which the rescan will be performed.
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate the backend scanning result
   *     cache. This param only works if the corresponding backend scanning
   *     supports cache.
   */
  scheduleRescan(
      delay: number, refresh: boolean, invalidateCache: boolean = false) {
    if (this.rescanTime_) {
      if (this.rescanTime_ <= Date.now() + delay) {
        return;
      }
      clearTimeout(this.rescanTimeoutId_);
    }

    const sequence = this.changeDirectorySequence_;

    this.rescanTime_ = Date.now() + delay;
    this.rescanTimeoutId_ = setTimeout(() => {
      this.rescanTimeoutId_ = undefined;
      if (sequence === this.changeDirectorySequence_) {
        this.rescan(refresh, invalidateCache);
      }
    }, delay);
  }

  /**
   * Cancel a rescan on timeout if it is scheduled.
   */
  private clearRescanTimeout_() {
    this.rescanTime_ = null;
    if (this.rescanTimeoutId_) {
      clearTimeout(this.rescanTimeoutId_);
      this.rescanTimeoutId_ = undefined;
    }
  }

  /**
   * Rescan current directory. May be called indirectly through rescanLater or
   * directly in order to reflect user action. Will first cache all the
   * directory contents in an array, then seamlessly substitute the fileList
   * contents, preserving the select element etc.
   *
   * This should be to scan the contents of current directory (or search).
   *
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate the backend scanning result
   *     cache. This param only works if the corresponding backend scanning
   *     supports cache.
   */
  rescan(refresh: boolean, invalidateCache: boolean = false) {
    this.clearRescanTimeout_();
    if (this.runningScan_) {
      this.pendingRescan_ = true;
      return;
    }

    const dirContents = this.currentDirContents_.clone();
    dirContents.setFileList(new FileListModel(this.metadataModel_));
    dirContents.setMetadataSnapshot(
        this.currentDirContents_.createMetadataSnapshot());

    const sequence = this.changeDirectorySequence_;

    const successCallback = () => {
      if (sequence === this.changeDirectorySequence_) {
        this.replaceDirectoryContents_(dirContents);
        this.dispatchEvent(new CustomEvent('cur-dir-rescan-completed'));
      }
    };

    this.scan_(
        dirContents, refresh, invalidateCache, successCallback, () => {},
        () => {}, () => {});
  }

  /**
   * Run scan on the current DirectoryContents. The active fileList is cleared
   * and the entries are added directly.
   *
   * This should be used when changing directory or initiating a new search.
   *
   * @param newDirContents New DirectoryContents instance to replace
   *     `currentDirContents_`.
   * @param callback Callback with result. True if the scan is completed
   *     successfully, false if the scan is failed.
   */
  private clearAndScan_(
      newDirContents: DirectoryContents, callback: (result: boolean) => void) {
    if (this.currentDirContents_.isScanning()) {
      this.currentDirContents_.cancelScan();
    }
    this.currentDirContents_ = newDirContents;
    this.clearRescanTimeout_();

    if (this.pendingScan_) {
      this.pendingScan_ = false;
    }

    if (this.runningScan_) {
      if (this.runningScan_.isScanning()) {
        this.runningScan_.cancelScan();
      }
      this.runningScan_ = null;
    }

    const sequence = this.changeDirectorySequence_;
    let cancelled = false;

    const onDone = () => {
      if (cancelled) {
        return;
      }

      this.dispatchEvent(new CustomEvent('cur-dir-scan-completed'));
      callback(true);
    };

    const onFailed = (error: DOMError) => {
      if (cancelled) {
        return;
      }

      this.dispatchEvent(
          new CustomEvent('cur-dir-scan-failed', {detail: {error}}));
      callback(false);
    };

    const onUpdated = (event: DirContentsScanUpdatedEvent) => {
      if (cancelled) {
        return;
      }

      if (this.changeDirectorySequence_ !== sequence) {
        cancelled = true;
        this.dispatchEvent(new CustomEvent('cur-dir-scan-canceled'));
        callback(false);
        return;
      }

      const newEvent = new CustomEvent('cur-dir-scan-updated', {
        detail: {
          isStoreBased: event.detail.isStoreBased,
        },
      });
      this.dispatchEvent(newEvent);
    };

    const onCancelled = () => {
      if (cancelled) {
        return;
      }

      cancelled = true;
      this.dispatchEvent(new CustomEvent('cur-dir-scan-canceled'));
      callback(false);
    };

    // Clear metadata information for the old (no longer visible) items in the
    // file list.
    const fileList = this.getFileList();
    const removedUrls = [];
    for (let i = 0; i < fileList.length; i++) {
      removedUrls.push(fileList.item(i)!.toURL());
    }
    this.metadataModel_.notifyEntriesRemoved(removedUrls);

    // Retrieve metadata information for the newly selected directory.
    const currentEntry = this.currentDirContents_.getDirectoryEntry();
    if (currentEntry) {
      const locationInfo = this.volumeManager_.getLocationInfo(currentEntry);
      // When bulk pinning is enabled, this call is made with more frequency in
      // the UI delegate as hosted documents receive the available offline tick
      // when they are both explicitly pinned and heuristically cached.
      if (locationInfo && locationInfo.isDriveBased &&
          !isDriveFsBulkPinningEnabled()) {
        chrome.fileManagerPrivate.pollDriveHostedFilePinStates();
      }
      if (!isFakeEntry(currentEntry)) {
        this.metadataModel_.get([currentEntry], [
          ...LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
          ...DLP_METADATA_PREFETCH_PROPERTY_NAMES,
        ]);
      }
    }

    // Clear the table, and start scanning.
    fileList.splice(0, fileList.length);
    this.dispatchEvent(new CustomEvent('cur-dir-scan-started'));
    this.scan_(
        this.currentDirContents_, false, true, onDone, onFailed, onUpdated,
        onCancelled);
  }

  /**
   * Similar to clearAndScan_() but instead of passing a `newDirContents`, it
   * uses the `currentDirContents_`.
   */
  clearCurrentDirAndScan() {
    const sequence = ++this.changeDirectorySequence_;
    this.directoryChangeQueue_.run(callback => {
      if (this.changeDirectorySequence_ !== sequence) {
        callback();
        return;
      }
      const currentDirEntry = this.getCurrentDirEntry()!;
      assert(currentDirEntry);
      const newDirContents = this.createDirectoryContents_(
          this.currentFileListContext_, currentDirEntry,
          currentDirEntry.toURL(), this.lastSearchQuery_);
      this.clearAndScan_(newDirContents, callback);
    });
  }

  /**
   * Adds/removes/updates items of file list.
   * @param changedEntries Entries of updated/added files.
   * @param removedUrls URLs of removed files.
   */
  private partialUpdate_(changedEntries: Entry[], removedUrls: string[]) {
    // This update should be included in the current running update.
    if (this.pendingScan_) {
      return;
    }

    if (this.runningScan_) {
      // Do update after the current scan is finished.
      const previousScan = this.runningScan_;
      const onPreviousScanCompleted = () => {
        previousScan.removeEventListener(
            'dir-contents-scan-completed', onPreviousScanCompleted);
        // Run the update asynchronously.
        Promise.resolve().then(() => {
          this.partialUpdate_(changedEntries, removedUrls);
        });
      };
      previousScan.addEventListener(
          'dir-contents-scan-completed', onPreviousScanCompleted);
      return;
    }

    const onFinish = () => {
      this.runningScan_ = null;

      this.currentDirContents_.removeEventListener(
          'dir-contents-scan-completed', onCompleted);
      this.currentDirContents_.removeEventListener(
          'dir-contents-scan-failed', onFailure);
      this.currentDirContents_.removeEventListener(
          'dir-contents-scan-canceled', onCancelled);
    };

    const onCompleted = () => {
      onFinish();
      this.dispatchEvent(new CustomEvent('cur-dir-rescan-completed'));
    };

    const onFailure = () => {
      onFinish();
    };

    const onCancelled = () => {
      onFinish();
    };

    this.runningScan_ = this.currentDirContents_;
    this.currentDirContents_.addEventListener(
        'dir-contents-scan-completed', onCompleted);
    this.currentDirContents_.addEventListener(
        'dir-contents-scan-failed', onFailure);
    this.currentDirContents_.addEventListener(
        'dir-contents-scan-canceled', onCancelled);
    this.currentDirContents_.update(changedEntries, removedUrls);
  }

  /**
   * Perform a directory contents scan. Should be called only from rescan() and
   * clearAndScan_().
   *
   * @param dirContents DirectoryContents instance on which the scan will be
   *     run.
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate scanning result cache.
   * @param successCallback Callback on success.
   * @param failureCallback Callback on failure.
   * @param updatedCallback Callback on update. Only on the last update,
   *     `successCallback` is called instead of this.
   * @param cancelledCallback Callback on cancel.
   */
  private scan_(
      dirContents: DirectoryContents, refresh: boolean,
      invalidateCache: boolean, successCallback: VoidCallback,
      failureCallback: (error: DOMError) => void,
      updatedCallback: (event: DirContentsScanUpdatedEvent) => void,
      cancelledCallback: VoidCallback) {
    /**
     * Runs pending scan if there is one.
     * @return Did pending scan exist.
     */
    const maybeRunPendingRescan = (): boolean => {
      if (this.pendingRescan_) {
        this.rescanSoon(refresh);
        this.pendingRescan_ = false;
        return true;
      }
      return false;
    };

    const onFinished = () => {
      dirContents.removeEventListener('dir-contents-scan-completed', onSuccess);
      dirContents.removeEventListener(
          'dir-contents-scan-updated', updatedCallback);
      dirContents.removeEventListener('dir-contents-scan-failed', onFailure);
      dirContents.removeEventListener(
          'dir-contents-scan-canceled', cancelledCallback);
    };

    const onSuccess = () => {
      onFinished();

      // Record metric for Downloads directory.
      const dirEntry = dirContents.getDirectoryEntry();
      if (dirEntry && !dirContents.isSearch()) {
        const locationInfo = this.volumeManager_.getLocationInfo(dirEntry);
        const volumeInfo = locationInfo && locationInfo.volumeInfo;
        if (volumeInfo && volumeInfo.volumeType === VolumeType.DOWNLOADS &&
            locationInfo.isRootEntry) {
          recordMediumCount('DownloadsCount', dirContents.getFileListLength());
        }
      }

      this.runningScan_ = null;
      successCallback();
      this.scanFailures_ = 0;
      maybeRunPendingRescan();
    };

    const onFailure = (event: DirContentsScanFailedEvent) => {
      onFinished();

      this.runningScan_ = null;
      this.scanFailures_++;
      failureCallback(event.detail.error);

      if (maybeRunPendingRescan()) {
        return;
      }

      // Do not rescan for Guest OS (including Crostini) errors.
      // TODO(crbug/1293229): Guest OS currently reuses the Crostini error
      // string, but once it gets its own strings this needs to include both.
      if (event.detail.error.name === CROSTINI_CONNECT_ERR) {
        return;
      }

      if (this.scanFailures_ <= 1) {
        this.rescanLater(refresh);
      }
    };

    const onCancelled = () => {
      onFinished();
      cancelledCallback();
    };

    this.runningScan_ = dirContents;

    dirContents.addEventListener('dir-contents-scan-completed', onSuccess);
    dirContents.addEventListener('dir-contents-scan-updated', updatedCallback);
    dirContents.addEventListener('dir-contents-scan-failed', onFailure);
    dirContents.addEventListener('dir-contents-scan-canceled', onCancelled);
    dirContents.scan(refresh, invalidateCache);
  }

  /**
   * @param dirContents DirectoryContents instance. This must be a different
   *     instance from this.currentDirContents_.
   */
  private replaceDirectoryContents_(dirContents: DirectoryContents) {
    console.assert(
        this.currentDirContents_ !== dirContents,
        'Give directory contents instance must be different from current one.');
    dispatchSimpleEvent(this, 'begin-update-files');
    this.updateSelectionAndPublishEvent_(this.fileListSelection_, () => {
      const selectedEntries = this.getSelectedEntries_();
      const selectedIndices = this.fileListSelection_.selectedIndexes;

      // Restore leadIndex in case leadName no longer exists.
      const leadIndex = this.fileListSelection_.leadIndex;
      const leadEntry = this.getLeadEntry_();
      const isCheckSelectMode = this.fileListSelection_.getCheckSelectMode();

      this.currentDirContents_ = dirContents;
      this.currentDirContents_.replaceContextFileList();

      this.setSelectedEntries_(selectedEntries);
      this.fileListSelection_.leadIndex = leadIndex;
      this.setLeadEntry_(leadEntry);

      // If nothing is selected after update, then select file next to the
      // latest selection
      let forceChangeEvent = false;
      if (this.fileListSelection_.selectedIndexes.length === 0 &&
          selectedIndices.length !== 0) {
        const maxIdx = Math.max.apply(null, selectedIndices);
        this.selectIndex(
            Math.min(
                maxIdx - selectedIndices.length + 2,
                this.getFileList().length) -
            1);
        forceChangeEvent = true;
      } else if (isCheckSelectMode) {
        // Otherwise, ensure check select mode is retained if it was previously
        // active.
        this.fileListSelection_.setCheckSelectMode(true);
      }
      return forceChangeEvent;
    });

    dispatchSimpleEvent(this, 'end-update-files');
  }

  /**
   * Called when rename is done successfully.
   * Note: conceptually, DirectoryModel should work without this, because
   * entries can be renamed by other systems anytime and the Files app should
   * reflect it correctly.
   * TODO(hidehiko): investigate more background, and remove this if possible.
   *
   * @param oldEntry The old entry.
   * @param newEntry The new entry.
   * @return Resolves on completion.
   */
  onRenameEntry(oldEntry: Entry|FilesAppEntry, newEntry: Entry|FilesAppEntry):
      Promise<void> {
    return new Promise(resolve => {
      this.currentDirContents_.prefetchMetadata([newEntry], true, () => {
        // If the current directory is the old entry, then quietly change to
        // the new one.
        if (isSameEntry(oldEntry, this.getCurrentDirEntry())) {
          this.changeDirectoryEntry(
              newEntry as DirectoryEntry | FilesAppDirEntry);
        }

        // Replace the old item with the new item. oldEntry instance itself may
        // have been removed/replaced from the list during the async process, we
        // find an entry which should be replaced by checking toURL().
        const list = this.getFileList();
        let oldEntryExist = false;
        let newEntryExist = false;
        const oldEntryUrl = oldEntry.toURL();
        const newEntryUrl = newEntry.toURL();

        for (let i = 0; i < list.length; i++) {
          const item = list.item(i)!;
          const url = item.toURL();
          if (url === oldEntryUrl) {
            list.replaceItem(item, newEntry);
            oldEntryExist = true;
            break;
          }

          if (url === newEntryUrl) {
            newEntryExist = true;
          }
        }

        // When both old and new entries don't exist, it may be in the middle of
        // update process. In DirectoryContent.update deletion is executed at
        // first and insertion is executed as a async call. There is a chance
        // that this method is called in the middle of update process.
        if (!oldEntryExist && !newEntryExist) {
          list.push(newEntry);
        }

        resolve();
      });
    });
  }

  /**
   * Updates data model and selects new directory.
   * @param newDirectory Directory entry to be selected.
   * @return A promise which is resolved when new directory is selected. If
   *     current directory has changed during the operation, this will be
   *     rejected.
   */
  async updateAndSelectNewDirectory(newDirectory: DirectoryEntry):
      Promise<void> {
    // Refresh the cache.
    this.metadataModel_.notifyEntriesCreated([newDirectory]);
    const dirContents = this.currentDirContents_;
    const sequence = this.changeDirectorySequence_;
    await new Promise(resolve => {
      dirContents.prefetchMetadata([newDirectory], false, resolve);
    });

    // If current directory has changed during the prefetch, do not try to
    // select new directory.
    if (sequence !== this.changeDirectorySequence_) {
      return Promise.reject();
    }

    // If target directory is already in the list, just select it.
    const existing =
        this.getFileList().slice().filter(e => e.name === newDirectory.name);
    if (existing.length) {
      this.selectEntry(newDirectory);
    } else {
      this.fileListSelection_.beginChange();
      this.getFileList().splice(0, 0, newDirectory);
      this.selectEntry(newDirectory);
      this.fileListSelection_.endChange();
    }
  }

  /**
   * Gets the current MyFilesEntry.
   */
  getMyFiles(): null|FilesAppDirEntry {
    const {myFilesEntry} = getMyFiles(getStore().getState());
    return myFilesEntry;
  }

  async changeDirectoryFileData(fileData: FileData): Promise<boolean> {
    if (fileData.entry) {
      const result = await new Promise<boolean>(resolve => {
        this.changeDirectoryEntry(
            fileData.entry as DirectoryEntry, (ret) => resolve(ret));
      });
      return result;
    }

    // Increment the sequence value.
    const sequence = ++this.changeDirectorySequence_;
    this.stopActiveSearch_();

    // If there is on-going scan, cancel it.
    if (this.currentDirContents_.isScanning()) {
      this.currentDirContents_.cancelScan();
    }

    const unlock = await this.directoryChangeQueue_.lock();
    try {
      // Remove the watcher for the previous entry.
      await this.fileWatcher_.resetWatchedEntry();
      if (this.changeDirectorySequence_ !== sequence) {
        return false;
      }

      const newDirectoryContents = this.createDirectoryContents_(
          this.currentFileListContext_, undefined, fileData.key, '');
      if (!newDirectoryContents) {
        return false;
      }

      const previousDirEntry = this.currentDirContents_.getDirectoryEntry();
      const previousFileKey = this.currentDirContents_.getFileKey();
      const r = await new Promise<boolean>(
          resolve => this.clearAndScan_(newDirectoryContents, resolve));
      if (!r) {
        return r;
      }

      this.finalizeChangeDirectory_(
          previousDirEntry, previousFileKey, undefined, fileData.key);
    } catch (error) {
    } finally {
      unlock();
    }

    return true;
  }

  /**
   * Changes the current directory to the directory represented by
   * a DirectoryEntry or a fake entry.
   *
   * Dispatches the 'directory-changed' event when the directory is successfully
   * changed.
   *
   * Note : if this is called from UI, please consider to use DirectoryModel.
   * activateDirectoryEntry instead of this, which is higher-level function and
   * cares about the selection.
   *
   * @param dirEntry The entry of the new directory to be opened.
   * @param callback Executed if the directory loads successfully.
   */
  changeDirectoryEntry(
      dirEntry: DirectoryEntry|FilesAppDirEntry,
      callback?: (result: boolean) => void) {
    // Increment the sequence value.
    const sequence = ++this.changeDirectorySequence_;
    this.stopActiveSearch_();

    // When switching to MyFiles volume, we should use a FilesAppEntry if
    // available because it returns UI-only entries too, like Linux files and
    // Play files.
    const locationInfo = this.volumeManager_.getLocationInfo(dirEntry);
    if (locationInfo && locationInfo.rootType === RootType.DOWNLOADS &&
        locationInfo.isRootEntry) {
      dirEntry = this.getMyFiles()!;
    }

    // If there is on-going scan, cancel it.
    if (this.currentDirContents_.isScanning()) {
      this.currentDirContents_.cancelScan();
    }

    this.directoryChangeQueue_.run(async queueTaskCallback => {
      await this.fileWatcher_.changeWatchedDirectory(dirEntry);
      if (this.changeDirectorySequence_ !== sequence) {
        queueTaskCallback();
        return;
      }

      const newDirectoryContents = this.createDirectoryContents_(
          this.currentFileListContext_, dirEntry, dirEntry.toURL(), '');
      if (!newDirectoryContents) {
        queueTaskCallback();
        return;
      }

      const previousDirEntry = this.currentDirContents_.getDirectoryEntry();
      const previousFileKey = this.currentDirContents_.getFileKey();
      this.clearAndScan_(newDirectoryContents, result => {
        // Calls the callback of the method and inform it about success or lack
        // of thereof.
        if (callback) {
          callback(result);
        }
        // Notify that the current task of this.directoryChangeQueue_
        // is completed.
        setTimeout(queueTaskCallback, 0);
      });

      this.finalizeChangeDirectory_(
          previousDirEntry, previousFileKey, dirEntry, dirEntry.toURL());
    });
  }

  private async finalizeChangeDirectory_(
      previousDirEntry: UniversalDirectory|undefined,
      previousFileKey: FileKey|undefined,
      newDirectory: UniversalDirectory|undefined, fileKey: FileKey) {
    // For tests that open the dialog to empty directories, everything
    // is loaded at this point.
    testSendMessage('directory-change-complete');
    const previousVolumeInfo = previousDirEntry ?
        this.volumeManager_.getVolumeInfo(previousDirEntry) :
        null;
    // VolumeInfo for dirEntry.
    const currentVolumeInfo = this.getCurrentVolumeInfo();
    const event = new CustomEvent('directory-changed', {
      detail: {
        previousDirEntry,
        previousFileKey: previousFileKey,
        newDirEntry: newDirectory,
        newFileKey: fileKey,
        volumeChanged: (previousVolumeInfo !== currentVolumeInfo),
      },
    });
    await currentVolumeInfo?.resolveDisplayRoot();
    this.dispatchEvent(event);
    if (previousDirEntry) {
      // If we changed from a directory to another directory always clear
      // search and search query on directory change. When the Files app is
      // started previousDirEntry is undefined. For that case we must not
      // clear the search query as it may be part of the starting parameters.
      this.cachedSearch_ = {};
      this.store_.dispatch(clearSearch());
      this.clearLastSearchQuery();
    }
    // Notify the Store that the new directory has successfully changed.
    this.store_.dispatch(changeDirectory({
      to: newDirectory,
      toKey: fileKey,
      status: PropStatus.SUCCESS,
    }));
  }

  /**
   * Activates the given directory.
   * This method:
   *  - Changes the current directory, if the given directory is not the current
   * directory.
   *  - Clears the selection, if the given directory is the current directory.
   *
   * @param dirEntry The entry of the new directory to be opened.
   * @param callback Executed if the directory loads successfully.
   */
  activateDirectoryEntry(
      dirEntry: DirectoryEntry|FilesAppDirEntry, callback?: VoidCallback) {
    const currentDirectoryEntry = this.getCurrentDirEntry();
    if (currentDirectoryEntry && isSameEntry(dirEntry, currentDirectoryEntry)) {
      // On activating the current directory, clear the selection on the
      // filelist.
      this.clearSelection();
    } else {
      // Otherwise, changes the current directory.
      this.changeDirectoryEntry(dirEntry, callback);
    }
  }

  /**
   * Clears the selection in the file list.
   */
  clearSelection() {
    this.setSelectedEntries_([]);
  }

  /**
   * Creates an object which could say whether directory has changed while it
   * has been active or not. Designed for long operations that should be
   * cancelled if the used change current directory.
   * @return Created object.
   */
  createDirectoryChangeTracker(): DirectoryChangeTracker {
    const tracker = {
      dm_: this,
      active_: false,
      hasChanged: false,

      start: function() {
        if (!this.active_) {
          this.dm_.numChangeTrackerRunning_++;
          this.dm_.addEventListener(
              'directory-changed', this.onDirectoryChange_);
          this.active_ = true;
          this.hasChanged = false;
        }
      },

      stop: function() {
        if (this.active_) {
          this.dm_.numChangeTrackerRunning_--;
          this.dm_.removeEventListener(
              'directory-changed', this.onDirectoryChange_);
          this.active_ = false;
        }
      },

      onDirectoryChange_: function(_event: Event) {
        tracker.stop();
        tracker.hasChanged = true;
      },
    };
    return tracker;
  }

  /**
   * @param entry Entry to be selected.
   */
  selectEntry(entry: Entry) {
    const fileList = this.getFileList();
    for (let i = 0; i < fileList.length; i++) {
      if (fileList.item(i)!.toURL() === entry.toURL()) {
        this.selectIndex(i);
        return;
      }
    }
  }

  /**
   * @param entries Array of entries.
   */
  selectEntries(entries: Entry[]) {
    // URLs are needed here, since we are comparing Entries by URLs.
    const urls = entriesToURLs(entries);
    const fileList = this.getFileList();
    this.fileListSelection_.beginChange();
    this.fileListSelection_.unselectAll();
    for (let i = 0; i < fileList.length; i++) {
      if (urls.indexOf(fileList.item(i)!.toURL()) >= 0) {
        this.fileListSelection_.setIndexSelected(i, true);
      }
    }
    this.fileListSelection_.endChange();
  }

  /**
   * @param index Index of file.
   */
  selectIndex(index: number) {
    if (index >= this.getFileList().length) {
      return;
    }

    // If a list bound with the model it will do scrollIndexIntoView(index).
    this.fileListSelection_.selectedIndex = index;
  }

  /**
   * Handles update of VolumeInfoList.
   * @param event Event of VolumeInfoList's 'splice'.
   */
  private onVolumeInfoListUpdated_(event: SpliceEvent) {
    const spliceEventDetail = event.detail;
    // Fallback to the default volume's root if the current volume is unmounted.
    if (this.hasCurrentDirEntryBeenUnmounted_(spliceEventDetail.removed)) {
      this.volumeManager_.getDefaultDisplayRoot().then((displayRoot) => {
        if (displayRoot) {
          this.changeDirectoryEntry(displayRoot);
        }
      });
    }

    // If a volume within My files or removable root is mounted/unmounted rescan
    // its contents.
    const currentDir = this.getCurrentDirEntry();
    const affectedVolumes =
        spliceEventDetail.added.concat(spliceEventDetail.removed);
    for (const volume of affectedVolumes) {
      if (isSameEntry(currentDir, volume.prefixEntry)) {
        this.rescan(false);
        break;
      }
    }

    // If the current directory is the OneDrive placeholder and the real
    // OneDrive is mounted, switch to it.
    if (isSkyvaultV2Enabled() && currentDir &&
        isOneDrivePlaceholder(currentDir)) {
      for (const newVolume of spliceEventDetail.added) {
        if (isOneDrive(newVolume)) {
          newVolume.resolveDisplayRoot().then((displayRoot: DirectoryEntry) => {
            this.changeDirectoryEntry(displayRoot);
          });
        }
      }
    }

    if (spliceEventDetail.added.length !== 1) {
      return;
    }
    // Redirect to newly mounted volume when:
    // * There is no directory currently selected, meaning it's the first volume
    //   to appear.
    // * A new file backed provided volume is mounted, then redirect to it in
    //   the focused window, because this means a zip file has been mounted.
    //   Note, that this is a temporary solution for https://crbug.com/427776.
    // * Crostini is mounted, redirect if it is the currently selected dir.
    if (!currentDir ||
        (window.isFocused && window.isFocused() &&
         spliceEventDetail.added[0].volumeType === VolumeType.PROVIDED &&
         spliceEventDetail.added[0].source === Source.FILE) ||
        (spliceEventDetail.added[0].volumeType === VolumeType.CROSTINI &&
         this.getCurrentRootType() === RootType.CROSTINI) ||
        // TODO(crbug/1293229): Don't redirect if the user is looking at a
        // different Guest OS folder.
        (isGuestOs(spliceEventDetail.added[0].volumeType) &&
         this.getCurrentRootType() === RootType.GUEST_OS)) {
      // Resolving a display root on FSP volumes is instant, despite the
      // asynchronous call.
      spliceEventDetail.added[0].resolveDisplayRoot().then(
          (_displayRoot: DirectoryEntry) => {
            // Only change directory if "currentDir" hasn't changed during the
            // display root resolution and if there isn't a directory change in
            // progress, because other part of the system will eventually change
            // the directory.
            if (currentDir === this.getCurrentDirEntry() &&
                this.numChangeTrackerRunning_ === 0) {
              this.changeDirectoryEntry(spliceEventDetail.added[0].displayRoot);
            }
          });
    }
  }

  /**
   * Returns whether the current directory entry has been unmounted.
   *
   * @param removedVolumes The removed volumes.
   */
  private hasCurrentDirEntryBeenUnmounted_(removedVolumes: VolumeInfo[]):
      boolean {
    const entry = this.getCurrentDirEntry();
    if (!entry) {
      return false;
    }

    if (!isFakeEntry(entry)) {
      return !this.volumeManager_.getVolumeInfo(entry);
    }

    const rootType = this.getCurrentRootType();
    for (const volume of removedVolumes) {
      if (rootType && volume.fakeEntries[rootType]) {
        return true;
      }
      // The removable root is selected and one of its child partitions has been
      // unmounted.
      if (volume.prefixEntry === entry) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if directory search should be used for the entry and query.
   *
   * @param entry Directory entry.
   * @param query Search query string.
   * @return True if directory search should be used for the entry and query.
   */
  isSearchDirectory(entry?: DirectoryEntry|FilesAppEntry, query?: string):
      boolean {
    if (!entry) {
      return !!query;
    }
    const rootType = getRootType(entry);
    if (isRecentRootType(rootType) || rootType === RootType.CROSTINI ||
        rootType === RootType.DRIVE_FAKE_ROOT) {
      return true;
    }
    if (rootType === RootType.MY_FILES) {
      return false;
    }

    if ((query || '').trimStart()) {
      return true;
    }

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (locationInfo &&
        (locationInfo.rootType === RootType.MEDIA_VIEW ||
         locationInfo.isSpecialSearchRoot)) {
      return true;
    }
    return false;
  }

  /**
   * Creates scanner factory for the entry and query.
   *
   * @param entry Directory entry.
   * @param query Search query string.
   * @param options search options.
   * @return The factory to create ContentScanner instance.
   */
  createScannerFactory(
      fileKey: FileKey, entry?: DirectoryEntry|FilesAppEntry, query?: string,
      options?: SearchOptions): () => ContentScanner {
    if (!entry && !!fileKey) {
      // Store-based scanner, it doesn't use Entry. E.g: Materialized View.
      return () => {
        return new StoreScanner(fileKey);
      };
    }

    assert(entry);

    const sanitizedQuery = (query || '').trimStart();
    const locationInfo = this.volumeManager_.getLocationInfo(entry);

    if (isRecentScan(entry, options)) {
      return () => {
        return new RecentContentScanner(
            sanitizedQuery, 30, this.volumeManager_, entry.sourceRestriction,
            getFileCategory(entry, sanitizedQuery, options));
      };
    }
    // TODO(b/271485133): Make sure the entry here is a fake entry, not real
    // volume entry.
    const rootType = getRootType(entry);
    if (rootType === RootType.CROSTINI) {
      return () => {
        return new CrostiniMounter();
      };
    }
    if (rootType === RootType.GUEST_OS) {
      return () => {
        return new GuestOsMounter((entry as GuestOsPlaceholder).guest_id);
      };
    }
    if (rootType === RootType.MY_FILES) {
      return () => {
        return new DirectoryContentScanner(entry as FilesAppDirEntry);
      };
    }
    if (rootType === RootType.DRIVE_FAKE_ROOT) {
      return () => {
        return new EmptyContentScanner();
      };
    }
    if (rootType === RootType.TRASH) {
      return () => {
        return new TrashContentScanner(this.volumeManager_);
      };
    }
    if (isOneDrivePlaceholder(entry)) {
      return () => {
        return new EmptyContentScanner();
      };
    }
    if (sanitizedQuery) {
      return () => {
        return new SearchV2ContentScanner(
            this.volumeManager_,
            // TODO(b/289003444): Fix this cast.
            entry as DirectoryEntry | FilesAppDirEntry, sanitizedQuery,
            options || getDefaultSearchOptions());
      };
    }
    if (locationInfo && locationInfo.rootType === RootType.MEDIA_VIEW) {
      return () => {
        return new MediaViewContentScanner(entry as DirectoryEntry);
      };
    }
    if (locationInfo && locationInfo.isRootEntry &&
        locationInfo.isSpecialSearchRoot) {
      // Drive special search.
      let searchType: chrome.fileManagerPrivate.SearchType;
      switch (locationInfo.rootType) {
        case RootType.DRIVE_OFFLINE:
          searchType = chrome.fileManagerPrivate.SearchType.OFFLINE;
          break;
        case RootType.DRIVE_SHARED_WITH_ME:
          searchType = chrome.fileManagerPrivate.SearchType.SHARED_WITH_ME;
          break;
        case RootType.DRIVE_RECENT:
          searchType = chrome.fileManagerPrivate.SearchType.EXCLUDE_DIRECTORIES;
          break;
        default:
          // Unknown special search entry.
          throw new Error('Unknown special search type.');
      }
      return () => {
        return new DriveMetadataSearchContentScanner(searchType);
      };
    }
    // Local fetch or search.
    return () => {
      return new DirectoryContentScanner(entry as DirectoryEntry);
    };
  }

  /**
   * Creates directory contents for the entry and query.
   *
   * @param context File list context.
   * @param entry Current directory.
   * @param query Search query string.
   * @param options Search options.
   * @return Directory contents.
   */
  private createDirectoryContents_(
      context: FileListContext, entry?: DirectoryEntry|FilesAppDirEntry,
      fileKey?: FileKey, query?: string,
      options?: SearchOptions): DirectoryContents {
    if (!entry && !fileKey) {
      console.error('`fileKey` or `entry` must be provided');
    }

    const isSearch = this.isSearchDirectory(entry, query);
    const key = fileKey ?? entry!.toURL();
    const scannerFactory =
        this.createScannerFactory(key, entry, query, options);
    return new DirectoryContents(context, isSearch, entry, key, scannerFactory);
  }

  /**
   * Gets the last search query.
   * @return the last search query.
   */
  getLastSearchQuery(): string {
    return this.lastSearchQuery_;
  }

  /**
   * Clears the last search query with the empty string.
   */
  clearLastSearchQuery() {
    this.lastSearchQuery_ = '';
  }

  /**
   * Performs search and displays results. The search type is dependent on the
   * current directory. If we are currently on drive, server side content search
   * over drive mount point. If the current directory is not on the drive, file
   * name search over current directory will be performed.
   *
   * @param query Query that will be searched for.
   * @param options Search options, such as file type, etc.
   */
  private search_(query: string, options: SearchOptions) {
    this.lastSearchQuery_ = query;
    this.stopActiveSearch_();
    const currentDirEntry = this.getCurrentDirEntry();
    if (!currentDirEntry) {
      // Not yet initialized. Do nothing.
      return;
    }

    const sequence = ++this.changeDirectorySequence_;
    this.directoryChangeQueue_.run(callback => {
      if (this.changeDirectorySequence_ !== sequence) {
        callback();
        return;
      }

      assert(currentDirEntry);
      if (!(query || '').trimStart()) {
        if (this.isSearching()) {
          const newDirContents = this.createDirectoryContents_(
              this.currentFileListContext_, currentDirEntry,
              currentDirEntry.toURL());
          this.clearAndScan_(newDirContents, callback);
        } else {
          callback();
        }
        return;
      }

      const newDirContents = this.createDirectoryContents_(
          this.currentFileListContext_, currentDirEntry,
          currentDirEntry.toURL(), query, options);
      if (!newDirContents) {
        callback();
        return;
      }

      this.store_.dispatch(updateSearch(
          {query: query, status: PropStatus.STARTED, options: undefined}));
      this.onSearchCompleted_ = () => {
        // Notify the store-aware parts.
        this.store_.dispatch(updateSearch({
          status: PropStatus.SUCCESS,
          query: undefined,
          options: undefined,
        }));
      };
      this.addEventListener('cur-dir-scan-completed', this.onSearchCompleted_);
      this.clearAndScan_(newDirContents, callback);
    });
  }

  /**
   * In case the search was active, remove listeners and send notifications on
   * its canceling.
   */
  private stopActiveSearch_() {
    if (!this.isSearching()) {
      return;
    }

    if (this.onSearchCompleted_) {
      this.removeEventListener(
          'cur-dir-scan-completed', this.onSearchCompleted_);
      this.onSearchCompleted_ = null;
    }
  }

  /**
   * Update the file list when certain IO task is finished. To keep the file
   * list refresh for non-watchable fake directory entries and volumes, we need
   * to explicitly subscribe to the IO task status event, and manually refresh.
   */
  private updateFileListAfterIoTask_(
      event: chrome.fileManagerPrivate.ProgressStatus) {
    let rescan = false;
    const fakeDirectoryEntryRootTypes = new Set([
      RootType.RECENT,
      RootType.TRASH,
    ]);
    const currentRootType = this.getCurrentRootType();
    const currentVolumeInfo = this.getCurrentVolumeInfo();
    if (currentRootType && fakeDirectoryEntryRootTypes.has(currentRootType)) {
      // Refresh if non-watchable fake directory entry.
      rescan = true;
    } else if (currentVolumeInfo && !currentVolumeInfo.watchable) {
      // Refresh if non-watchable volume.
      rescan = true;
    }
    if (!rescan) {
      return;
    }
    const isIOTaskFinished =
        event.state === chrome.fileManagerPrivate.IoTaskState.SUCCESS;
    if (isIOTaskFinished) {
      this.rescanLater(/* refresh= */ false, /* invalidateCache= */ true);
    }
  }
}
