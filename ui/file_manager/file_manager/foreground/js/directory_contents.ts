// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {mountGuest} from '../../common/js/api.js';
import {AsyncQueue, ConcurrentQueue} from '../../common/js/async_util.js';
import {createDOMError} from '../../common/js/dom_utils.js';
import {isDriveRootType, isFakeEntry, isTrashEntry, readEntriesRecursively} from '../../common/js/entry_utils.js';
import {isType} from '../../common/js/file_type.js';
import type {EntryList, UniversalDirectory, UniversalEntry} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';
import {recordInterval, recordMediumCount, startInterval} from '../../common/js/metrics.js';
import {getEarliestTimestamp} from '../../common/js/recent_date_bucket.js';
import {createTrashReaders, TRASH_CONFIG} from '../../common/js/trash.js';
import {debug, FileErrorToDomError} from '../../common/js/util.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {directoryContentSelector, fetchDirectoryContents} from '../../state/ducks/current_directory.js';
import {getDefaultSearchOptions} from '../../state/ducks/search.js';
import {type DirectoryContent, type FileKey, PropStatus, SearchLocation, type SearchOptions} from '../../state/state.js';
import {getFileData, getStore, type Store} from '../../state/store.js';

import {ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES, CROSTINI_CONNECT_ERR, DLP_METADATA_PREFETCH_PROPERTY_NAMES, FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES, LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES} from './constants.js';
import {FileListModel} from './file_list_model.js';
import type {MetadataItem} from './metadata/metadata_item.js';
import {type MetadataKey} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';

// Common callback types used by content scanners.
type ScanResultCallback = (entries: UniversalEntry[]) => void;
type ScanErrorCallback = (error: DOMError) => void;

/**
 * Scanner of the entries.
 */
export abstract class ContentScanner {
  protected canceled_: boolean = false;

  /**
   * Starts to scan the entries. Any entries discovered during the scanning
   * operation should be delivered on the `entriesCallback`. This callback can
   * be called multiple times. Once the scanning completed successfully, it must
   * call the `successCallback`. If the scanning encountered an error it must
   * call the `errorCallback`. The last parameter tells the scanner if it must
   * invalidate any cached results before scanning.
   */
  abstract scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback,
      invalidateCache?: boolean): Promise<void>;

  /**
   * Request cancelling of the running scan. When the cancelling is done,
   * an error will be reported from errorCallback passed to scan().
   */
  cancel() {
    this.canceled_ = true;
  }

  /**
   * Whether the scanner pushes the entry directly to the store.
   */
  isStoreBased(): boolean {
    return false;
  }
}

/**
 * No-op class to be used for fake entries and such.
 */
export class EmptyContentScanner extends ContentScanner {
  /**
   * A dummy implementation of the scan method. It delivers an empty list of
   * entries on the `entriesCallback` and immediately calls the
   * `successCallback`.
   */
  override scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      _errorCallback: ScanErrorCallback,
      _invalidateCache?: boolean): Promise<void> {
    entriesCallback([]);
    successCallback();
    return Promise.resolve();
  }
}

/**
 * Scanner of the entries in a directory.
 */
export class DirectoryContentScanner extends ContentScanner {
  constructor(private readonly entry_: UniversalDirectory|undefined) {
    super();
  }

  /**
   * Starts to read the entries in the directory.
   */
  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    if (!this.entry_ || !this.entry_.createReader) {
      // If entry is not specified or if entry doesn't implement createReader,
      // we cannot read it.
      errorCallback(
          createDOMError(FileErrorToDomError.INVALID_MODIFICATION_ERR));
      return;
    }

    startInterval('DirectoryScan');
    const reader = this.entry_.createReader();
    const readEntries = () => {
      reader.readEntries(entries => {
        if (this.canceled_) {
          errorCallback(createDOMError(FileErrorToDomError.ABORT_ERR));
          return;
        }

        if (entries.length === 0) {
          // All entries are read.
          recordInterval('DirectoryScan');
          successCallback();
          return;
        }

        entriesCallback(entries);
        readEntries();
      }, errorCallback);
    };
    readEntries();
  }
}

/**
 * Latency metric variant names supported by the search content scanner.
 */
enum LatencyVariant {
  /** Local volume; typically local SSD */
  LOCAL = 'Local',
  /** Removable storage, typically USB */
  REMOVABLE = 'Removable',
  /** Provided volume, such as OneDrive */
  PROVIDED = 'Provided',
  /** Android based volume exposed via DocumentsProvider type service. */
  DOCUMENTS_PROVIDER = 'DocumentsProvider',
}

/**
 * A content scanner capable of scanning both the local file system and Google
 * Drive. When created you need to specify the root type, current entry
 * in the directory tree, the search query and options. The `rootType` together
 * with `options` is then used to determine if the search is conducted on the
 * local folder, root folder, or on the local file system and Google Drive.
 *
 * NOTE: This class is a stop-gap solution when transitioning to a content
 * scanner that talks to a browser level service. The service ultimately should
 * be the one that determines what is being searched, and aggregates the results
 * for the frontend client.
 */
export class SearchV2ContentScanner extends ContentScanner {
  private readonly query_: string;
  private readonly options_: SearchOptions;
  private readonly rootType_: RootType|null;
  private readonly driveSearchTypeMap_:
      Map<RootType, chrome.fileManagerPrivate.SearchType>;

  constructor(
      private readonly volumeManager_: VolumeManager,
      private readonly entry_: UniversalDirectory, query: string,
      options?: SearchOptions) {
    super();
    const locationInfo = this.volumeManager_.getLocationInfo(this.entry_);
    this.rootType_ = locationInfo ? locationInfo.rootType : null;
    this.query_ = query.toLowerCase();
    this.options_ = options || getDefaultSearchOptions();
    this.driveSearchTypeMap_ = new Map([
      [
        RootType.DRIVE_OFFLINE,
        chrome.fileManagerPrivate.SearchType.OFFLINE,
      ],
      [
        RootType.DRIVE_SHARED_WITH_ME,
        chrome.fileManagerPrivate.SearchType.EXCLUDE_DIRECTORIES,
      ],
      [
        RootType.DRIVE_RECENT,
        chrome.fileManagerPrivate.SearchType.EXCLUDE_DIRECTORIES,
      ],
    ]);
  }

