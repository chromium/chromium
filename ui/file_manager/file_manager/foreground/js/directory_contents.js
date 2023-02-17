// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {mountGuest} from '../../common/js/api.js';
import {AsyncQueue, ConcurrentQueue} from '../../common/js/async_util.js';
import {createDOMError} from '../../common/js/dom_utils.js';
import {FileType} from '../../common/js/file_type.js';
import {metrics} from '../../common/js/metrics.js';
import {createTrashReaders} from '../../common/js/trash.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppDirEntry} from '../../externs/files_app_entry_interfaces.js';
import {SearchFileType, SearchLocation, SearchOptions, SearchRecency} from '../../externs/ts/state.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {getDefaultSearchOptions} from '../../state/store.js';

import {constants} from './constants.js';
import {FileListModel} from './file_list_model.js';
import {MetadataModel} from './metadata/metadata_model.js';

/**
 * Scanner of the entries.
 */
export class ContentScanner {
  constructor() {
    this.cancelled_ = false;
  }

  /**
   * Starts to scan the entries. For example, starts to read the entries in a
   * directory, or starts to search with some query on a file system.
   * Derived classes must override this method.
   *
   * @param {function(Array<Entry>)} entriesCallback Called when some chunk of
   *     entries are read. This can be called a couple of times until the
   *     completion.
   * @param {function()} successCallback Called when the scan is completed
   *     successfully.
   * @param {function(DOMError)} errorCallback Called an error occurs.
   * @param {boolean=} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {}

  /**
   * Request cancelling of the running scan. When the cancelling is done,
   * an error will be reported from errorCallback passed to scan().
   */
  cancel() {
    this.cancelled_ = true;
  }
}

/**
 * Scanner of the entries in a directory.
 */
export class DirectoryContentScanner extends ContentScanner {
  /**
   * @param {DirectoryEntry|FilesAppDirEntry} entry The directory to be read.
   */
  constructor(entry) {
    super();
    this.entry_ = entry;
  }

  /**
   * Starts to read the entries in the directory.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    if (!this.entry_ || !this.entry_.createReader) {
      // If entry is not specified or if entry doesn't implement createReader,
      // we cannot read it.
      errorCallback(createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
      return;
    }

    metrics.startInterval('DirectoryScan');
    const reader = this.entry_.createReader();
    const readEntries = () => {
      reader.readEntries(entries => {
        if (this.cancelled_) {
          errorCallback(createDOMError(util.FileError.ABORT_ERR));
          return;
        }

        if (entries.length === 0) {
          // All entries are read.
          metrics.recordInterval('DirectoryScan');
          successCallback();
          return;
        }

        entriesCallback(entries);
        readEntries();
      }, errorCallback);
    };
    readEntries();
    return;
  }
}

/**
 * Scanner of the entries for the search results on Drive File System.
 */
export class DriveSearchContentScanner extends ContentScanner {
  /** @param {string} query The query string. */
  constructor(query) {
    super();
    this.query_ = query;
  }

  /**
   * Starts to search on Drive File System.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    // Let's give another search a chance to cancel us before we begin.
    setTimeout(() => {
      // Check cancelled state before read the entries.
      if (this.cancelled_) {
        errorCallback(createDOMError(util.FileError.ABORT_ERR));
        return;
      }
      chrome.fileManagerPrivate.searchDrive(
          {
            query: this.query_,
            category: chrome.fileManagerPrivate.FileCategory.ALL,
            nextFeed: '',
          },
          (entries, nextFeed) => {
            if (chrome.runtime.lastError) {
              console.error(chrome.runtime.lastError.message);
            }

            if (this.cancelled_) {
              errorCallback(createDOMError(util.FileError.ABORT_ERR));
              return;
            }

            // TODO(tbarzic): Improve error handling.
            if (!entries) {
              console.warn('Drive search encountered an error.');
              errorCallback(
                  createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
              return;
            }

            if (entries.length >= DriveSearchContentScanner.MAX_RESULTS_) {
              // More results were received than expected, so trim.
              entries =
                  entries.slice(0, DriveSearchContentScanner.MAX_RESULTS_);
            }

            if (entries.length > 0) {
              entriesCallback(entries);
            }

            successCallback();
          });
    }, DriveSearchContentScanner.SCAN_DELAY_);
    return;
  }
}

/**
 * Delay in milliseconds to be used for drive search scan, in order to reduce
 * the number of server requests while user is typing the query.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.SCAN_DELAY_ = 200;

/**
 * Maximum number of results which is shown on the search.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.MAX_RESULTS_ = 100;

/**
 * Scanner of the entries of the file name search on the directory tree, whose
 * root is entry.
 */
export class LocalSearchContentScanner extends ContentScanner {
  /**
   * @param {DirectoryEntry} entry The root of the search target directory tree.
   * @param {string} query The query of the search.
   */
  constructor(entry, query) {
    super();
    this.entry_ = entry;
    this.query_ = query.toLowerCase();
  }

