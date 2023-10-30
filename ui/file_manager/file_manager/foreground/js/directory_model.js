// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {Aggregator, AsyncQueue} from '../../common/js/async_util.js';
import {convertURLsToEntries, entriesToURLs, isFakeEntry, isGuestOs, isNativeEntry, isOneDriveId, isRecentRootType, isSameEntry, urlToEntry} from '../../common/js/entry_utils.js';
import {EntryList, GuestOsPlaceholder, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled} from '../../common/js/flags.js';
import {recordMediumCount} from '../../common/js/metrics.js';
import {getEntryLabel} from '../../common/js/translations.js';
import {testSendMessage} from '../../common/js/util.js';
import {isNative, VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus, SearchLocation, SearchOptions, State, Volume, VolumeId} from '../../externs/ts/state.js';
// @ts-ignore: error TS6133: 'Store' is declared but its value is never read.
import {Store} from '../../externs/ts/store.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {getMyFiles} from '../../state/ducks/all_entries.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {clearSearch, getDefaultSearchOptions, updateSearch} from '../../state/ducks/search.js';
import {getFileData, getStore, getVolume} from '../../state/store.js';

import {constants} from './constants.js';
import {ContentScanner, CrostiniMounter, DirectoryContents, DirectoryContentScanner, DriveMetadataSearchContentScanner, FileFilter, FileListContext, GuestOsMounter, MediaViewContentScanner, RecentContentScanner, SearchV2ContentScanner, TrashContentScanner} from './directory_contents.js';
import {FileListModel} from './file_list_model.js';
import {FileWatcher} from './file_watcher.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {FileListSelectionModel, FileListSingleSelectionModel} from './ui/file_list_selection_model.js';
import {ListSelectionModel} from './ui/list_selection_model.js';
import {ListSingleSelectionModel} from './ui/list_single_selection_model.js';

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
 * @param {!DirectoryEntry|!FilesAppEntry} entry Directory entry.
 * @param {SearchOptions=} options search options.
 * @private
 */