  /**
   * For the given `dirEntry` it returns a list of searchable roots. This
   * method exists as we have special volumes that aggregate other volumes.
   * Examples include Crostini, Playfiles, aggregated in My files or
   * USB partitions aggregated by USB root. For those cases we return multiple
   * search roots. For plain directories we just return the directory itself.
   */
  private getSearchRoots_(dirEntry: UniversalDirectory): DirectoryEntry[] {
    const typeName: string|null =
        'typeName' in dirEntry ? dirEntry.typeName : null;
    if (typeName !== 'EntryList' && typeName !== 'VolumeEntry') {
      return [dirEntry as DirectoryEntry];
    }
    const children = (dirEntry as EntryList).getUiChildren();
    const allRoots = [dirEntry, ...children];
    return allRoots.filter(entry => !isFakeEntry(entry))
        .map(entry => entry.filesystem!.root);
  }

  /**
   * For the given entry attempts to return the top most volume that contains
   * this entry. The reason for this method is that for some entries, getting
   * the root volume is not sufficient. For example, for a Linux folder the root
   * volume would be the Linux volume. However, in the UI Linux is nested inside
   * My files, so we need to get My files as the top-most volume of a Linux
   * directory.
   */
  private getTopMostVolume_(entry: UniversalDirectory): UniversalDirectory {
    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    if (!volumeInfo) {
      // It's a placeholder or a fake entry.
      return entry;
    }
    const topEntry = volumeInfo.prefixEntry ?
        // TODO(b/289003444): Fix this cast.
        volumeInfo.prefixEntry as UniversalDirectory :
        volumeInfo.displayRoot;
    // Here entry should never be null, but due to Closure annotations, Closure
    // thinks it may be (both prefixEntry and displayRoot above are not
    // guaranteed to be non-null).
    return topEntry ? this.getWrappedVolumeEntry_(topEntry) : entry;
  }

  private getWrappedVolumeEntry_(entry: UniversalDirectory):
      UniversalDirectory {
    const state = getStore().getState();
    // Fetch the wrapped VolumeEntry from the store.
    const fileData = state.allEntries[entry.toURL()];
    if (!fileData || !fileData.entry) {
      console.warn(`Missing FileData for ${entry.toURL()}`);
      // TODO(b/289003444): Fix this cast.
      return entry as UniversalDirectory;
    }
    return fileData.entry as UniversalDirectory;
  }

  /**
   * For the given colume type returns root directories for all volumes with the
   * given `volumeType`.
   */
  private getRootFoldersByVolumeType_(volumeType: string):
      UniversalDirectory[] {
    const rootDirs: UniversalDirectory[] = [];
    const volumeInfoList = this.volumeManager_.volumeInfoList;
    for (let index = 0; index < volumeInfoList.length; ++index) {
      const volumeInfo = volumeInfoList.item(index);
      if (volumeInfo.volumeType === volumeType) {
        const displayRoot = volumeInfo.displayRoot;
        if (displayRoot) {
          rootDirs.push(...this.getSearchRoots_(displayRoot));
        }
      }
    }
    return rootDirs;
  }

  /**
   * Creates a single promise that, when fulfilled, returns a non-null array of
   * file entries. The array may be empty. The metricVariant must be a valid
   * name of the UMA search metric variant.
   */
  private makeFileSearchPromise_(
      params: chrome.fileManagerPrivate.SearchMetadataParams,
      metricVariant: LatencyVariant): Promise<UniversalEntry[]> {
    return new Promise<UniversalEntry[]>((resolve, reject) => {
      startInterval(`Search.${metricVariant}.Latency`);
      chrome.fileManagerPrivate.searchFiles(
          params, (entries: UniversalEntry[]) => {
            if (this.canceled_) {
              reject(createDOMError(FileErrorToDomError.ABORT_ERR));
            } else if (chrome.runtime.lastError) {
              reject(createDOMError(
                  FileErrorToDomError.NOT_READABLE_ERR,
                  chrome.runtime.lastError.message));
            } else {
              recordInterval(`Search.${metricVariant}.Latency`);
              resolve(entries);
            }
          });
    });
  }

  /**
   * Creates a promise that, when fulfilled, returns a non-null array of
   * file entries. This promise uses a client side recursive entry reader.
   */
  private makeReadEntriesRecursivelyPromise_(
      folder: UniversalDirectory, modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory, maxResults: number,
      metricVariant: LatencyVariant): Promise<UniversalEntry[]> {
    // A promise that resolves to an entry if it is modified after cutoffDate or
    // null, otherwise. Used to filter entries by modified time. If we fail to
    // get metadata for an entry we return it without comparison, to be on the
    // safe side.
    const newDateFilterPromise = (entry: UniversalEntry, cutoffDate: Date) =>
        new Promise<UniversalEntry|null>(resolve => {
          entry.getMetadata(
              // TODO(b:289003444): Check if metadata is available in the store.
              (metadata) => {
                resolve(metadata.modificationTime > cutoffDate ? entry : null);
              },
              () => {
                resolve(entry);
              });
        });
    return new Promise<UniversalEntry[]>((resolve, reject) => {
      startInterval(`Search.${metricVariant}.Latency`);
      const collectedEntries: UniversalEntry[] = [];
      let workLeft = 1;
      readEntriesRecursively(
          folder,
          // More entries found callback.
          (entries: UniversalEntry[]) => {
            const filtered = entries.filter((entry: UniversalEntry) => {
              if (entry.name.toLowerCase().indexOf(this.query_) < 0) {
                return false;
              }
              if (category !== chrome.fileManagerPrivate.FileCategory.ALL) {
                if (!isType([category], entry)) {
                  return false;
                }
              }
              return true;
            });
            if (modifiedTimestamp === 0) {
              collectedEntries.push(...filtered);
            } else {
              workLeft += filtered.length;
              const cutoff = new Date(modifiedTimestamp);
              Promise
                  .all(filtered.map(
                      (entry: UniversalEntry) =>
                          newDateFilterPromise(entry, cutoff)))
                  .then((modified: Array<UniversalEntry|null>) => {
                    const nullEntryFilter =
                        (e: UniversalEntry|null): e is UniversalEntry => {
                          return e !== null;
                        };
                    collectedEntries.push(...modified.filter(nullEntryFilter));
                    workLeft -= modified.length;
                    if (workLeft <= 0) {
                      recordInterval(`Search.${metricVariant}.Latency`);
                      resolve(collectedEntries);
                    }
                  });
            }
          },
          // All entries read callback.
          () => {
            if (--workLeft <= 0) {
              recordInterval(`Search.${metricVariant}.Latency`);
              resolve(collectedEntries);
            }
          },
          // Error callback.
          () => {
            if (!this.canceled_ && collectedEntries.length >= maxResults) {
              recordInterval(`Search.${metricVariant}.Latency`);
              resolve(collectedEntries);
            } else {
              reject();
            }
          },
          // Should stop callback.
          () => {
            return collectedEntries.length >= maxResults || this.canceled_;
          });
    });
  }