  /**
   * Starts the file name search.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    util.readEntriesRecursively(assert(this.entry_), (entries) => {
      const matchEntries = entries.filter(
          entry => entry.name.toLowerCase().indexOf(this.query_) >= 0);
      if (matchEntries.length > 0) {
        entriesCallback(matchEntries);
      }
    }, successCallback, errorCallback, () => this.cancelled_);
    return;
  }
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
  /**
   * @param {!VolumeManagerCommon.RootType|null} rootType The root type of the
   *    location in the directory tree, if known.
   * @param {!DirectoryEntry} entry The current directory.
   * @param {!string} query The query of the search.
   * @param {SearchOptions=} options The options for the search.
   */
  constructor(rootType, entry, query, options = undefined) {
    super();
    this.rootType_ = rootType;
    this.entry_ = entry;
    this.query_ = query.toLowerCase();
    this.options_ = options || getDefaultSearchOptions();
  }

  /**
   * For the given options returns the category of files to which the search
   * should be limited (e.g., images, videos, etc.).
   */
  getDesiredCategory_() {
    switch (this.options_.type) {
      case SearchFileType.AUDIO:
        return chrome.fileManagerPrivate.FileCategory.AUDIO;
      case SearchFileType.DOCUMENTS:
        return chrome.fileManagerPrivate.FileCategory.DOCUMENT;
      case SearchFileType.IMAGES:
        return chrome.fileManagerPrivate.FileCategory.IMAGE;
      case SearchFileType.VIDEOS:
        return chrome.fileManagerPrivate.FileCategory.VIDEO;
      default:
        return chrome.fileManagerPrivate.FileCategory.ALL;
    }
  }

  isSearchingRoot_() {
    if (this.options_.location === SearchLocation.EVERYWHERE ||
        this.options_.location === SearchLocation.THIS_CHROMEBOOK) {
      return true;
    }
  }

  /**
   * @returns Whether or not the local (MY_FILES) search should be performed.
   * @private
   */
  isSearchingLocal_() {
    if (this.isSearchingRoot_()) {
      return true;
    }
    if (this.options_.location === SearchLocation.THIS_FOLDER) {
      return (this.rootType_ !== VolumeManagerCommon.RootType.DRIVE);
    }
    return false;
  }

  /**
   * Computes the timestamp based on options. If the options ask for today's
   * results, it uses the time in ms from midnight. For yesterday, it goes back
   * by one day from midnight. For week, it goes back by 6 days from midnight.
   * For a month, it goes back by 30 days since midnight, regardless of how
   * many days are in the current month. For a year, it goes back by 365 days
   * since midnight, regardless if the current year is a leap year or not.
   * @private
   */
  getEarliestTimestamp_() {
    const now = new Date();
    const midnight = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    const midnightMs = midnight.getTime();
    const dayMs = 24 * 60 * 60 * 1000;

    switch (this.options_.recency) {
      case SearchRecency.TODAY:
        return midnightMs;
      case SearchRecency.YESTERDAY:
        return midnightMs - 1 * dayMs;
      case SearchRecency.LAST_WEEK:
        return midnightMs - 6 * dayMs;
      case SearchRecency.LAST_MONTH:
        return midnightMs - 30 * dayMs;
      case SearchRecency.LAST_YEAR:
        return midnightMs - 365 * dayMs;
      default:
        return 0;
    }
  }