function isRecentScan(entry, options) {
  // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
  // 'FileSystemDirectoryEntry | FilesAppEntry'.
  if (isRecentRootType(entry.rootType)) {
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
 * @param {!FakeEntry} entry
 * @param {string|undefined} query
 * @param {!SearchOptions|undefined} options
 */
function getFileCategory(entry, query, options) {
  if (query) {
    if (options) {
      return options.fileCategory;
    }
  }
  return entry.fileCategory;
}

/**
 * Data model of the file manager.
 */
export class DirectoryModel extends EventTarget {
  /**
   * @param {boolean} singleSelection True if only one file could be selected
   *                                  at the time.
   * @param {FileFilter} fileFilter Instance of FileFilter.
   * @param {!MetadataModel} metadataModel Metadata model.
   *     service.
   * @param {!VolumeManager} volumeManager The volume manager.
   * @param {!FileOperationManager} fileOperationManager File operation manager.
   */
  constructor(
      singleSelection, fileFilter, metadataModel, volumeManager,
      // @ts-ignore: error TS6133: 'fileOperationManager' is declared but its
      // value is never read.
      fileOperationManager) {
    super();

    this.fileListSelection_ = singleSelection ?
        new FileListSingleSelectionModel() :
        new FileListSelectionModel();

    this.runningScan_ = null;
    this.pendingScan_ = null;
    this.pendingRescan_ = null;
    this.rescanTime_ = null;
    this.scanFailures_ = 0;
    this.changeDirectorySequence_ = 0;
    this.cachedSearch_ = {};

    /** @private @type {?function(Event): void} */
    this.onSearchCompleted_ = null;

    /**
     * @private @type {boolean}
     */
    this.ignoreCurrentDirectoryDeletion_ = false;

    this.directoryChangeQueue_ = new AsyncQueue();

    /**
     * Number of running directory change trackers.
     * @private @type {number}
     */
    this.numChangeTrackerRunning_ = 0;

    this.rescanAggregator_ =
        new Aggregator(this.rescanSoon.bind(this, true), 500);

    this.fileFilter_ = fileFilter;
    this.fileFilter_.addEventListener(
        'changed', this.onFilterChanged_.bind(this));

    this.currentFileListContext_ =
        new FileListContext(fileFilter, metadataModel, volumeManager);
    this.currentDirContents_ =
        // @ts-ignore: error TS2345: Argument of type 'null' is not assignable
        // to parameter of type 'FileSystemDirectoryEntry | FilesAppDirEntry |
        // FakeEntry'.
        new DirectoryContents(this.currentFileListContext_, false, null, () => {
          // @ts-ignore: error TS2345: Argument of type 'null' is not assignable
          // to parameter of type 'FileSystemDirectoryEntry | FilesAppDirEntry'.
          return new DirectoryContentScanner(null);
        });

    /**
     * Empty file list which is used as a dummy for inactive view of file list.
     * @private @type {!FileListModel}
     */
    this.emptyFileList_ = new FileListModel(metadataModel);

    this.metadataModel_ = metadataModel;

    this.volumeManager_ = volumeManager;
    this.volumeManager_.volumeInfoList.addEventListener(
        'splice', this.onVolumeInfoListUpdated_.bind(this));

    /**
     * File watcher.
     * @private @type {!FileWatcher}
     * @const
     */
    this.fileWatcher_ = new FileWatcher();
    this.fileWatcher_.addEventListener(
        'watcher-directory-changed',
        this.onWatcherDirectoryChanged_.bind(this));
    // For non-watchable directories (e.g. FakeEntry) and volumes (MTP) we need
    // to subscribe to the IOTask and manually refresh.
    chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(
        this.updateFileListAfterIOTask_.bind(this));

    /** @private @type {string} */
    this.lastSearchQuery_ = '';

    /** @private @type {?Record<!VolumeId, !Volume>} */
    this.volumes_ = null;

    /** @private @type {!Store} */
    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  /** @param {!State} state latest state from the store. */
  onStateChanged(state) {
    this.handleDirectoryState_(state);
    this.handleSearchState_(state);
  }

  /**
   * Handles the current directory slice of the store's state.
   * @param {!State} state latest state from the store.
   * @private
   */
  handleDirectoryState_(state) {
    const currentEntry = this.getCurrentDirEntry();
    const currentURL = currentEntry ? currentEntry.toURL() : null;
    let newURL = state.currentDirectory ? state.currentDirectory.key : null;

    // Observe volume changes.
    if (this.volumes_ !== state.volumes) {
      this.onStateVolumeChanged_(state);
      this.volumes_ = state.volumes;
    }

    // If the directory is the same, ignore it.
    if (currentURL === newURL) {
      return;
    }

    // When something changed the current directory status to STARTED, Here we
    // initiate the actual change and will update to SUCCESS at the end.
    if (state.currentDirectory?.status === PropStatus.STARTED) {
      newURL = /** @type {string} */ (newURL);
      const entry =
          state.allEntries[newURL] ? state.allEntries[newURL].entry : null;

      if (!entry) {
        // TODO(lucmult): Fix potential race condition in this await/then.
        urlToEntry(newURL).then((entry) => {
          if (!entry) {
            console.error(`Failed to find the new directory key ${newURL}`);
            return;
          }
          // Initiate the directory change.
          this.changeDirectoryEntry(/** @type {!DirectoryEntry} */ (entry));
        });
        return;
      }

      // Initiate the directory change.
      this.changeDirectoryEntry(/** @type {!DirectoryEntry} */ (entry));
    }
  }

  /**
   * Reacts to changes in the search state of the store. If the search changed
   * and the query is not empty, this method triggers a new directory search.
   * @param {!State} state
   * @private
   */
  handleSearchState_(state) {
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
    // @ts-ignore: error TS2322: Type 'SearchData | undefined' is not assignable
    // to type '{}'.
    this.cachedSearch_ = search;

    // We change the search state (STARTED, SUCCESS, etc.) so only trigger
    // a new search if the query or the options have changed.
    if (!search) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'query' does not exist on type '{}'.
    if (!lastSearch || lastSearch.query !== search.query ||
        // @ts-ignore: error TS2339: Property 'options' does not exist on type
        // '{}'.
        lastSearch.options !== search.options) {
      this.search_(
          search.query || '', search.options || getDefaultSearchOptions());
    }
  }

  /**
   * Disposes the directory model by removing file watchers.
   */
  dispose() {
    this.fileWatcher_.dispose();
  }

  /**
   * @return {FileListModel} Files in the current directory.
   */
  getFileList() {
    return this.currentFileListContext_.fileList;
  }

  /**
   * @return {!FileListModel} File list which is always empty.
   */
  getEmptyFileList() {
    return this.emptyFileList_;
  }

  /**
   * @return {!FileListSelectionModel|!FileListSingleSelectionModel} Selection
   * in the fileList.
   */
  getFileListSelection() {
    return this.fileListSelection_;
  }

  /**
   * Obtains current volume information.
   * @return {import('../../externs/volume_info.js').VolumeInfo}
   */
  getCurrentVolumeInfo() {
    const entry = this.getCurrentDirEntry();
    if (!entry) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'VolumeInfo'.
      return null;
    }
    // @ts-ignore: error TS2322: Type 'VolumeInfo | null' is not assignable to
    // type 'VolumeInfo'.
    return this.volumeManager_.getVolumeInfo(entry);
  }

  /**
   * @return {?VolumeManagerCommon.RootType} Root type of current root, or null
   *     if not found.
   */
  getCurrentRootType() {
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
   * @return {!Array<string>}
   */
  getPrefetchPropertyNames() {
    return this.currentFileListContext_.prefetchPropertyNames;
  }

  /**
   * @return {boolean} True if the current directory is read only. If there is
   *     no entry set, then returns true.
   */
  isReadOnly() {
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
   * @return {boolean} True if entries in the current directory can be deleted.
   *     Similar to !isReadOnly() except that we allow items in the read-only
   *     Trash root to be deleted. If there is no entry set, then returns false.
   */
  canDeleteEntries() {
    const currentDirEntry = this.getCurrentDirEntry();
    if (currentDirEntry &&
        // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
        // 'FileSystemDirectoryEntry | FilesAppDirEntry | FakeEntry'.
        currentDirEntry.rootType === VolumeManagerCommon.RootType.TRASH) {
      return true;
    }
    return !this.isReadOnly();
  }

  /**
   * @return {boolean} True if the a scan is active.
   */
  isScanning() {
    return this.currentDirContents_.isScanning();
  }

  /**
   * @return {boolean} True if search is in progress.
   */
  isSearching() {
    return this.currentDirContents_.isSearch();
  }

  /**
   * @return {boolean} True if it's on Drive.
   */
  isOnDrive() {
    return this.isCurrentRootVolumeType_(VolumeManagerCommon.VolumeType.DRIVE);
  }

  /**
   * @return {boolean} True if it's on MTP volume.
   */
  isOnMTP() {
    return this.isCurrentRootVolumeType_(VolumeManagerCommon.VolumeType.MTP);
  }

  /**
   * @return {boolean} True if the current volume is provided by FuseBox.
   */
  isOnFuseBox() {
    const info = this.getCurrentVolumeInfo();
    return info ? info.diskFileSystemType === 'fusebox' : false;
  }

  /**
   * @return {boolean} True if it's on a Linux native volume.
   */
  isOnNative() {
    const rootType = this.getCurrentRootType();
    return rootType != null && !isRecentRootType(rootType) &&
        isNative(VolumeManagerCommon.getVolumeTypeFromRootType(rootType));
  }

  /**
   * @return {boolean} True if the current volume is blocked by DLP.
   */
  isDlpBlocked() {
    if (!isDlpEnabled()) {
      return false;
    }
    const info = this.getCurrentVolumeInfo();
    return info ? this.volumeManager_.isDisabled(info.volumeType) : false;
  }

  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume Type
   * @return {boolean} True if current root volume type is equal to specified
   *     volume type.
   * @private
   */
  isCurrentRootVolumeType_(volumeType) {
    const rootType = this.getCurrentRootType();
    return rootType != null && !isRecentRootType(rootType) &&
        VolumeManagerCommon.getVolumeTypeFromRootType(rootType) === volumeType;
  }

  /**
   * Updates the selection by using the updateFunc and publish the change event.
   * If updateFunc returns true, it force to dispatch the change event even if
   * the selection index is not changed.
   *
   * @param {ListSelectionModel|ListSingleSelectionModel} selection
   *     Selection to be updated.
   * @param {function(): boolean} updateFunc Function updating the selection.
   * @private
   */
  updateSelectionAndPublishEvent_(selection, updateFunc) {
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
      const event = new Event('change');
      // The selection status (selected or not) is not changed because
      // this event is caused by the change of selected item.
      // @ts-ignore: error TS2339: Property 'changes' does not exist on type
      // 'Event'.
      event.changes = [];
      selection.dispatchEvent(event);
    }
  }

  /**
   * Sets to ignore current directory deletion. This method is used to prevent
   * going up to the volume root with the deletion of current directory by
   * rename operation in directory tree.
   * @param {boolean} value True to ignore current directory deletion.
   */
  setIgnoringCurrentDirectoryDeletion(value) {
    this.ignoreCurrentDirectoryDeletion_ = value;
  }

  /**
   * Invoked when a change in the directory is detected by the watcher.
   * @param {Event} event Event object.
   * @private
   */
  onWatcherDirectoryChanged_(event) {
    const directoryEntry = this.getCurrentDirEntry();

    if (!this.ignoreCurrentDirectoryDeletion_) {
      // If the change is deletion of currentDir, move up to its parent
      // directory.
      directoryEntry.getDirectory(
          directoryEntry.fullPath, {create: false}, () => {}, async () => {
            const volumeInfo =
                this.volumeManager_.getVolumeInfo(assert(directoryEntry));
            if (volumeInfo) {
              const displayRoot = await volumeInfo.resolveDisplayRoot();
              this.changeDirectoryEntry(displayRoot);
            }
          });
    }

    // @ts-ignore: error TS2339: Property 'changedFiles' does not exist on type
    // 'Event'.
    if (event.changedFiles) {
      // @ts-ignore: error TS7034: Variable 'addedOrUpdatedFileUrls' implicitly
      // has type 'any[]' in some locations where its type cannot be determined.
      const addedOrUpdatedFileUrls = [];
      // @ts-ignore: error TS7034: Variable 'deletedFileUrls' implicitly has
      // type 'any[]' in some locations where its type cannot be determined.
      let deletedFileUrls = [];
      // @ts-ignore: error TS7006: Parameter 'change' implicitly has an 'any'
      // type.
      event.changedFiles.forEach(change => {
        if (change.changes.length === 1 && change.changes[0] === 'delete') {
          deletedFileUrls.push(change.url);
        } else {
          addedOrUpdatedFileUrls.push(change.url);
        }
      });

      // @ts-ignore: error TS7005: Variable 'addedOrUpdatedFileUrls' implicitly
      // has an 'any[]' type.
      convertURLsToEntries(addedOrUpdatedFileUrls)
          .then(result => {
            // @ts-ignore: error TS7005: Variable 'deletedFileUrls' implicitly
            // has an 'any[]' type.
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
   * @private
   */
  async onFilterChanged_() {
    const currentDirectory = this.getCurrentDirEntry();
    if (currentDirectory && isNativeEntry(currentDirectory) &&
        !this.fileFilter_.filter(
            /** @type {!DirectoryEntry} */ (currentDirectory))) {
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
   * @param {!State} state latest state from the store.
   * @private
   */
  onStateVolumeChanged_(state) {
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
          // @ts-ignore: error TS18048: 'state.currentDirectory' is possibly
          // 'undefined'.
          getFileData(state, state.currentDirectory.key);
      const currentDirectoryOnOdfs =
          isOneDriveId(getVolume(state, currentDirectoryFileData)?.providerId);
      if (currentDirectoryOnOdfs) {
        const {myFilesEntry} = /**
                                  @type {{myFilesVolume: (Volume|null),
                                      myFilesEntry: (VolumeEntry|EntryList)}}
                                */
            (getMyFiles(state));
        const myFilesRootKey = myFilesEntry.toURL();
        this.store_.dispatch(changeDirectory({toKey: myFilesRootKey}));
      }
    }
  }

  /**
   * Returns the filter.
   * @return {FileFilter} The file filter.
   */
  getFileFilter() {
    return this.fileFilter_;
  }

  /**
   * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} Current directory.
   */
  getCurrentDirEntry() {
    return this.currentDirContents_.getDirectoryEntry();
  }

  /**
   * @public
   * @return {string}
   */
  getCurrentDirName() {
    const dirEntry = this.getCurrentDirEntry();
    if (!dirEntry) {
      return '';
    }

    const locationInfo = this.volumeManager_.getLocationInfo(dirEntry);
    return getEntryLabel(locationInfo, dirEntry);
  }

  /**
   * @return {Array<Entry>} Array of selected entries.
   * @private
   */
  getSelectedEntries_() {
    const indexes = this.fileListSelection_.selectedIndexes;
    const fileList = this.getFileList();
    if (fileList) {
      return indexes.map(i => fileList.item(i));
    }
    return [];
  }

  /**
   * @param {Array<Entry>} value List of selected entries.
   * @private
   */
  setSelectedEntries_(value) {
    const indexes = [];
    const fileList = this.getFileList();
    const urls = entriesToURLs(value);

    for (let i = 0; i < fileList.length; i++) {
      if (urls.indexOf(fileList.item(i).toURL()) !== -1) {
        indexes.push(i);
      }
    }
    this.fileListSelection_.selectedIndexes = indexes;
  }

  /**
   * @return {Entry} Lead entry.
   * @private
   */
  getLeadEntry_() {
    const index = this.fileListSelection_.leadIndex;
    // @ts-ignore: error TS2322: Type 'FileSystemEntry | null' is not assignable
    // to type 'FileSystemEntry'.
    return index >= 0 ?
        /** @type {Entry} */ (this.getFileList().item(index)) :
        null;
  }

  /**
   * @param {Entry} value The new lead entry.
   * @private
   */
  setLeadEntry_(value) {
    const fileList = this.getFileList();
    for (let i = 0; i < fileList.length; i++) {
      if (isSameEntry(/** @type {Entry} */ (fileList.item(i)), value)) {
        this.fileListSelection_.leadIndex = i;
        return;
      }
    }
  }

  /**
   * Schedule rescan with short delay.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean=} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  rescanSoon(refresh, invalidateCache = false) {
    this.scheduleRescan(SHORT_RESCAN_INTERVAL, refresh, invalidateCache);
  }

  /**
   * Schedule rescan with delay. Designed to handle directory change
   * notification.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean=} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  rescanLater(refresh, invalidateCache = false) {
    this.scheduleRescan(SIMULTANEOUS_RESCAN_INTERVAL, refresh, invalidateCache);
  }

  /**
   * Schedule rescan with delay. If another rescan has been scheduled does
   * nothing. File operation may cause a few notifications what should cause
   * a single refresh.
   * @param {number} delay Delay in ms after which the rescan will be performed.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean=} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  scheduleRescan(delay, refresh, invalidateCache = false) {
    if (this.rescanTime_) {
      if (this.rescanTime_ <= Date.now() + delay) {
        return;
      }
      clearTimeout(this.rescanTimeoutId_);
    }

    const sequence = this.changeDirectorySequence_;

    this.rescanTime_ = Date.now() + delay;
    this.rescanTimeoutId_ = setTimeout(() => {
      this.rescanTimeoutId_ = null;
      if (sequence === this.changeDirectorySequence_) {
        this.rescan(refresh, invalidateCache);
      }
    }, delay);
  }

  /**
   * Cancel a rescan on timeout if it is scheduled.
   * @private
   */
  clearRescanTimeout_() {
    this.rescanTime_ = null;
    if (this.rescanTimeoutId_) {
      clearTimeout(this.rescanTimeoutId_);
      this.rescanTimeoutId_ = null;
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
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean=} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  rescan(refresh, invalidateCache = false) {
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
        dispatchSimpleEvent(this, 'rescan-completed');
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
   * @param {DirectoryContents} newDirContents New DirectoryContents instance to
   *     replace currentDirContents_.
   * @param {function(boolean):void} callback Callback with result. True if the
   *     scan is completed successfully, false if the scan is failed.
   * @private
   */
  clearAndScan_(newDirContents, callback) {
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

      dispatchSimpleEvent(this, 'scan-completed');
      callback(true);
    };

    /** @param {DOMError} error error. */
    const onFailed = error => {
      if (cancelled) {
        return;
      }

      const event = new Event('scan-failed');
      // @ts-ignore: error TS2339: Property 'error' does not exist on type
      // 'Event'.
      event.error = error;
      this.dispatchEvent(event);
      callback(false);
    };

    const onUpdated = () => {
      if (cancelled) {
        return;
      }

      if (this.changeDirectorySequence_ !== sequence) {
        cancelled = true;
        dispatchSimpleEvent(this, 'scan-cancelled');
        callback(false);
        return;
      }

      dispatchSimpleEvent(this, 'scan-updated');
    };

    const onCancelled = () => {
      if (cancelled) {
        return;
      }

      cancelled = true;
      dispatchSimpleEvent(this, 'scan-cancelled');
      callback(false);
    };

    // Clear metadata information for the old (no longer visible) items in the
    // file list.
    const fileList = this.getFileList();
    const removedUrls = [];
    for (let i = 0; i < fileList.length; i++) {
      removedUrls.push(fileList.item(i).toURL());
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
        this.metadataModel_.get(
            // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry |
            // FilesAppDirEntry | FakeEntry' is not assignable to type
            // 'FileSystemEntry'.
            [currentEntry],
            constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES.concat(
                constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES));
      }
    }

    // Clear the table, and start scanning.
    // @ts-ignore: error TS2555: Expected at least 3 arguments, but got 2.
    fileList.splice(0, fileList.length);
    dispatchSimpleEvent(this, 'scan-started');
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
      const newDirContents = this.createDirectoryContents_(
          this.currentFileListContext_, assert(this.getCurrentDirEntry()),
          this.lastSearchQuery_);
      this.clearAndScan_(newDirContents, callback);
    });
  }

  /**
   * Adds/removes/updates items of file list.
   * @param {Array<Entry>} changedEntries Entries of updated/added files.
   * @param {Array<string>} removedUrls URLs of removed files.
   * @private
   */
  partialUpdate_(changedEntries, removedUrls) {
    // This update should be included in the current running update.
    if (this.pendingScan_) {
      return;
    }

    if (this.runningScan_) {
      // Do update after the current scan is finished.
      const previousScan = this.runningScan_;
      const onPreviousScanCompleted = () => {
        previousScan.removeEventListener(
            'scan-completed', onPreviousScanCompleted);
        // Run the update asynchronously.
        Promise.resolve().then(() => {
          this.partialUpdate_(changedEntries, removedUrls);
        });
      };
      previousScan.addEventListener('scan-completed', onPreviousScanCompleted);
      return;
    }

    const onFinish = () => {
      this.runningScan_ = null;

      this.currentDirContents_.removeEventListener(
          'scan-completed', onCompleted);
      this.currentDirContents_.removeEventListener('scan-failed', onFailure);
      this.currentDirContents_.removeEventListener(
          'scan-cancelled', onCancelled);
    };

    const onCompleted = () => {
      onFinish();
      dispatchSimpleEvent(this, 'rescan-completed');
    };

    const onFailure = () => {
      onFinish();
    };

    const onCancelled = () => {
      onFinish();
    };

    this.runningScan_ = this.currentDirContents_;
    this.currentDirContents_.addEventListener('scan-completed', onCompleted);
    this.currentDirContents_.addEventListener('scan-failed', onFailure);
    this.currentDirContents_.addEventListener('scan-cancelled', onCancelled);
    this.currentDirContents_.update(changedEntries, removedUrls);
  }

  /**
   * Perform a directory contents scan. Should be called only from rescan() and
   * clearAndScan_().
   *
   * @param {DirectoryContents} dirContents DirectoryContents instance on which
   *     the scan will be run.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean} invalidateCache True to invalidate scanning result cache.
   * @param {function():void} successCallback Callback on success.
   * @param {function(DOMError):void} failureCallback Callback on failure.
   * @param {function():void} updatedCallback Callback on update. Only on the
   *     last update, {@code successCallback} is called instead of this.
   * @param {function():void} cancelledCallback Callback on cancel.
   * @private
   */
  scan_(
      dirContents, refresh, invalidateCache, successCallback, failureCallback,
      updatedCallback, cancelledCallback) {
    /**
     * Runs pending scan if there is one.
     *
     * @return {boolean} Did pending scan exist.
     */
    const maybeRunPendingRescan = () => {
      if (this.pendingRescan_) {
        this.rescanSoon(refresh);
        this.pendingRescan_ = false;
        return true;
      }
      return false;
    };

    const onFinished = () => {
      dirContents.removeEventListener('scan-completed', onSuccess);
      dirContents.removeEventListener('scan-updated', updatedCallback);
      dirContents.removeEventListener('scan-failed', onFailure);
      dirContents.removeEventListener('scan-cancelled', cancelledCallback);
    };

    const onSuccess = () => {
      onFinished();

      // Record metric for Downloads directory.
      if (!dirContents.isSearch()) {
        const locationInfo = this.volumeManager_.getLocationInfo(
            assert(dirContents.getDirectoryEntry()));
        const volumeInfo = locationInfo && locationInfo.volumeInfo;
        if (volumeInfo &&
            volumeInfo.volumeType ===
                VolumeManagerCommon.VolumeType.DOWNLOADS &&
            locationInfo.isRootEntry) {
          recordMediumCount('DownloadsCount', dirContents.getFileListLength());
        }
      }

      this.runningScan_ = null;
      successCallback();
      this.scanFailures_ = 0;
      maybeRunPendingRescan();
    };

    // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
    const onFailure = event => {
      onFinished();

      this.runningScan_ = null;
      this.scanFailures_++;
      failureCallback(event.error);

      if (maybeRunPendingRescan()) {
        return;
      }

      // Do not rescan for Guest OS (including Crostini) errors.
      // TODO(crbug/1293229): Guest OS currently reuses the Crostini error
      // string, but once it gets its own strings this needs to include both.
      if (event.error.name === constants.CROSTINI_CONNECT_ERR) {
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

    dirContents.addEventListener('scan-completed', onSuccess);
    dirContents.addEventListener('scan-updated', updatedCallback);
    dirContents.addEventListener('scan-failed', onFailure);
    dirContents.addEventListener('scan-cancelled', onCancelled);
    dirContents.scan(refresh, invalidateCache);
  }

  /**
   * @param {DirectoryContents} dirContents DirectoryContents instance. This
   *     must be a different instance from this.currentDirContents_.
   * @private
   */
  replaceDirectoryContents_(dirContents) {
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

      // @ts-ignore: error TS6133: 'previousDirContents' is declared but its
      // value is never read.
      const previousDirContents = this.currentDirContents_;
      this.currentDirContents_ = dirContents;
      this.currentDirContents_.replaceContextFileList();

      this.setSelectedEntries_(selectedEntries);
      this.fileListSelection_.leadIndex = leadIndex;
      this.setLeadEntry_(leadEntry);

      // If nothing is selected after update, then select file next to the
      // latest selection
      let forceChangeEvent = false;
      if (this.fileListSelection_.selectedIndexes.length == 0 &&
          selectedIndices.length != 0) {
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
   * @param {Entry} entry The entry to be searched.
   * @return {number} The index in the fileList, or -1 if not found.
   * @private
   */
  // @ts-ignore: error TS6133: 'findIndexByEntry_' is declared but its value is
  // never read.
  findIndexByEntry_(entry) {
    const fileList = this.getFileList();
    for (let i = 0; i < fileList.length; i++) {
      if (isSameEntry(/** @type {Entry} */ (fileList.item(i)), entry)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Called when rename is done successfully.
   * Note: conceptually, DirectoryModel should work without this, because
   * entries can be renamed by other systems anytime and the Files app should
   * reflect it correctly.
   * TODO(hidehiko): investigate more background, and remove this if possible.
   *
   * @param {!Entry} oldEntry The old entry.
   * @param {!Entry} newEntry The new entry.
   * @return {!Promise<void>} Resolves on completion.
   */
  onRenameEntry(oldEntry, newEntry) {
    return new Promise(resolve => {
      this.currentDirContents_.prefetchMetadata([newEntry], true, () => {
        // If the current directory is the old entry, then quietly change to the
        // new one.
        if (isSameEntry(oldEntry, this.getCurrentDirEntry())) {
          this.changeDirectoryEntry(
              /** @type {!DirectoryEntry|!FilesAppDirEntry} */ (newEntry));
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
          const item = list.item(i);
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
   * @param {!DirectoryEntry} newDirectory Directory entry to be selected.
   * @return {!Promise<void>} A promise which is resolved when new directory is
   *     selected. If current directory has changed during the operation, this
   *     will be rejected.
   */
  async updateAndSelectNewDirectory(newDirectory) {
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
   * @return {FilesAppDirEntry} myFilesEntry
   */
  getMyFiles() {
    const {myFilesEntry} = getMyFiles(getStore().getState());
    return myFilesEntry;
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
   * @param {!DirectoryEntry|!FilesAppDirEntry} dirEntry The entry of the new
   *     directory to be opened.
   * @param {function(boolean)=} opt_callback Executed if the directory loads
   *     successfully.
   */
  changeDirectoryEntry(dirEntry, opt_callback) {
    // Increment the sequence value.
    const sequence = ++this.changeDirectorySequence_;
    this.stopActiveSearch_();

    // When switching to MyFiles volume, we should use a FilesAppEntry if
    // available because it returns UI-only entries too, like Linux files and
    // Play files.
    const locationInfo = this.volumeManager_.getLocationInfo(dirEntry);
    if (locationInfo &&
        locationInfo.rootType === VolumeManagerCommon.RootType.DOWNLOADS &&
        locationInfo.isRootEntry) {
      dirEntry = this.getMyFiles();
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
          this.currentFileListContext_, dirEntry, '');
      if (!newDirectoryContents) {
        queueTaskCallback();
        return;
      }

      const previousDirEntry = this.currentDirContents_.getDirectoryEntry();
      this.clearAndScan_(newDirectoryContents, result => {
        // Calls the callback of the method and inform it about success or lack
        // of thereof.
        if (opt_callback) {
          opt_callback(result);
        }
        // Notify that the current task of this.directoryChangeQueue_
        // is completed.
        setTimeout(queueTaskCallback, 0);
      });

      // For tests that open the dialog to empty directories, everything
      // is loaded at this point.
      testSendMessage('directory-change-complete');
      const previousVolumeInfo = previousDirEntry ?
          this.volumeManager_.getVolumeInfo(previousDirEntry) :
          null;
      // VolumeInfo for dirEntry.
      const currentVolumeInfo = this.getCurrentVolumeInfo();
      const event = new Event('directory-changed');
      // @ts-ignore: error TS2339: Property 'previousDirEntry' does not exist on
      // type 'Event'.
      event.previousDirEntry = previousDirEntry;
      // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on type
      // 'Event'.
      event.newDirEntry = dirEntry;
      // @ts-ignore: error TS2339: Property 'volumeChanged' does not exist on
      // type 'Event'.
      event.volumeChanged = previousVolumeInfo !== currentVolumeInfo;
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
      this.store_.dispatch(
          // @ts-ignore: error TS2345: Argument of type '{ to:
          // FileSystemDirectoryEntry | FilesAppDirEntry; status: string; }' is
          // not assignable to parameter of type '{ to?:
          // FileSystemDirectoryEntry | FilesAppDirEntry | undefined; toKey:
          // string; status?: string | undefined; }'.
          changeDirectory({to: dirEntry, status: PropStatus.SUCCESS}));
    });
  }

  /**
   * Activates the given directory.
   * This method:
   *  - Changes the current directory, if the given directory is not the current
   *    directory.
   *  - Clears the selection, if the given directory is the current directory.
   *
   * @param {!DirectoryEntry|!FilesAppDirEntry} dirEntry The entry of the new
   *     directory to be opened.
   * @param {function()=} opt_callback Executed if the directory loads
   *     successfully.
   */
  activateDirectoryEntry(dirEntry, opt_callback) {
    const currentDirectoryEntry = this.getCurrentDirEntry();
    if (currentDirectoryEntry && isSameEntry(dirEntry, currentDirectoryEntry)) {
      // On activating the current directory, clear the selection on the
      // filelist.
      this.clearSelection();
    } else {
      // Otherwise, changes the current directory.
      this.changeDirectoryEntry(dirEntry, opt_callback);
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
   * @return {!DirectoryChangeTracker} Created object.
   */
  createDirectoryChangeTracker() {
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

      // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any'
      // type.
      onDirectoryChange_: function(event) {
        tracker.stop();
        tracker.hasChanged = true;
      },
    };
    return tracker;
  }

  /**
   * @param {Entry} entry Entry to be selected.
   */
  selectEntry(entry) {
    const fileList = this.getFileList();
    for (let i = 0; i < fileList.length; i++) {
      if (fileList.item(i).toURL() === entry.toURL()) {
        this.selectIndex(i);
        return;
      }
    }
  }

  /**
   * @param {Array<Entry>} entries Array of entries.
   */
  selectEntries(entries) {
    // URLs are needed here, since we are comparing Entries by URLs.
    const urls = entriesToURLs(entries);
    const fileList = this.getFileList();
    this.fileListSelection_.beginChange();
    this.fileListSelection_.unselectAll();
    for (let i = 0; i < fileList.length; i++) {
      if (urls.indexOf(fileList.item(i).toURL()) >= 0) {
        this.fileListSelection_.setIndexSelected(i, true);
      }
    }
    this.fileListSelection_.endChange();
  }

  /**
   * @param {number} index Index of file.
   */
  selectIndex(index) {
    // this.focusCurrentList_();
    if (index >= this.getFileList().length) {
      return;
    }

    // If a list bound with the model it will do scrollIndexIntoView(index).
    this.fileListSelection_.selectedIndex = index;
  }

  /**
   * Handles update of VolumeInfoList.
   * @param {Event} event Event of VolumeInfoList's 'splice'.
   * @private
   */
  onVolumeInfoListUpdated_(event) {
    // Fallback to the default volume's root if the current volume is unmounted.
    // @ts-ignore: error TS2339: Property 'removed' does not exist on type
    // 'Event'.
    if (this.hasCurrentDirEntryBeenUnmounted_(event.removed)) {
      this.volumeManager_.getDefaultDisplayRoot((displayRoot) => {
        if (displayRoot) {
          this.changeDirectoryEntry(displayRoot);
        }
      });
    }

    // If a volume within My files or removable root is mounted/unmounted rescan
    // its contents.
    const currentDir = this.getCurrentDirEntry();
    // @ts-ignore: error TS2339: Property 'removed' does not exist on type
    // 'Event'.
    const affectedVolumes = event.added.concat(event.removed);
    for (const volume of affectedVolumes) {
      if (isSameEntry(currentDir, volume.prefixEntry)) {
        this.rescan(false);
        break;
      }
    }

    // If the current directory is the Drive placeholder and the real Drive is
    // mounted, switch to it.
    if (this.getCurrentRootType() ===
        VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT) {
      // @ts-ignore: error TS2339: Property 'added' does not exist on type
      // 'Event'.
      for (const newVolume of event.added) {
        if (newVolume.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
          // @ts-ignore: error TS7006: Parameter 'displayRoot' implicitly has an
          // 'any' type.
          newVolume.resolveDisplayRoot().then((displayRoot) => {
            this.changeDirectoryEntry(displayRoot);
          });
        }
      }
    }
    // @ts-ignore: error TS2339: Property 'added' does not exist on type
    // 'Event'.
    if (event.added.length !== 1) {
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
        // @ts-ignore: error TS2339: Property 'isFocused' does not exist on type
        // 'Window & typeof globalThis'.
        (window.isFocused() &&
         // @ts-ignore: error TS2339: Property 'added' does not exist on type
         // 'Event'.
         event.added[0].volumeType ===
             VolumeManagerCommon.VolumeType.PROVIDED &&
         // @ts-ignore: error TS2339: Property 'added' does not exist on type
         // 'Event'.
         event.added[0].source === VolumeManagerCommon.Source.FILE) ||
        // @ts-ignore: error TS2339: Property 'added' does not exist on type
        // 'Event'.
        (event.added[0].volumeType ===
             VolumeManagerCommon.VolumeType.CROSTINI &&
         this.getCurrentRootType() === VolumeManagerCommon.RootType.CROSTINI) ||
        // TODO(crbug/1293229): Don't redirect if the user is looking at a
        // different Guest OS folder.
        // @ts-ignore: error TS2339: Property 'added' does not exist on type
        // 'Event'.
        (isGuestOs(event.added[0].volumeType) &&
         this.getCurrentRootType() === VolumeManagerCommon.RootType.GUEST_OS)) {
      // Resolving a display root on FSP volumes is instant, despite the
      // asynchronous call.
      // @ts-ignore: error TS7006: Parameter 'displayRoot' implicitly has an
      // 'any' type.
      event.added[0].resolveDisplayRoot().then((displayRoot) => {
        // Only change directory if "currentDir" hasn't changed during the
        // display root resolution and if there isn't a directory change in
        // progress, because other part of the system will eventually change the
        // directory.
        if (currentDir === this.getCurrentDirEntry() &&
            this.numChangeTrackerRunning_ === 0) {
          // @ts-ignore: error TS2339: Property 'added' does not exist on type
          // 'Event'.
          this.changeDirectoryEntry(event.added[0].displayRoot);
        }
      });
    }
  }

  /**
   * Returns whether the current directory entry has been unmounted.
   *
   * @param {!Array<!import('../../externs/volume_info.js').VolumeInfo>}
   *     removedVolumes The removed volumes.
   * @private
   */
  hasCurrentDirEntryBeenUnmounted_(removedVolumes) {
    const entry = this.getCurrentDirEntry();
    if (!entry) {
      return false;
    }

    if (!isFakeEntry(entry)) {
      return !this.volumeManager_.getVolumeInfo(entry);
    }

    const rootType = this.getCurrentRootType();
    for (const volume of removedVolumes) {
      // @ts-ignore: error TS2538: Type 'null' cannot be used as an index type.
      if (volume.fakeEntries[rootType]) {
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
   * @param {!DirectoryEntry|!FilesAppEntry} entry Directory entry.
   * @param {string=} query Search query string.
   * @return {boolean} True if directory search should be used for the entry
   *     and query.
   */
  isSearchDirectory(entry, query) {
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (isRecentRootType(entry.rootType) ||
        // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
        // 'FileSystemDirectoryEntry | FilesAppEntry'.
        entry.rootType == VolumeManagerCommon.RootType.CROSTINI ||
        // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
        // 'FileSystemDirectoryEntry | FilesAppEntry'.
        entry.rootType == VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT) {
      return true;
    }
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.MY_FILES) {
      return false;
    }

    if ((query || '').trimStart()) {
      return true;
    }

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (locationInfo &&
        (locationInfo.rootType == VolumeManagerCommon.RootType.MEDIA_VIEW ||
         locationInfo.isSpecialSearchRoot)) {
      return true;
    }
    return false;
  }

  /**
   * Creates scanner factory for the entry and query.
   *
   * @param {!DirectoryEntry|!FilesAppEntry} entry Directory entry.
   * @param {string=} query Search query string.
   * @param {SearchOptions=} options search options.
   * @return {function():ContentScanner} The factory to create ContentScanner
   *     instance.
   */
  createScannerFactory(entry, query, options) {
    const sanitizedQuery = (query || '').trimStart();
    const locationInfo = this.volumeManager_.getLocationInfo(entry);

    if (isRecentScan(entry, options)) {
      const fakeEntry = /** @type {!FakeEntry} */ (entry);
      return () => {
        return new RecentContentScanner(
            sanitizedQuery, this.volumeManager_, fakeEntry.sourceRestriction,
            getFileCategory(fakeEntry, sanitizedQuery, options));
      };
    }
    // TODO(b/271485133): Make sure the entry here is a fake entry, not real
    // volume entry.
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.CROSTINI) {
      return () => {
        return new CrostiniMounter();
      };
    }
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.GUEST_OS) {
      return () => {
        const placeholder = /** @type {!GuestOsPlaceholder} */ (entry);
        return new GuestOsMounter(placeholder.guest_id);
      };
    }
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.MY_FILES) {
      return () => {
        return new DirectoryContentScanner(
            /** @type {!FilesAppDirEntry} */ (entry));
      };
    }
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT) {
      return () => {
        return new ContentScanner();
      };
    }
    // @ts-ignore: error TS2339: Property 'rootType' does not exist on type
    // 'FileSystemDirectoryEntry | FilesAppEntry'.
    if (entry.rootType == VolumeManagerCommon.RootType.TRASH) {
      return () => {
        return new TrashContentScanner(this.volumeManager_);
      };
    }
    if (sanitizedQuery) {
      return () => {
        return new SearchV2ContentScanner(
            this.volumeManager_, entry, sanitizedQuery,
            options || getDefaultSearchOptions());
      };
    }
    if (locationInfo &&
        locationInfo.rootType == VolumeManagerCommon.RootType.MEDIA_VIEW) {
      return () => {
        return new MediaViewContentScanner(
            /** @type {!DirectoryEntry} */ (entry));
      };
    }
    if (locationInfo && locationInfo.isRootEntry &&
        locationInfo.isSpecialSearchRoot) {
      // Drive special search.
      // @ts-ignore: error TS7034: Variable 'searchType' implicitly has type
      // 'any' in some locations where its type cannot be determined.
      let searchType;
      switch (locationInfo.rootType) {
        case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
          searchType = chrome.fileManagerPrivate.SearchType.OFFLINE;
          break;
        case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
          searchType = chrome.fileManagerPrivate.SearchType.SHARED_WITH_ME;
          break;
        case VolumeManagerCommon.RootType.DRIVE_RECENT:
          searchType = chrome.fileManagerPrivate.SearchType.EXCLUDE_DIRECTORIES;
          break;
        default:
          // Unknown special search entry.
          throw new Error('Unknown special search type.');
      }
      return () => {
        // @ts-ignore: error TS7005: Variable 'searchType' implicitly has an
        // 'any' type.
        return new DriveMetadataSearchContentScanner(searchType);
      };
    }
    // Local fetch or search.
    return () => {
      return new DirectoryContentScanner(
          /** @type {!DirectoryEntry} */ (entry));
    };
  }

  /**
   * Creates directory contents for the entry and query.
   *
   * @param {FileListContext} context File list context.
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry Current directory.
   * @param {string=} query Search query string.
   * @param {SearchOptions=} options Search options.
   * @return {!DirectoryContents} Directory contents.
   * @private
   */
  createDirectoryContents_(context, entry, query, options) {
    const isSearch = this.isSearchDirectory(entry, query);
    const scannerFactory = this.createScannerFactory(entry, query, options);
    return new DirectoryContents(context, isSearch, entry, scannerFactory);
  }

  /**
   * Gets the last search query.
   * @return {string} the last search query.
   */
  getLastSearchQuery() {
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
   * @param {string} query Query that will be searched for.
   * @param {!SearchOptions} options Search options, such as file
   *     type, etc.
   * @private
   */
  search_(query, options) {
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

      if (!(query || '').trimStart()) {
        if (this.isSearching()) {
          const newDirContents = this.createDirectoryContents_(
              this.currentFileListContext_, assert(currentDirEntry));
          this.clearAndScan_(newDirContents, callback);
        } else {
          callback();
        }
        return;
      }

      const newDirContents = this.createDirectoryContents_(
          this.currentFileListContext_, assert(currentDirEntry), query,
          options);
      if (!newDirContents) {
        callback();
        return;
      }

      this.store_.dispatch(
          // @ts-ignore: error TS2345: Argument of type '{ query: string;
          // status: string; }' is not assignable to parameter of type
          // 'SearchData'.
          updateSearch({query: query, status: PropStatus.STARTED}));
      // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an
      // 'any[]' type.
      this.onSearchCompleted_ = (...args) => {
        // Notify the store-aware parts.
        // @ts-ignore: error TS2345: Argument of type '{ status: string; }' is
        // not assignable to parameter of type 'SearchData'.
        this.store_.dispatch(updateSearch({status: PropStatus.SUCCESS}));
      };
      this.addEventListener('scan-completed', this.onSearchCompleted_);
      this.clearAndScan_(newDirContents, callback);
    });
  }

  /**
   * In case the search was active, remove listeners and send notifications on
   * its canceling.
   * @private
   */
  stopActiveSearch_() {
    if (!this.isSearching()) {
      return;
    }

    if (this.onSearchCompleted_) {
      this.removeEventListener('scan-completed', this.onSearchCompleted_);
      this.onSearchCompleted_ = null;
    }
  }

  /**
   * Update the file list when certain IO task is finished. To keep the file
   * list refresh for non-watchable fake directory entries and volumes, we need
   * to explicitly subscribe to the IO task status event, and manually refresh.
   * @param {!chrome.fileManagerPrivate.ProgressStatus} event
   * @private
   */
  updateFileListAfterIOTask_(event) {
    let rescan = false;
    /** @type {!Set<?VolumeManagerCommon.RootType>} */
    const fakeDirectoryEntryRootTypes = new Set([
      VolumeManagerCommon.RootType.RECENT,
      VolumeManagerCommon.RootType.TRASH,
    ]);
    const currentRootType = this.getCurrentRootType();
    const currentVolumeInfo = this.getCurrentVolumeInfo();
    if (fakeDirectoryEntryRootTypes.has(currentRootType)) {
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
        event.state === chrome.fileManagerPrivate.IOTaskState.SUCCESS;
    if (isIOTaskFinished) {
      this.rescanLater(/* refresh= */ false, /* invalidateCache= */ true);
    }
  }
}

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
 * @typedef {{
 *   start: function():void,
 *   stop: function():void,
 *   hasChanged: boolean,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'DirectoryChangeTracker' implicitly has an
// 'any' type.
export let DirectoryChangeTracker;