  /**
   * For the given set of `folders` holding directory entries, creates an array
   * of promises that, when fulfilled, return an array of entries in those
   * directories.
   */
  private makeFileSearchPromiseList_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory, maxResults: number,
      metricVariant: LatencyVariant,
      folders: UniversalDirectory[]): Array<Promise<UniversalEntry[]>> {
    const baseParams: chrome.fileManagerPrivate.SearchMetadataParams = {
      rootDir: undefined,  // Provided in the loop below.
      query: this.query_,
      types: chrome.fileManagerPrivate.SearchType.ALL,
      maxResults: maxResults,
      modifiedTimestamp: modifiedTimestamp,
      category: category,
    };
    return folders.map(
        (searchDir: UniversalDirectory): Promise<UniversalEntry[]> =>
            this.makeFileSearchPromise_(
                {
                  ...baseParams,
                  rootDir: searchDir as DirectoryEntry,
                },
                metricVariant));
  }

  /**
   * Returns an array of promises that, when fulfilled, return an array of
   * entries matching the current query, modified timestamp, and category for
   * folders located under My files.
   */
  private createMyFilesSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    const myFilesVolume =
        this.volumeManager_.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS);
    if (!myFilesVolume || !myFilesVolume.displayRoot) {
      return [];
    }
    const myFilesEntry = this.getWrappedVolumeEntry_(myFilesVolume.displayRoot);
    return this.makeFileSearchPromiseList_(
        modifiedTimestamp, category, maxResults, LatencyVariant.LOCAL,
        this.getSearchRoots_(myFilesEntry));
  }

  /**
   * Returns an array of promises that, when fulfilled, return an array of
   * entries matching the current query, modified timestamp, and category for
   * all known removable drives.
   */
  private createRemovablesSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    return this.makeFileSearchPromiseList_(
        modifiedTimestamp, category, maxResults, LatencyVariant.REMOVABLE,
        this.getRootFoldersByVolumeType_(VolumeType.REMOVABLE));
  }

  /**
   * Returns an array of promises that, when fulfilled, return an array of
   * entries matching the current query, modified timestamp, and category for
   * all known document providers.
   */
  private createDocumentsProviderSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    const rootFolderList =
        this.getRootFoldersByVolumeType_(VolumeType.DOCUMENTS_PROVIDER);
    return rootFolderList.map(
        rootFolder => this.makeReadEntriesRecursivelyPromise_(
            rootFolder, modifiedTimestamp, category, maxResults,
            LatencyVariant.DOCUMENTS_PROVIDER));
  }

  /**
   * Returns an array of promises that, when fulfilled, return an array of
   * entries matching the current query, modified timestamp, and category for
   * all known file system provider volumes.
   */
  private createFileSystemProviderSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    const rootFolderList =
        this.getRootFoldersByVolumeType_(VolumeType.PROVIDED);
    return rootFolderList.map(
        rootFolder => this.makeReadEntriesRecursivelyPromise_(
            rootFolder, modifiedTimestamp, category, maxResults,
            LatencyVariant.PROVIDED));
  }

  /**
   * Returns a promise that, when fulfilled, returns an array of file entries
   * matching the current query, modified timestamp and category for files
   * located on Drive.
   */
  private createDriveSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Promise<UniversalEntry[]> {
    let searchType = this.rootType_ !== null ?
        this.driveSearchTypeMap_.get(this.rootType_) :
        null;
    if (!searchType) {
      searchType = chrome.fileManagerPrivate.SearchType.ALL;
    }
    return new Promise<UniversalEntry[]>((resolve, reject) => {
      startInterval('Search.Drive.Latency');
      chrome.fileManagerPrivate.searchDriveMetadata(
          {
            query: this.query_,
            category: category,
            types: searchType!,
            maxResults: maxResults,
            modifiedTimestamp: modifiedTimestamp,
            rootDir: undefined,
          },
          (results) => {
            if (chrome.runtime.lastError) {
              reject(createDOMError(
                  FileErrorToDomError.NOT_READABLE_ERR,
                  chrome.runtime.lastError.message));
            } else if (this.canceled_) {
              reject(createDOMError(FileErrorToDomError.ABORT_ERR));
            } else if (!results) {
              reject(
                  createDOMError(FileErrorToDomError.INVALID_MODIFICATION_ERR));
            } else {
              recordInterval('Search.Drive.Latency');
              resolve(results.map(r => r.entry));
            }
          });
    });
  }

  private createDirectorySearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    if (isDriveRootType(this.rootType_)) {
      return [
        this.createDriveSearch_(modifiedTimestamp, category, maxResults),
      ];
    }
    const searchFolder = this.options_.location === SearchLocation.THIS_FOLDER ?
        this.entry_ :
        this.getTopMostVolume_(this.entry_);
    if (this.rootType_ === RootType.DOCUMENTS_PROVIDER) {
      return [this.makeReadEntriesRecursivelyPromise_(
          searchFolder, modifiedTimestamp, category, maxResults,
          LatencyVariant.DOCUMENTS_PROVIDER)];
    }
    if (this.rootType_ === RootType.PROVIDED) {
      return [this.makeReadEntriesRecursivelyPromise_(
          searchFolder, modifiedTimestamp, category, maxResults,
          LatencyVariant.PROVIDED)];
    }
    const metricVariant = this.rootType_ === RootType.REMOVABLE ?
        LatencyVariant.REMOVABLE :
        LatencyVariant.LOCAL;
    // My Files or a folder nested in it.
    return this.makeFileSearchPromiseList_(
        modifiedTimestamp, category, maxResults, metricVariant,
        this.getSearchRoots_(searchFolder));
  }

  private createEverywhereSearch_(
      modifiedTimestamp: number,
      category: chrome.fileManagerPrivate.FileCategory,
      maxResults: number): Array<Promise<UniversalEntry[]>> {
    return [
      ...this.createMyFilesSearch_(modifiedTimestamp, category, maxResults),
      ...this.createRemovablesSearch_(modifiedTimestamp, category, maxResults),
      this.createDriveSearch_(modifiedTimestamp, category, maxResults),
      ...this.createDocumentsProviderSearch_(
          modifiedTimestamp, category, maxResults),
      ...this.createFileSystemProviderSearch_(
          modifiedTimestamp, category, maxResults),
    ];
  }

  /**
   * Starts the file name search.
   */
  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    const category = this.options_.fileCategory;
    const modifiedTimestamp =
        getEarliestTimestamp(this.options_.recency, new Date());
    const maxResults = 100;

    const searchPromises =
        this.options_.location === SearchLocation.EVERYWHERE ?
        this.createEverywhereSearch_(modifiedTimestamp, category, maxResults) :
        this.createDirectorySearch_(modifiedTimestamp, category, maxResults);

    if (!searchPromises) {
      console.warn(
          `No search promises for options ${JSON.stringify(this.options_)}`);
      successCallback();
    }
    // The job of entriesCallbackCaller is to call entriesCallback as soon as
    // entries are available. We call successCallback only once all of them are
    // settled, but we do not wish to wait for all of promises to be settled
    // before showing the entries.
    const entriesCallbackCaller = (entries: UniversalEntry[]): number => {
      if (entries && entries.length > 0) {
        entriesCallback(entries);
      }
      return entries ? entries.length : 0;
    };
    Promise.allSettled(searchPromises.map(p => p.then(entriesCallbackCaller)))
        .then((results) => {
          let resultCount = 0;
          for (const result of results) {
            if (result.status === 'rejected') {
              errorCallback(result.reason);
            } else if (result.status === 'fulfilled') {
              resultCount += result.value;
            }
          }
          successCallback();
          recordMediumCount('Search.ResultCount', resultCount);
        });
  }
}