  /**
   * @returns Whether or not the Google Drive search should be performed.
   * @private
   */
  isSearchingDrive_() {
    if (this.options_.location === SearchLocation.EVERYWHERE) {
      return true;
    }
    if (this.options_.location === SearchLocation.THIS_FOLDER) {
      return (this.rootType_ === VolumeManagerCommon.RootType.DRIVE);
    }
    return false;
  }

  /**
   * Starts the file name search.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    const searchPromises = [];
    const category = this.getDesiredCategory_();
    if (this.isSearchingLocal_()) {
      searchPromises.push(new Promise((resolve, reject) => {
        const rootDir =
            this.isSearchingRoot_() ? this.entry_.filesystem.root : this.entry_;
        const timestamp = this.getEarliestTimestamp_();
        chrome.fileManagerPrivate.searchFiles(
            {
              rootDir: rootDir,
              query: this.query_,
              types: chrome.fileManagerPrivate.SearchType.ALL,
              maxResults: 100,
              timestamp: timestamp,
              category: category,
            },
            /**
             * @param {!Array<!Entry>} entries
             */
            (entries) => {
              if (this.cancelled_) {
                reject(createDOMError(util.FileError.ABORT_ERR));
              } else if (chrome.runtime.lastError) {
                reject(createDOMError(
                    util.FileError.NOT_READABLE_ERR,
                    chrome.runtime.lastError.message));
              } else {
                resolve(entries);
              }
            });
      }));
    }
    if (this.isSearchingDrive_()) {
      searchPromises.push(new Promise((resolve, reject) => {
        chrome.fileManagerPrivate.searchDrive(
            {
              query: this.query_,
              category: category,
              nextFeed: '',
            },
            (entries, nextFeed) => {
              if (chrome.runtime.lastError) {
                reject(createDOMError(
                    util.FileError.NOT_READABLE_ERR,
                    chrome.runtime.lastError.message));
              } else if (this.cancelled_) {
                reject(createDOMError(util.FileError.ABORT_ERR));
              } else if (!entries) {
                reject(createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
              } else {
                resolve(entries);
              }
            });
      }));
    }
    if (!searchPromises) {
      console.warn(
          `No search promises for options ${JSON.stringify(this.options_)}`);
      successCallback();
    }
    Promise.allSettled(searchPromises).then((results) => {
      for (const result of results) {
        if (result.status === 'rejected') {
          errorCallback(/** @type {DOMError} */ (result.reason));
        } else if (result.status === 'fulfilled') {
          entriesCallback(result.value);
        }
      }
      successCallback();
    });
  }
}

/**
 * Scanner of the entries for the metadata search on Drive File System.
 */
export class DriveMetadataSearchContentScanner extends ContentScanner {
  /**
   * @param {!chrome.fileManagerPrivate.SearchType} searchType The
   *     option of the search.
   */
  constructor(searchType) {
    super();
    this.searchType_ = searchType;
  }

  /**
   * Starts to metadata-search on Drive File System.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    chrome.fileManagerPrivate.searchDriveMetadata(
        {query: '', types: this.searchType_, maxResults: 100}, results => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
          }
          if (this.cancelled_) {
            errorCallback(createDOMError(util.FileError.ABORT_ERR));
            return;
          }

          if (!results) {
            console.warn('Drive search encountered an error.');
            errorCallback(
                createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
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
    return;
  }
}

export class RecentContentScanner extends ContentScanner {
  /**
   * @param {string} query Search query.
   * @param {VolumeManager} volumeManager Volume manager.
   * @param {chrome.fileManagerPrivate.SourceRestriction=} opt_sourceRestriction
   * @param {chrome.fileManagerPrivate.FileCategory=} opt_fileCategory
   */
  constructor(query, volumeManager, opt_sourceRestriction, opt_fileCategory) {
    super();

    /**
     * @private {string}
     */
    this.query_ = query.toLowerCase();

    /**
     * @private {VolumeManager}
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private {chrome.fileManagerPrivate.SourceRestriction}
     */
    this.sourceRestriction_ = opt_sourceRestriction ||
        chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE;

    /**
     * @private {chrome.fileManagerPrivate.FileCategory}
     */
    this.fileCategory_ =
        opt_fileCategory || chrome.fileManagerPrivate.FileCategory.ALL;
  }