/**
 * Scanner of the entries for the metadata search on Drive File System.
 */
export class DriveMetadataSearchContentScanner extends ContentScanner {
  constructor(private readonly searchType_:
                  chrome.fileManagerPrivate.SearchType) {
    super();
  }

  /**
   * Starts to metadata-search on Drive File System.
   */
  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    chrome.fileManagerPrivate.searchDriveMetadata(
        {
          query: '',
          types: this.searchType_,
          maxResults: 100,
          rootDir: undefined,
          modifiedTimestamp: undefined,
          category: undefined,
        },
        (results: chrome.fileManagerPrivate.DriveMetadataSearchResult[]) => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
          }
          if (this.canceled_) {
            errorCallback(createDOMError(FileErrorToDomError.ABORT_ERR));
            return;
          }

          if (!results) {
            console.warn('Drive search encountered an error.');
            errorCallback(
                createDOMError(FileErrorToDomError.INVALID_MODIFICATION_ERR));
            return;
          }

          const entries = results.map(result => {
            return result.entry;
          });
          if (entries.length > 0) {
            entriesCallback(entries);
          }
          successCallback();
        });
  }
}

export class RecentContentScanner extends ContentScanner {
  private readonly query_: string;
  private readonly sourceRestriction_:
      chrome.fileManagerPrivate.SourceRestriction;
  private readonly fileCategory_: chrome.fileManagerPrivate.FileCategory;

  constructor(
      query: string, private readonly cutoffDays_: number,
      private readonly volumeManager_: VolumeManager,
      sourceRestriction?: chrome.fileManagerPrivate.SourceRestriction,
      fileCategory?: chrome.fileManagerPrivate.FileCategory) {
    super();

    this.query_ = query.toLowerCase();
    this.sourceRestriction_ = sourceRestriction ||
        chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE;
    this.fileCategory_ =
        fileCategory || chrome.fileManagerPrivate.FileCategory.ALL;
  }

  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, invalidateCache: boolean = false) {
    /**
     * Files app launched with "volumeFilter" launch parameter will filter
     * out some volumes. Before returning the recent entries, we need to
     * check if the entry's volume location is valid or not
     * (crbug.com/1333385/#c17).
     */
    const isAllowedVolume = (entry: Entry) =>
        this.volumeManager_.getVolumeInfo(entry) !== null;
    chrome.fileManagerPrivate.getRecentFiles(
        this.sourceRestriction_, this.query_, this.cutoffDays_,
        this.fileCategory_, invalidateCache, entries => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            errorCallback(
                createDOMError(FileErrorToDomError.INVALID_MODIFICATION_ERR));
            return;
          }
          if (entries.length > 0) {
            entriesCallback(entries.filter(entry => isAllowedVolume(entry)));
          }
          successCallback();
        });
  }
}

/**
 * Scanner of media-view volumes.
 */
export class MediaViewContentScanner extends ContentScanner {
  /**
   * Creates a scanner at the given root entry of the media-view volume.
   */
  constructor(private rootEntry_: UniversalDirectory) {
    super();
  }

  /**
   * This scanner provides flattened view of media providers.
   *
   * In FileSystem API level, each media-view root directory has directory
   * hierarchy. We need to list files under the root directory to provide
   * flatten view. A file will not be shown in multiple directories in
   * media-view hierarchy since no folders will be added in media documents
   * provider. We can list all files without duplication by just retrieving
   * files in directories recursively.
   */
  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    // To provide flatten view of files, this media-view scanner retrieves
    // files in directories inside the media's root entry recursively.
    readEntriesRecursively(
        this.rootEntry_,
        entries => entriesCallback(entries.filter(entry => !entry.isDirectory)),
        successCallback, errorCallback, () => false);
  }
}

/**
 * Shows an empty list and spinner whilst starting and mounting the
 * crostini container.
 *
 * This function is only called once to start and mount the crostini
 * container.  When FilesApp starts, the related fake root entry for
 * crostini is shown which uses this CrostiniMounter as its ContentScanner.
 *
 * When the sshfs mount completes, it will show up as a disk volume.
 * `refreshNavigationRootsReducer` will detect that crostini
 * is mounted as a disk volume and hide the fake root item while the
 * disk volume exists.
 */
export class CrostiniMounter extends ContentScanner {
  override async scan(
      _entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    chrome.fileManagerPrivate.mountCrostini(() => {
      if (chrome.runtime.lastError) {
        console.warn(`Cannot mount Crostini volume: ${
            chrome.runtime.lastError.message}`);
        errorCallback(createDOMError(
            CROSTINI_CONNECT_ERR, chrome.runtime.lastError.message));
        return;
      }
      successCallback();
    });
  }
}

/**
 * Shows an empty list and spinner whilst starting and mounting a Guest OS's
 * shared files.
 *
 * When FilesApp starts, the related placeholder root entry is shown which uses
 * this GuestOsMounter as its ContentScanner. When the mount succeeds it will
 * show up as a disk volume. `refreshNavigationRootsReducer` will
 * detect thew new volume and hide the placeholder root item while the disk
 * volume exists.
 */
export class GuestOsMounter extends ContentScanner {
  /**
   * Creates a new GuestOSMounter. The `guest_id` is the id for the
   * GuestOsMountProvider to use
   */
  constructor(private readonly guest_id_: number) {
    super();
  }

  override async scan(
      _entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    try {
      await mountGuest(this.guest_id_);
      successCallback();
    } catch (error) {
      errorCallback(createDOMError(
          // TODO(crbug/1293229): Strings
          CROSTINI_CONNECT_ERR, JSON.stringify(error)));
    }
  }
}

/**
 * Read all the Trash directories for content.
 */
export class TrashContentScanner extends ContentScanner {
  private readonly readers_: FileSystemDirectoryReader[];

  /**
   * volumeManager Identifies the underlying filesystem.
   */
  constructor(volumeManager: VolumeManager) {
    super();
    this.readers_ = createTrashReaders(volumeManager);
  }

  /**
   * Scan all the trash directories for content.
   */
  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    const readEntries = (idx: number) => {
      if (idx >= this.readers_.length) {
        // All Trash directories have been read.
        successCallback();
        return;
      }
      this.readers_[idx]!.readEntries(entries => {
        if (this.canceled_) {
          errorCallback(createDOMError(FileErrorToDomError.ABORT_ERR));
          return;
        }

        entriesCallback(entries);
        readEntries(idx + 1);
      }, errorCallback);
    };
    readEntries(0);
    return;
  }
}

type EntryFilter = (e: UniversalEntry) => boolean;

/**
 * Top-level Android folders which are visible by default.
 */
const DEFAULT_ANDROID_FOLDERS = ['Documents', 'Movies', 'Music', 'Pictures'];

/**
 * Windows files or folders to hide by default.
 */
const WINDOWS_HIDDEN = ['$RECYCLE.BIN'];


/**
 * This class manages filters and determines a file should be shown or not.
 * When filters are changed, a 'changed' event is fired.
 */
export class FileFilter extends EventTarget {
  private filters_ = new Map<string, EntryFilter>();

  constructor(private readonly volumeManager_: VolumeManager) {
    super();
    /**
     * Setup initial filters.
     */
    this.setupInitialFilters_();
  }

  private setupInitialFilters_() {
    this.setHiddenFilesVisible(false);
    this.setAllAndroidFoldersVisible(false);
    this.hideAndroidDownload();
  }

  /**
   * Registers the given filter with the given name.
   */
  addFilter(name: string, filterFn: EntryFilter) {
    this.filters_.set(name, filterFn);
    dispatchSimpleEvent(this, 'changed');
  }

  /**
   * @param name Filter identifier.
   */
  removeFilter(name: string) {
    this.filters_.delete(name);
    dispatchSimpleEvent(this, 'changed');
  }

  /**
   * Show/Hide hidden files (i.e. files starting with '.', or other system files
   * for Windows files). Passing `true` as the `visible` parameters means the
   * hidden files should be visible to the user.
   */
  setHiddenFilesVisible(visible: boolean) {
    if (!visible) {
      this.addFilter('hidden', (entry: UniversalEntry): boolean => {
        if (entry.name.startsWith('.')) {
          return false;
        }
        // Hide folders under .Trash, but we don't want to hide anything showing
        // in "Trash", hence the `!isTrashEntry` check because all entries
        // showing under "Trash" will be TrashEntry.
        const insideTrash = TRASH_CONFIG.map(t => t.trashDir)
                                .some(dir => entry.fullPath.startsWith(dir));
        if (insideTrash && !isTrashEntry(entry)) {
          return false;
        }
        // Only hide WINDOWS_HIDDEN in downloads:/PvmDefault.
        if (entry.fullPath.startsWith('/PvmDefault/') &&
            WINDOWS_HIDDEN.includes(entry.name)) {
          const info = this.volumeManager_.getLocationInfo(entry);
          if (info && info.rootType === RootType.DOWNLOADS) {
            return false;
          }
        }
        return true;
      });
    } else {
      this.removeFilter('hidden');
    }
  }