  /**
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    /** @type {function(!FileEntry): boolean} */
    const isMatchQuery = (entry) =>
        entry.name.toLowerCase().indexOf(this.query_) >= 0;
    /**
     * Files app launched with "volumeFilter" launch parameter will filter out
     * some volumes. Before returning the recent entries, we need to check if
     * the entry's volume location is valid or not (crbug.com/1333385/#c17).
     */
    /** @type {function(!FileEntry): boolean} */
    const isAllowedVolume = (entry) =>
        this.volumeManager_.getVolumeInfo(entry) !== null;
    chrome.fileManagerPrivate.getRecentFiles(
        this.sourceRestriction_, this.fileCategory_, invalidateCache,
        entries => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            errorCallback(
                createDOMError(util.FileError.INVALID_MODIFICATION_ERR));
            return;
          }
          if (entries.length > 0) {
            entriesCallback(entries.filter(
                entry => isMatchQuery(entry) && isAllowedVolume(entry)));
          }
          successCallback();
        });
    return;
  }
}

/**
 * Scanner of media-view volumes.
 */
export class MediaViewContentScanner extends ContentScanner {
  /**
   * @param {!DirectoryEntry} rootEntry The root entry of the media-view volume.
   */
  constructor(rootEntry) {
    super();
    this.rootEntry_ = rootEntry;
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
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    // To provide flatten view of files, this media-view scanner retrieves files
    // in directories inside the media's root entry recursively.
    util.readEntriesRecursively(
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
 * NavigationListModel.reorderNavigationItems_ will detect that crostini
 * is mounted as a disk volume and hide the fake root item while the
 * disk volume exists.
 */
export class CrostiniMounter extends ContentScanner {
  /**
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    chrome.fileManagerPrivate.mountCrostini(() => {
      if (chrome.runtime.lastError) {
        console.warn(`Cannot mount Crostini volume: ${
            chrome.runtime.lastError.message}`);
        errorCallback(createDOMError(
            constants.CROSTINI_CONNECT_ERR, chrome.runtime.lastError.message));
        return;
      }
      successCallback();
    });
    return;
  }
}

/**
 * Shows an empty list and spinner whilst starting and mounting a Guest OS's
 * shared files.
 *
 * When FilesApp starts, the related placeholder root entry is shown which uses
 * this GuestOsMounter as its ContentScanner. When the mount succeeds it will
 * show up as a disk volume. NavigationListModel.reorderNavigationItems_ will
 * detect thew new volume and hide the placeholder root item while the disk
 * volume exists.
 */
export class GuestOsMounter extends ContentScanner {
  /**
   * @param {number} guest_id The id of the GuestOsMountProvider to use
   */
  constructor(guest_id) {
    super();

    /** @private @const {number} */
    this.guest_id_ = guest_id;
  }

  /**
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    try {
      await mountGuest(this.guest_id_);
      successCallback();
    } catch (error) {
      errorCallback(createDOMError(
          // TODO(crbug/1293229): Strings
          constants.CROSTINI_CONNECT_ERR, error));
    }
    return;
  }
}

/**
 * Read all the Trash directories for content.
 */
export class TrashContentScanner extends ContentScanner {
  /**
   * @param {!VolumeManager} volumeManager Identifies the underlying filesystem.
   */
  constructor(volumeManager) {
    super();

    this.readers_ = createTrashReaders(volumeManager);
  }

  /**
   * Scan all the trash directories for content.
   * @override
   */
  async scan(
      entriesCallback, successCallback, errorCallback,
      invalidateCache = false) {
    const readEntries = (idx) => {
      if (this.readers_.length === idx) {
        // All Trash directories have been read.
        successCallback();
        return;
      }
      this.readers_[idx].readEntries(entries => {
        if (this.cancelled_) {
          errorCallback(createDOMError(util.FileError.ABORT_ERR));
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

/**
 * This class manages filters and determines a file should be shown or not.
 * When filters are changed, a 'changed' event is fired.
 */
export class FileFilter extends EventTarget {
  /** @param {!VolumeManager} volumeManager */
  constructor(volumeManager) {
    super();

    /**
     * @type {Object<Function>}
     * @private
     */
    this.filters_ = {};

    /**
     * @type {!VolumeManager}
     * @const
     * @private
     */
    this.volumeManager_ = volumeManager;

    /**
     * Setup initial filters.
     */
    this.setupInitialFilters_();
  }

  /**
   * @private
   */
  setupInitialFilters_() {
    this.setHiddenFilesVisible(false);
    this.setAllAndroidFoldersVisible(false);
    this.hideAndroidDownload();
  }

  /**
   * @param {string} name Filter identifier.
   * @param {function(Entry)} callback A filter - a function receiving an Entry,
   *     and returning bool.
   */
  addFilter(name, callback) {
    this.filters_[name] = callback;
    dispatchSimpleEvent(this, 'changed');
  }

  /**
   * @param {string} name Filter identifier.
   */
  removeFilter(name) {
    delete this.filters_[name];
    dispatchSimpleEvent(this, 'changed');
  }

  /**
   * Show/Hide hidden files (i.e. files starting with '.', or other system files
   * for Windows files).
   * @param {boolean} visible True if hidden files should be visible to the
   *     user.
   */
  setHiddenFilesVisible(visible) {
    if (!visible) {
      this.addFilter('hidden', entry => {
        if (entry.name.startsWith('.')) {
          return false;
        }
        // Only hide WINDOWS_HIDDEN in downloads:/PvmDefault.
        if (entry.fullPath.startsWith('/PvmDefault/') &&
            FileFilter.WINDOWS_HIDDEN.includes(entry.name)) {
          const info = this.volumeManager_.getLocationInfo(entry);
          if (info &&
              info.rootType === VolumeManagerCommon.RootType.DOWNLOADS) {
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
   * @return {boolean} True if hidden files are visible to the user now.
   */
  isHiddenFilesVisible() {
    return !('hidden' in this.filters_);
  }

  /**
   * Show/Hide uncommon Android folders.
   * @param {boolean} visible True if uncommon folders should be visible to the
   *     user.
   */
  setAllAndroidFoldersVisible(visible) {
    if (!visible) {
      this.addFilter('android_hidden', entry => {
        if (entry.filesystem && entry.filesystem.name !== 'android_files') {
          return true;
        }
        // Hide top-level folder or sub-folders that should be hidden.
        if (entry.fullPath) {
          const components = entry.fullPath.split('/');
          if (components[1] &&
              FileFilter.DEFAULT_ANDROID_FOLDERS.indexOf(components[1]) == -1) {
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
   * @return {boolean} True if uncommon folders is visible to the user now.
   */
  isAllAndroidFoldersVisible() {
    return !('android_hidden' in this.filters_);
  }

  /**
   * Sets up a filter to hide /Download directory in 'Play files' volume.
   *
   * "Play files/Download" is an alias to Chrome OS's Downloads volume. It is
   * convenient in Android file picker, but can be confusing in Chrome OS Files
   * app. This function adds a filter to hide the Android's /Download.
   */
  hideAndroidDownload() {
    this.addFilter('android_download', entry => {
      if (entry.filesystem && entry.filesystem.name === 'android_files' &&
          entry.fullPath === '/Download') {
        return false;
      }
      return true;
    });
  }

  /**
   * @param {Entry} entry File entry.
   * @return {boolean} True if the file should be shown, false otherwise.
   */
  filter(entry) {
    for (const name in this.filters_) {
      if (!this.filters_[name](entry)) {
        return false;
      }
    }
    return true;
  }
}

/**
 * Top-level Android folders which are visible by default.
 * @const {!Array<string>}
 */
FileFilter.DEFAULT_ANDROID_FOLDERS =
    ['Documents', 'Movies', 'Music', 'Pictures'];

/**
 * Windows files or folders to hide by default.
 * @const {!Array<string>}
 */
FileFilter.WINDOWS_HIDDEN = ['$RECYCLE.BIN'];


/**
 * A context of DirectoryContents.
 * TODO(yoshiki): remove this. crbug.com/224869.
 */
export class FileListContext {
  /**
   * @param {FileFilter} fileFilter The file-filter context.
   * @param {!MetadataModel} metadataModel
   * @param {!VolumeManager} volumeManager The volume manager.
   */
  constructor(fileFilter, metadataModel, volumeManager) {
    /**
     * @type {FileListModel}
     */
    this.fileList = new FileListModel(metadataModel);

    /**
     * @public {!MetadataModel}
     * @const
     */
    this.metadataModel = metadataModel;

    /**
     * @type {FileFilter}
     */
    this.fileFilter = fileFilter;

    /**
     * @public {!Array<string>}
     * @const
     */
    this.prefetchPropertyNames = Array.from(new Set([
      ...constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
      ...constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES,
      ...constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES,
      ...constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES,
    ]));

    /** @public {!VolumeManager} */
    this.volumeManager = volumeManager;
  }
}

/**
 * This class is responsible for scanning directory (or search results),
 * and filling the fileList. Different descendants handle various types of
 * directory contents shown: basic directory, drive search results, local search
 * results.
 * TODO(hidehiko): Remove EventTarget from this.
 */
export class DirectoryContents extends EventTarget {
  /**
   *
   * @param {FileListContext} context The file list context.
   * @param {boolean} isSearch True for search directory contents, otherwise
   *     false.
   * @param {DirectoryEntry|FakeEntry|FilesAppDirEntry} directoryEntry The entry
   *     of the current directory.
   * @param {function():ContentScanner} scannerFactory The factory to create
   *     ContentScanner instance.
   */
  constructor(context, isSearch, directoryEntry, scannerFactory) {
    super();

    /** @private {FileListContext} */
    this.context_ = context;

    /** @private {FileListModel} */
    this.fileList_ = context.fileList;
    this.fileList_.InitNewDirContents(context.volumeManager);

    this.isSearch_ = isSearch;
    this.directoryEntry_ = directoryEntry;

    this.scannerFactory_ = scannerFactory;
    this.scanner_ = null;
    this.processNewEntriesQueue_ = new AsyncQueue();
    this.scanCancelled_ = false;

    /**
     * Metadata snapshot which is used to know which file is actually changed.
     * @type {Object}
     */
    this.metadataSnapshot_ = null;
  }

  /**
   * Create the copy of the object, but without scan started.
   * @return {!DirectoryContents} Object copy.
   */
  clone() {
    return new DirectoryContents(
        this.context_, this.isSearch_, this.directoryEntry_,
        this.scannerFactory_);
  }

  /**
   * Returns the file list length.
   * @return {number}
   */
  getFileListLength() {
    return this.fileList_.length;
  }

  /**
   * Use a given fileList instead of the fileList from the context.
   * @param {!FileListModel} fileList The new file list.
   */
  setFileList(fileList) {
    this.fileList_ = fileList;
  }

  /**
   * Creates snapshot of metadata in the directory.
   * @return {!Object} Metadata snapshot of current directory contents.
   */
  createMetadataSnapshot() {
    const snapshot = {};
    const entries = /** @type {!Array<!Entry>} */ (this.fileList_.slice());
    const metadata =
        this.context_.metadataModel.getCache(entries, ['modificationTime']);
    for (let i = 0; i < entries.length; i++) {
      snapshot[entries[i].toURL()] = metadata[i];
    }
    return snapshot;
  }

  /**
   * Sets metadata snapshot which is used to check changed files.
   * @param {!Object} metadataSnapshot A metadata snapshot.
   */
  setMetadataSnapshot(metadataSnapshot) {
    this.metadataSnapshot_ = metadataSnapshot;
  }

  /**
   * Use the filelist from the context and replace its contents with the entries
   * from the current fileList. If metadata snapshot is set, this method checks
   * actually updated files and dispatch change events by calling updateIndexes.
   */
  replaceContextFileList() {
    if (this.context_.fileList !== this.fileList_) {
      // TODO(yawano): While we should update the list with adding or deleting
      // what actually added and deleted instead of deleting and adding all
      // items, splice of array data model is expensive since it always runs
      // sort and we replace the list in this way to reduce the number of splice
      // calls.
      const spliceArgs = this.fileList_.slice();
      const fileList = this.context_.fileList;
      spliceArgs.unshift(0, fileList.length);
      fileList.splice.apply(fileList, spliceArgs);
      this.fileList_ = fileList;

      // Check updated files and dispatch change events.
      if (this.metadataSnapshot_) {
        const updatedIndexes = [];
        const entries = /** @type {!Array<!Entry>} */ (this.fileList_.slice());
        const newMetadatas =
            this.context_.metadataModel.getCache(entries, ['modificationTime']);

        for (let i = 0; i < entries.length; i++) {
          const url = entries[i].toURL();
          const newMetadata = newMetadatas[i];
          // If the Files app fails to obtain both old and new modificationTime,
          // regard the entry as not updated.
          if ((this.metadataSnapshot_[url] &&
               this.metadataSnapshot_[url].modificationTime &&
               this.metadataSnapshot_[url].modificationTime.getTime()) !==
              (newMetadata.modificationTime &&
               newMetadata.modificationTime.getTime())) {
            updatedIndexes.push(i);
          }
        }

        if (updatedIndexes.length > 0) {
          this.fileList_.updateIndexes(updatedIndexes);
        }
      }
    }
  }

  /**
   * @return {boolean} If the scan is active.
   */
  isScanning() {
    return this.scanner_ || this.processNewEntriesQueue_.isRunning();
  }

  /**
   * @return {boolean} True if search results (drive or local).
   */
  isSearch() {
    return this.isSearch_;
  }

  /**
   * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} A DirectoryEntry for
   *     current directory. In case of search -- the top directory from which
   *     search is run.
   */
  getDirectoryEntry() {
    return this.directoryEntry_;
  }

  /**
   * Start directory scan/search operation. Either 'scan-completed' or
   * 'scan-failed' event will be fired upon completion.
   *
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {boolean} invalidateCache True to invalidate the backend scanning
   *     result cache. This param only works if the corresponding backend
   *     scanning supports cache.
   */
  scan(refresh, invalidateCache) {
    /**
     * Invoked when the scanning is completed successfully.
     * @this {DirectoryContents}
     */
    function completionCallback() {
      this.onScanFinished_();
      this.onScanCompleted_();
    }

    /**
     * Invoked when the scanning is finished but is not completed due to error.
     * @param {DOMError} error error.
     * @this {DirectoryContents}
     */
    function errorCallback(error) {
      this.onScanFinished_();
      this.onScanError_(error);
    }

    // TODO(hidehiko,mtomasz): this scan method must be called at most once.
    // Remove such a limitation.
    this.scanner_ = this.scannerFactory_();
    this.scanner_.scan(
        this.onNewEntries_.bind(this, refresh), completionCallback.bind(this),
        errorCallback.bind(this), invalidateCache);
  }

  /**
   * Adds/removes/updates items of file list.
   * @param {Array<Entry>} updatedEntries Entries of updated/added files.
   * @param {Array<string>} removedUrls URLs of removed files.
   */
  update(updatedEntries, removedUrls) {
    const removedMap = {};
    for (let i = 0; i < removedUrls.length; i++) {
      removedMap[removedUrls[i]] = true;
    }

    const updatedMap = {};
    for (let i = 0; i < updatedEntries.length; i++) {
      updatedMap[updatedEntries[i].toURL()] = updatedEntries[i];
    }

    const updatedList = [];
    const updatedIndexes = [];
    for (let i = 0; i < this.fileList_.length; i++) {
      const url = this.fileList_.item(i).toURL();

      if (url in removedMap) {
        // Find the maximum range in which all items need to be removed.
        const begin = i;
        let end = i + 1;
        while (end < this.fileList_.length &&
               this.fileList_.item(end).toURL() in removedMap) {
          end++;
        }
        // Remove the range [begin, end) at once to avoid multiple sorting.
        this.fileList_.splice(begin, end - begin);
        i--;
        continue;
      }

      if (url in updatedMap) {
        updatedList.push(updatedMap[url]);
        updatedIndexes.push(i);
        delete updatedMap[url];
      }
    }

    if (updatedIndexes.length > 0) {
      this.fileList_.updateIndexes(updatedIndexes);
    }

    const addedList = [];
    for (const url in updatedMap) {
      addedList.push(updatedMap[url]);
    }

    if (removedUrls.length > 0) {
      this.context_.metadataModel.notifyEntriesRemoved(removedUrls);
    }

    this.prefetchMetadata(updatedList, true, () => {
      this.onNewEntries_(true, addedList);
      this.onScanFinished_();
      this.onScanCompleted_();
    });
  }

  /**
   * Cancels the running scan.
   */
  cancelScan() {
    if (this.scanCancelled_) {
      return;
    }
    this.scanCancelled_ = true;
    if (this.scanner_) {
      this.scanner_.cancel();
    }

    this.onScanFinished_();

    this.processNewEntriesQueue_.cancel();
    dispatchSimpleEvent(this, 'scan-cancelled');
  }

  /**
   * Called when the scanning by scanner_ is done, even when the scanning is
   * succeeded or failed. This is called before completion (or error) callback.
   *
   * @private
   */
  onScanFinished_() {
    this.scanner_ = null;
  }

  /**
   * Called when the scanning by scanner_ is succeeded.
   * @private
   */
  onScanCompleted_() {
    if (this.scanCancelled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();
      dispatchSimpleEvent(this, 'scan-completed');
    });
  }

  /**
   * Called in case scan has failed. Should send the event.
   * @param {DOMError} error error.
   * @private
   */
  onScanError_(error) {
    if (this.scanCancelled_) {
      return;
    }

    this.processNewEntriesQueue_.run(callback => {
      // Call callback first, so isScanning() returns false in the event
      // handlers.
      callback();
      const event = new Event('scan-failed');
      event.error = error;
      this.dispatchEvent(event);
    });
  }

  /**
   * Called when some chunk of entries are read by scanner.
   *
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {Array<Entry>} entries The list of the scanned entries.
   * @private
   */
  onNewEntries_(refresh, entries) {
    if (this.scanCancelled_) {
      return;
    }

    if (entries.length === 0) {
      return;
    }

    // Caching URL to reduce a number of calls of toURL in sort.
    // This is a temporary solution. We need to fix a root cause of slow toURL.
    // See crbug.com/370908 for detail.
    entries.forEach(entry => {
      entry['cachedUrl'] = entry.toURL();
    });

    this.processNewEntriesQueue_.run(callbackOuter => {
      const finish = () => {
        if (!this.scanCancelled_) {
          // From new entries remove all entries that are rejected by the
          // filters or are already present in the current file list.
          const currentURLs = {};
          for (let i = 0; i < this.fileList_.length; ++i) {
            currentURLs[this.fileList_.item(i).toURL()] = true;
          }
          const entriesFiltered = entries.filter(
              (e) => this.context_.fileFilter.filter(e) &&
                  !(e['cachedUrl'] in currentURLs));

          // Update the filelist without waiting the metadata.
          this.fileList_.push.apply(this.fileList_, entriesFiltered);
          dispatchSimpleEvent(this, 'scan-updated');
        }
        callbackOuter();
      };
      // Because the prefetchMetadata can be slow, throttling by splitting
      // entries into smaller chunks to reduce UI latency.
      // TODO(hidehiko,mtomasz): This should be handled in MetadataCache.
      const MAX_CHUNK_SIZE = 25;
      const prefetchMetadataQueue = new ConcurrentQueue(4);
      for (let i = 0; i < entries.length; i += MAX_CHUNK_SIZE) {
        if (prefetchMetadataQueue.isCancelled()) {
          break;
        }

        const chunk = entries.slice(i, i + MAX_CHUNK_SIZE);
        prefetchMetadataQueue.run(
            ((chunk, callbackInner) => {
              this.prefetchMetadata(chunk, refresh, () => {
                if (!prefetchMetadataQueue.isCancelled()) {
                  if (this.scanCancelled_) {
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

  /**
   * @param {!Array<!Entry>} entries Files.
   * @param {boolean} refresh True to refresh metadata, or false to use cached
   *     one.
   * @param {function(Object)} callback Callback on done.
   */
  prefetchMetadata(entries, refresh, callback) {
    if (refresh) {
      this.context_.metadataModel.notifyEntriesChanged(entries);
    }
    this.context_.metadataModel
        .get(entries, this.context_.prefetchPropertyNames)
        .then(callback);
  }
}