  /**
   * Returns whether or not hidden files are visible to the user now.
   */
  isHiddenFilesVisible(): boolean {
    return !this.filters_.has('hidden');
  }

  /**
   * Show/Hide uncommon Android folders.
   * @param visible True if uncommon folders should be visible to the
   *     user.
   */
  setAllAndroidFoldersVisible(visible: boolean) {
    if (!visible) {
      this.addFilter('android_hidden', (entry: UniversalEntry): boolean => {
        if (entry.filesystem && entry.filesystem.name !== 'android_files') {
          return true;
        }
        // Hide top-level folder or sub-folders that should be hidden.
        if (entry.fullPath) {
          const components = entry.fullPath.split('/');
          if (components[1] &&
              DEFAULT_ANDROID_FOLDERS.indexOf(components[1]) === -1) {
            return false;
          }
        }
        return true;
      });
    } else {
      this.removeFilter('android_hidden');
    }
  }

  /**
   * @return True if uncommon folders is visible to the user now.
   */
  isAllAndroidFoldersVisible(): boolean {
    return !this.filters_.has('android_hidden');
  }

  /**
   * Sets up a filter to hide /Download directory in 'Play files' volume.
   *
   * "Play files/Download" is an alias to Chrome OS's Downloads volume. It is
   * convenient in Android file picker, but can be confusing in Chrome OS Files
   * app. This function adds a filter to hide the Android's /Download.
   */
  hideAndroidDownload() {
    this.addFilter('android_download', (entry: UniversalEntry): boolean => {
      if (entry.filesystem && entry.filesystem.name === 'android_files' &&
          entry.fullPath === '/Download') {
        return false;
      }
      return true;
    });
  }

  /**
   * @param entry File entry.
   * @return True if the file should be shown, false otherwise.
   */
  filter(entry: UniversalEntry): boolean {
    for (const p of this.filters_.values()) {
      if (!p(entry)) {
        return false;
      }
    }
    return true;
  }
}

/**
 * A context of DirectoryContents.
 * TODO(yoshiki): remove this. crbug.com/224869.
 */
export class FileListContext {
  readonly fileList: FileListModel;
  readonly prefetchPropertyNames: MetadataKey[];

  constructor(
      readonly fileFilter: FileFilter, readonly metadataModel: MetadataModel,
      readonly volumeManager: VolumeManager) {
    this.fileList = new FileListModel(metadataModel);
    this.prefetchPropertyNames = Array.from(new Set([
      ...LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
      ...ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES,
      ...FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES,
      ...DLP_METADATA_PREFETCH_PROPERTY_NAMES,
    ]));
  }
}

export type DirContentsScanUpdatedEvent = CustomEvent<{
  /** Whether the content scanner was based in the store. */
  isStoreBased: boolean,
}>;
export type DirContentsScanFailedEvent = CustomEvent<{error: DOMError}>;
export type DirContentsScanCanceled = CustomEvent<undefined>;
export type DirContentsScanCompleted = CustomEvent<undefined>;

interface DirectoryContentsEventMap extends CustomEventMap {
  'dir-contents-scan-updated': DirContentsScanUpdatedEvent;
  'dir-contents-scan-failed': DirContentsScanFailedEvent;
  'dir-contents-scan-canceled': DirContentsScanCanceled;
  'dir-contents-scan-completed': DirContentsScanCompleted;
}

/**
 * This class is responsible for scanning directory (or search results), and
 * filling the fileList. Different descendants handle various types of directory
 * contents shown: basic directory, drive search results, local search results.
 */
export class DirectoryContents extends
    FilesEventTarget<DirectoryContentsEventMap> {
  private fileList_: FileListModel;
  private scanner_: ContentScanner|null = null;
  private processNewEntriesQueue_: AsyncQueue = new AsyncQueue();
  private scanCanceled_: boolean = false;
  /**
   * Metadata snapshot which is used to know which file is actually changed.
   */
  private metadataSnapshot_: Map<string, MetadataItem>|null = null;

  /**
   * @param context The file list context.
   * @param isSearch True for search directory contents, otherwise false.
   * @param directoryEntry The entry of the current directory.
   * @param scannerFactory The factory to create ContentScanner instance.
   */
  constructor(
      private readonly context_: FileListContext,
      private readonly isSearch_: boolean,
      private readonly directoryEntry_: UniversalDirectory|undefined,
      private readonly fileKey_: FileKey|undefined,
      private readonly scannerFactory_: () => ContentScanner) {
    super();

    this.fileList_ = this.context_.fileList;
    this.fileList_.initNewDirContents(this.context_.volumeManager);
  }

  /**
   * Create the copy of the object, but without scan started.
   * @return Object copy.
   */
  clone(): DirectoryContents {
    return new DirectoryContents(
        this.context_, this.isSearch_, this.directoryEntry_, this.fileKey_,
        this.scannerFactory_);
  }

  /**
   * Returns the file list length.
   */
  getFileListLength(): number {
    return this.fileList_.length;
  }

  /**
   * Use a given fileList instead of the fileList from the context.
   * @param fileList The new file list.
   */
  setFileList(fileList: FileListModel) {
    this.fileList_ = fileList;
  }

  /**
   * Creates snapshot of metadata in the directory. Returns Metadata snapshot
   * of current directory contents.
   */
  createMetadataSnapshot(): Map<string, MetadataItem> {
    const snapshot = new Map<string, MetadataItem>();
    const entries: UniversalEntry[] = this.fileList_.slice();
    const metadata =
        this.context_.metadataModel.getCache(entries, ['modificationTime']);
    for (const [i, entry] of entries.entries()) {
      snapshot.set(entry.toURL(), metadata[i]!);
    }
    return snapshot;
  }

  /**
   * Sets metadata snapshot which is used to check changed files.
   * @param metadataSnapshot A metadata snapshot.
   */
  setMetadataSnapshot(metadataSnapshot: Map<string, MetadataItem>) {
    this.metadataSnapshot_ = metadataSnapshot;
  }

  /**
   * Use the filelist from the context and replace its contents with the entries
   * from the current fileList. If metadata snapshot is set, this method checks
   * actually updated files and dispatch change events by calling updateIndexes.
   */
  replaceContextFileList() {
    if (this.context_.fileList === this.fileList_) {
      return;
    }
    // TODO(yawano): While we should update the list with adding or deleting
    // what actually added and deleted instead of deleting and adding all
    // items, splice of array data model is expensive since it always runs
    // sort and we replace the list in this way to reduce the number of splice
    // calls.
    const spliceArgs = this.fileList_.slice();
    const fileList = this.context_.fileList;
    fileList.splice(0, fileList.length, ...spliceArgs);
    this.fileList_ = fileList;

    // Check updated files and dispatch change events.
    if (!this.metadataSnapshot_) {
      return;
    }
    const updatedIndexes = [];
    const entries: UniversalEntry[] = this.fileList_.slice();
    const freshMetadata =
        this.context_.metadataModel.getCache(entries, ['modificationTime']);

    for (let i = 0; i < entries.length; i++) {
      const url = entries[i]!.toURL();
      const entryMetadata = freshMetadata[i];
      // If the Files app fails to obtain both old and new modificationTime,
      // regard the entry as not updated.
      const storedMetadata = this.metadataSnapshot_.get(url);
      if (entryMetadata?.modificationTime?.getTime() !==
          storedMetadata?.modificationTime?.getTime()) {
        updatedIndexes.push(i);
      }
    }

    if (updatedIndexes.length > 0) {
      this.fileList_.updateIndexes(updatedIndexes);
    }
  }

  /**
   * @return If the scan is active.
   */
  isScanning(): boolean {
    return this.scanner_ !== null || this.processNewEntriesQueue_.isRunning();
  }

  /**
   * @return True if search results (drive or local).
   */
  isSearch(): boolean {
    return this.isSearch_;
  }

  /**
   * @return A DirectoryEntry for
   *     current directory. In case of search -- the top directory from which
   *     search is run.
   */
  getDirectoryEntry(): UniversalDirectory|undefined {
    return this.directoryEntry_;
  }

  getFileKey(): FileKey|undefined {
    return this.fileKey_;
  }

  /**
   * Start directory scan/search operation. Either 'dir-contents-scan-completed'
   * or 'dir-contents-scan-failed' event will be fired upon completion.
   *
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param invalidateCache True to invalidate the backend scanning result
   *     cache. This param only works if the corresponding backend scanning
   *     supports cache.
   */
  scan(refresh: boolean, invalidateCache: boolean) {
    /**
     * Invoked when the scanning is completed successfully.
     */
    const completionCallback = () => {
      this.onScanFinished_();
      this.onScanCompleted_();
    };

    /**
     * Invoked when the scanning is finished but is not completed due to error.
     */
    const errorCallback = (error: DOMError) => {
      this.onScanFinished_();
      this.onScanError_(error);
    };

    // TODO(hidehiko,mtomasz): this scan method must be
    // called at most once. Remove such a limitation.
    this.scanner_ = this.scannerFactory_();
    this.scanner_.scan(
        this.onNewEntries_.bind(this, refresh, this.scanner_.isStoreBased()),
        completionCallback, errorCallback, invalidateCache);
  }

  /**
   * Adds/removes/updates items of file list.
   * @param updatedEntries Entries of updated/added files.
   * @param removedUrls URLs of removed files.
   */
  update(updatedEntries: Entry[], removedUrls: string[]) {
    const removedSet = new Set<string>();
    for (const url of removedUrls) {
      removedSet.add(url);
    }

    const updatedMap = new Map<string, Entry>();
    for (const entry of updatedEntries) {
      updatedMap.set(entry.toURL(), entry);
    }

    const updatedList: Entry[] = [];
    const updatedIndexes = [];
    for (let i = 0; i < this.fileList_.length; i++) {
      const url = this.fileList_.item(i)!.toURL();

      if (removedSet.has(url)) {
        // Find the maximum range in which all items need to be removed.
        const begin = i;
        let end = i + 1;
        while (end < this.fileList_.length &&
               removedSet.has(this.fileList_.item(end)?.toURL() || '')) {
          end++;
        }
        // Remove the range [begin, end) at once to avoid multiple sorting.
        this.fileList_.splice(begin, end - begin);
        i--;
        continue;
      }

      const updatedEntry = updatedMap.get(url);
      if (updatedEntry) {
        updatedList.push(updatedEntry);
        updatedIndexes.push(i);
        updatedMap.delete(url);
      }
    }

    if (updatedIndexes.length > 0) {
      this.fileList_.updateIndexes(updatedIndexes);
    }

    const addedList: Entry[] = [];
    for (const updatedEntry of updatedMap.values()) {
      addedList.push(updatedEntry);
    }

    if (removedUrls.length > 0) {
      this.context_.metadataModel.notifyEntriesRemoved(removedUrls);
    }

    this.prefetchMetadata(updatedList, true, () => {
      this.onNewEntries_(true, false, addedList);
      this.onScanFinished_();
      this.onScanCompleted_();
    });
  }

  /**
   * Cancels the running scan.
   */
  cancelScan() {
    if (this.scanCanceled_) {
      return;
    }
    this.scanCanceled_ = true;
    if (this.scanner_) {
      this.scanner_.cancel();
    }

    this.onScanFinished_();

    this.processNewEntriesQueue_.cancel();
    this.dispatchEvent(new CustomEvent('dir-contents-scan-canceled'));
  }

  /**
   * Called when the scanning by scanner_ is done, even when the scanning is
   * succeeded or failed. This is called before completion (or error) callback.
   *
   */
  private onScanFinished_() {
    this.scanner_ = null;
  }

  /**
   * Called when the scanning by scanner_ is succeeded.
   */
  private onScanCompleted_() {
    if (this.scanCanceled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();
      this.dispatchEvent(new CustomEvent('dir-contents-scan-completed'));
    });
  }

  /**
   * Called in case scan has failed. Should send the event.
   * @param error error.
   */
  private onScanError_(error: DOMError) {
    if (this.scanCanceled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();
      this.dispatchEvent(
          new CustomEvent('dir-contents-scan-failed', {detail: {error}}));
    });
  }

  /**
   * Called when some chunk of entries are read by scanner.
   *
   * @param refresh True to refresh metadata, or false to use cached one.
   * @param storeBased Whether the scan for `entries` was done in the store.
   * @param entries The list of the scanned entries.
   */
  private onNewEntries_(
      refresh: boolean, storeBased: boolean, entries: UniversalEntry[]) {
    if (this.scanCanceled_) {
      return;
    }

    if (entries.length === 0) {
      return;
    }

    this.processNewEntriesQueue_.run(callbackOuter => {
      const finish = () => {
        if (!this.scanCanceled_) {
          // From new entries remove all entries that are rejected by the
          // filters or are already present in the current file list.
          const currentURLs = new Set<string>();
          for (let i = 0; i < this.fileList_.length; ++i) {
            currentURLs.add(this.fileList_.item(i)!.toURL());
          }
          const entriesFiltered = entries.filter(
              (e) => this.context_.fileFilter.filter(e) &&
                  !(currentURLs.has(e.toURL())));

          // Update the filelist without waiting the metadata.
          this.fileList_.push.apply(this.fileList_, entriesFiltered);
          const event = new CustomEvent('dir-contents-scan-updated', {
            detail: {
              isStoreBased: storeBased,
            },
          });
          this.dispatchEvent(event);
        }
        callbackOuter();
      };

      // Because the prefetchMetadata can be slow, throttling by splitting
      // entries into smaller chunks to reduce UI latency.
      // TODO(hidehiko,mtomasz): This should be handled in MetadataCache.
      const MAX_CHUNK_SIZE = 25;
      const prefetchMetadataQueue = new ConcurrentQueue(4);
      for (let i = 0; i < entries.length; i += MAX_CHUNK_SIZE) {
        if (prefetchMetadataQueue.isCanceled()) {
          break;
        }

        const chunk = entries.slice(i, i + MAX_CHUNK_SIZE);
        prefetchMetadataQueue.run(
            ((chunk: UniversalEntry[], callbackInner: VoidCallback) => {
              this.prefetchMetadata(chunk, refresh, () => {
                if (!prefetchMetadataQueue.isCanceled()) {
                  if (this.scanCanceled_) {
                    prefetchMetadataQueue.cancel();
                  }
                }

                // Checks if this is the last task.
                if (prefetchMetadataQueue.getWaitingTasksCount() === 0 &&
                    prefetchMetadataQueue.getRunningTasksCount() === 1) {
                  // |callbackOuter| in |finish| must be called before
                  // |callbackInner|, to prevent double-calling.
                  finish();
                }

                callbackInner();
              });
            }).bind(null, chunk));
      }
    });
  }

  prefetchMetadata(
      entries: UniversalEntry[], refresh: boolean,
      callback: (items: MetadataItem[]) => void) {
    if (refresh) {
      this.context_.metadataModel.notifyEntriesChanged(entries);
    }
    this.context_.metadataModel
        .get(entries, this.context_.prefetchPropertyNames)
        .then(callback);
  }
}

/**
 * Scan entries using the Store and ActionsProducer to talk to the backend and
 * propagate the state.
 *
 * This adapts the Store to the existing ContentScanner architecture.
 */
export class StoreScanner extends ContentScanner {
  private store_: Store;
  private entriesCallback_?: ScanResultCallback;
  private successCallbcak_?: VoidCallback;
  private errorCallback_?: ScanErrorCallback;
  private unsubscribe_?: VoidCallback;

  constructor(private fileKey_: FileKey) {
    super();
    this.store_ = getStore();
  }

  private onDirectoryContentUpdated_(dirContent?: DirectoryContent) {
    if (!dirContent) {
      return;
    }
    if (!(this.entriesCallback_ && this.errorCallback_ &&
          this.successCallbcak_)) {
      return;
    }

    if (dirContent.status === PropStatus.ERROR) {
      // TODO(lucmult): Figure out the DOMError here.
      this.errorCallback_({} as DOMError);
      this.finalize_();
      return;
    }

    if (dirContent.status === PropStatus.STARTED &&
        dirContent.keys.length > 0) {
      const entries = this.getEntries_(dirContent.keys);
      this.entriesCallback_(entries);
      return;
    }

    if (dirContent.status === PropStatus.SUCCESS) {
      const entries = this.getEntries_(dirContent.keys);
      this.entriesCallback_(entries);
      this.successCallbcak_();
      this.finalize_();
      return;
    }
  }

  private getEntries_(keys: FileKey[]): UniversalEntry[] {
    const state = this.store_.getState();
    const entries: UniversalEntry[] = [];
    for (const k of keys) {
      const entry = getFileData(state, k)?.entry;
      if (!entry) {
        debug(`Failed to find entry for ${k}`);
        continue;
      }
      entries.push(entry);
    }
    return entries;
  }

  override async scan(
      entriesCallback: ScanResultCallback, successCallback: VoidCallback,
      errorCallback: ScanErrorCallback, _invalidateCache: boolean = false) {
    this.entriesCallback_ = entriesCallback;
    this.errorCallback_ = errorCallback;
    this.successCallbcak_ = successCallback;
    // Start listening to the store.
    this.unsubscribe_ = directoryContentSelector.subscribe(
        this.onDirectoryContentUpdated_.bind(this));

    // Dispatch action to scan in the store.
    this.store_.dispatch(fetchDirectoryContents(this.fileKey_));
  }

  override cancel() {
    super.cancel();
    this.finalize_();
  }

  private finalize_() {
    // Usubscribe from the store.
    if (this.unsubscribe_) {
      this.unsubscribe_();
    }
    this.unsubscribe_ = undefined;
    this.successCallbcak_ = undefined;
    this.errorCallback_ = undefined;
    this.entriesCallback_ = undefined;
  }

  override isStoreBased(): boolean {
    return true;
  }
}
