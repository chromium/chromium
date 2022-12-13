// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain utility functions used only by the
 * files app. Other shared utility functions can be found in base/*_util.js,
 * which allows finer-grained control over introducing dependencies.
 */

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {promisify} from './api.js';
import {createDOMError} from './dom_utils.js';
import {EntryList} from './files_app_entry_types.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Namespace for utility functions.
 */
const util = {};

/**
 * @param {!chrome.fileManagerPrivate.IconSet} iconSet Set of icons.
 * @return {string} CSS value.
 */
util.iconSetToCSSBackgroundImageValue = iconSet => {
  let lowDpiPart = null;
  let highDpiPart = null;
  if (iconSet.icon16x16Url) {
    lowDpiPart = 'url(' + iconSet.icon16x16Url + ') 1x';
  }
  if (iconSet.icon32x32Url) {
    highDpiPart = 'url(' + iconSet.icon32x32Url + ') 2x';
  }

  if (lowDpiPart && highDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ', ' + highDpiPart + ')';
  } else if (lowDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ')';
  } else if (highDpiPart) {
    return '-webkit-image-set(' + highDpiPart + ')';
  }

  return 'none';
};

/**
 * Mapping table of file error name to i18n localized error name.
 *
 * @const @enum {string}
 */
util.FileErrorLocalizedName = {
  'InvalidModificationError': 'FILE_ERROR_INVALID_MODIFICATION',
  'InvalidStateError': 'FILE_ERROR_INVALID_STATE',
  'NoModificationAllowedError': 'FILE_ERROR_NO_MODIFICATION_ALLOWED',
  'NotFoundError': 'FILE_ERROR_NOT_FOUND',
  'NotReadableError': 'FILE_ERROR_NOT_READABLE',
  'PathExistsError': 'FILE_ERROR_PATH_EXISTS',
  'QuotaExceededError': 'FILE_ERROR_QUOTA_EXCEEDED',
  'SecurityError': 'FILE_ERROR_SECURITY',
};
Object.freeze(util.FileErrorLocalizedName);

/**
 * Returns i18n localized error name for file error |name|.
 *
 * @param {?string|undefined} name File error name.
 * @return {string} Translated file error string.
 */
util.getFileErrorString = name => {
  const error = util.FileErrorLocalizedName[name] || 'FILE_ERROR_GENERIC';
  return loadTimeData.getString(error);
};

/**
 * Mapping table for FileError.code style enum to DOMError.name string.
 *
 * @const @enum {string}
 */
util.FileError = {
  ABORT_ERR: 'AbortError',
  INVALID_MODIFICATION_ERR: 'InvalidModificationError',
  INVALID_STATE_ERR: 'InvalidStateError',
  NO_MODIFICATION_ALLOWED_ERR: 'NoModificationAllowedError',
  NOT_FOUND_ERR: 'NotFoundError',
  NOT_READABLE_ERR: 'NotReadable',
  PATH_EXISTS_ERR: 'PathExistsError',
  QUOTA_EXCEEDED_ERR: 'QuotaExceededError',
  TYPE_MISMATCH_ERR: 'TypeMismatchError',
  ENCODING_ERR: 'EncodingError',
};
Object.freeze(util.FileError);

/**
 * Remove a file or a directory.
 * @param {Entry} entry The entry to remove.
 * @param {function()} onSuccess The success callback.
 * @param {function(DOMError)} onError The error callback.
 */
util.removeFileOrDirectory = (entry, onSuccess, onError) => {
  if (entry.isDirectory) {
    entry.removeRecursively(onSuccess, onError);
  } else {
    entry.remove(onSuccess, onError);
  }
};

/**
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 *
 * @param {number} bytes The number of bytes.
 * @param {number=} addedPrecision The number of precision digits to add.
 * @return {string} Localized string.
 */
util.bytesToString = (bytes, addedPrecision = 0) => {
  // Translation identifiers for size units.
  const UNITS = [
    'SIZE_BYTES',
    'SIZE_KB',
    'SIZE_MB',
    'SIZE_GB',
    'SIZE_TB',
    'SIZE_PB',
  ];

  // Minimum values for the units above.
  const STEPS = [
    0,
    Math.pow(2, 10),
    Math.pow(2, 20),
    Math.pow(2, 30),
    Math.pow(2, 40),
    Math.pow(2, 50),
  ];

  // Rounding with precision.
  const round = (value, decimals) => {
    const scale = Math.pow(10, decimals);
    return Math.round(value * scale) / scale;
  };

  const str = (n, u) => {
    return strf(u, n.toLocaleString());
  };

  const fmt = (s, u) => {
    const rounded = round(bytes / s, 1 + addedPrecision);
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  if (bytes < STEPS[1]) {
    return str(bytes, UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs, or with the desired
  // number of precision digits.
  if (bytes < STEPS[2]) {
    const rounded = addedPrecision ? round(bytes / STEPS[1], addedPrecision) :
                                     Math.ceil(bytes / STEPS[1]);
    return str(rounded, UNITS[1]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  let i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    if (bytes < STEPS[i + 1]) {
      return fmt(STEPS[i], UNITS[i]);
    }
  }

  return fmt(STEPS[i], UNITS[i]);
};

/**
 * Extracts path from filesystem: URL.
 * @param {?string=} url Filesystem URL.
 * @return {?string} The path if it can be parsed, null if it cannot.
 */
util.extractFilePath = url => {
  const match =
      /^filesystem:[\w-]*:\/\/[\w-]*\/(external|persistent|temporary)(\/.*)$/
          .exec(url);
  const path = match && match[2];
  if (!path) {
    return null;
  }
  return decodeURIComponent(path);
};

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
export function str(id) {
  try {
    return loadTimeData.getString(id);
  } catch (e) {
    console.warn('Failed to get string for', id);
    return id;
  }
}

/**
 * Returns a translated string with arguments replaced.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getStringF(id, ...).
 *
 * @param {string} id The id of the string to return.
 * @param {...*} var_args The values to replace into the string.
 * @return {string} The translated string with replaced values.
 */
export function strf(id, var_args) {
  return loadTimeData.getStringF.apply(loadTimeData, arguments);
}

// Export strf() into the util namespace.
util.strf = strf;

/**
 * @return {boolean} True if the Files app is running as an open files or a
 *     select folder dialog. False otherwise.
 */
util.runningInBrowser = () => {
  return !window.appID;
};

/**
 * The type of a file operation.
 * @enum {string}
 * @const
 */
util.FileOperationType = {
  COPY: 'COPY',
  DELETE: 'DELETE',
  MOVE: 'MOVE',
  RESTORE: 'RESTORE',
  RESTORE_TO_DESTINATION: 'RESTORE_TO_DESTINATION',
  ZIP: 'ZIP',
};
Object.freeze(util.FileOperationType);

/**
 * The type of a file operation error.
 * @enum {number}
 * @const
 */
util.FileOperationErrorType = {
  UNEXPECTED_SOURCE_FILE: 0,
  TARGET_EXISTS: 1,
  FILESYSTEM_ERROR: 2,
};
Object.freeze(util.FileOperationErrorType);

/**
 * The kind of an entry changed event.
 * @enum {number}
 * @const
 */
util.EntryChangedKind = {
  CREATED: 0,
  DELETED: 1,
};
Object.freeze(util.EntryChangedKind);

/**
 * Obtains whether an entry is fake or not.
 * @param {(!Entry|!FilesAppEntry)} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is fake.
 * @suppress {missingProperties} Closure compiler doesn't allow to call isNative
 * on Entry which is native and thus doesn't define this property, however we
 * handle undefined accordingly.
 * TODO(lucmult): Remove @suppress once all entries are sub-type of
 * FilesAppEntry.
 */
util.isFakeEntry = entry => {
  return (
      entry.getParent === undefined ||
      (entry.isNativeType !== undefined && !entry.isNativeType));
};

/**
 * Obtains whether an entry is the root directory of a Shared Drive.
 * @param {Entry|FilesAppEntry} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is root of a Shared Drive.
 */
util.isTeamDriveRoot = entry => {
  if (entry === null) {
    return false;
  }
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 3 && util.isSharedDriveEntry(entry);
};

/**
 * Obtains whether an entry is the grand root directory of Shared Drives.
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is the grand root of Shared Drives.
 */
util.isTeamDrivesGrandRoot = entry => {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 2 && util.isSharedDriveEntry(entry);
};

/**
 * Obtains whether an entry is descendant of the Shared Drives directory.
 * @param {!Entry|!FilesAppEntry} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is under Shared Drives.
 */
util.isSharedDriveEntry = entry => {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] == '' &&
      tree[1] == VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME;
};

/**
 * Extracts Shared Drive name from entry path.
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {string} The name of Shared Drive. Empty string if |entry| is not
 *     under Shared Drives.
 */
util.getTeamDriveName = entry => {
  if (!entry.fullPath || !util.isSharedDriveEntry(entry)) {
    return '';
  }
  const tree = entry.fullPath.split('/');
  if (tree.length < 3) {
    return '';
  }
  return tree[2];
};

/**
 * Returns true if the given root type is for a container of recent files.
 * @param {VolumeManagerCommon.RootType|null} rootType
 * @return {boolean}
 */
util.isRecentRootType = rootType => {
  return rootType == VolumeManagerCommon.RootType.RECENT;
};

/**
 * Returns true if the given entry is the root folder of recent files.
 * @param {!Entry|!FilesAppEntry} entry Entry or a fake entry.
 * @returns {boolean}
 */
util.isRecentRoot = entry => {
  return util.isFakeEntry(entry) && util.isRecentRootType(entry.rootType);
};

/**
 * Obtains whether an entry is the root directory of a Computer.
 * @param {Entry|FilesAppEntry} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is root of a Computer.
 */
util.isComputersRoot = entry => {
  if (entry === null) {
    return false;
  }
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 3 && util.isComputersEntry(entry);
};

/**
 * Obtains whether an entry is descendant of the My Computers directory.
 * @param {!Entry|!FilesAppEntry} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is under My Computers.
 */
util.isComputersEntry = entry => {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] == '' &&
      tree[1] == VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME;
};

/**
 * Returns true if the given entry is the root folder of Trash.
 * @param {!Entry|!FilesAppEntry} entry Entry or a fake entry.
 * @returns {boolean}
 */
util.isTrashRoot = entry => {
  return entry.fullPath === '/' &&
      entry.rootType == VolumeManagerCommon.RootType.TRASH;
};

/**
 * Returns true if the given entry is a descendent of Trash.
 * @param {!Entry|!FilesAppEntry} entry Entry or a fake entry.
 * @returns {boolean}
 */
util.isTrashEntry = entry => {
  return entry.fullPath !== '/' &&
      entry.rootType == VolumeManagerCommon.RootType.TRASH;
};


/**
 * Compares two entries.
 * @param {Entry|FilesAppEntry} entry1 The entry to be compared. Can
 * be a fake.
 * @param {Entry|FilesAppEntry} entry2 The entry to be compared. Can
 * be a fake.
 * @return {boolean} True if the both entry represents a same file or
 *     directory. Returns true if both entries are null.
 */
util.isSameEntry = (entry1, entry2) => {
  if (!entry1 && !entry2) {
    return true;
  }
  if (!entry1 || !entry2) {
    return false;
  }
  return entry1.toURL() === entry2.toURL();
};

/**
 * Compares two entry arrays.
 * @param {Array<!Entry>} entries1 The entry array to be compared.
 * @param {Array<!Entry>} entries2 The entry array to be compared.
 * @return {boolean} True if the both arrays contain same files or directories
 *     in the same order. Returns true if both arrays are null.
 */
util.isSameEntries = (entries1, entries2) => {
  if (!entries1 && !entries2) {
    return true;
  }
  if (!entries1 || !entries2) {
    return false;
  }
  if (entries1.length !== entries2.length) {
    return false;
  }
  for (let i = 0; i < entries1.length; i++) {
    if (!util.isSameEntry(entries1[i], entries2[i])) {
      return false;
    }
  }
  return true;
};

/**
 * Compares two file systems.
 * @param {FileSystem} fileSystem1 The file system to be compared.
 * @param {FileSystem} fileSystem2 The file system to be compared.
 * @return {boolean} True if the both file systems are equal. Also, returns true
 *     if both file systems are null.
 */
util.isSameFileSystem = (fileSystem1, fileSystem2) => {
  if (!fileSystem1 && !fileSystem2) {
    return true;
  }
  if (!fileSystem1 || !fileSystem2) {
    return false;
  }
  return util.isSameEntry(fileSystem1.root, fileSystem2.root);
};

/**
 * Checks if given two entries are in the same directory.
 * @param {!Entry} entry1
 * @param {!Entry} entry2
 * @return {boolean} True if given entries are in the same directory.
 */
util.isSiblingEntry = (entry1, entry2) => {
  const path1 = entry1.fullPath.split('/');
  const path2 = entry2.fullPath.split('/');
  if (path1.length != path2.length) {
    return false;
  }
  for (let i = 0; i < path1.length - 1; i++) {
    if (path1[i] != path2[i]) {
      return false;
    }
  }
  return true;
};

/**
 * Collator for sorting.
 * @type {Intl.Collator}
 */
util.collator =
    new Intl.Collator([], {usage: 'sort', numeric: true, sensitivity: 'base'});

/**
 * Compare by name. The 2 entries must be in same directory.
 * @param {Entry|FilesAppEntry} entry1 First entry.
 * @param {Entry|FilesAppEntry} entry2 Second entry.
 * @return {number} Compare result.
 */
util.compareName = (entry1, entry2) => {
  return util.collator.compare(entry1.name, entry2.name);
};

/**
 * Compare by label (i18n name). The 2 entries must be in same directory.
 * @param {EntryLocation} locationInfo
 * @param {!Entry|!FilesAppEntry} entry1 First entry.
 * @param {!Entry|!FilesAppEntry} entry2 Second entry.
 * @return {number} Compare result.
 */
util.compareLabel = (locationInfo, entry1, entry2) => {
  return util.collator.compare(
      util.getEntryLabel(locationInfo, entry1),
      util.getEntryLabel(locationInfo, entry2));
};

/**
 * Compare by path.
 * @param {Entry|FilesAppEntry} entry1 First entry.
 * @param {Entry|FilesAppEntry} entry2 Second entry.
 * @return {number} Compare result.
 */
util.comparePath = (entry1, entry2) => {
  return util.collator.compare(entry1.fullPath, entry2.fullPath);
};

/**
 * @param {EntryLocation} locationInfo
 * @param {!Array<Entry|FilesAppEntry>} bottomEntries entries that should be
 * grouped in the bottom, used for sorting Linux and Play files entries after
 * other folders in MyFiles.
 * return {function(Entry|FilesAppEntry, Entry|FilesAppEntry) to compare entries
 * by name.
 */
util.compareLabelAndGroupBottomEntries = (locationInfo, bottomEntries) => {
  const childrenMap = new Map();
  bottomEntries.forEach((entry) => {
    childrenMap.set(entry.toURL(), entry);
  });

  /**
   * Compare entries putting entries from |bottomEntries| in the bottom and
   * sort by name within entries that are the same type in regards to
   * |bottomEntries|.
   * @param {Entry|FilesAppEntry} entry1 First entry.
   * @param {Entry|FilesAppEntry} entry2 First entry.
   */
  function compare_(entry1, entry2) {
    // Bottom entry here means Linux or Play files, which should appear after
    // all native entries.
    const isBottomlEntry1 = childrenMap.has(entry1.toURL()) ? 1 : 0;
    const isBottomlEntry2 = childrenMap.has(entry2.toURL()) ? 1 : 0;

    // When there are the same type, just compare by label.
    if (isBottomlEntry1 === isBottomlEntry2) {
      return util.compareLabel(locationInfo, entry1, entry2);
    }

    return isBottomlEntry1 - isBottomlEntry2;
  }

  return compare_;
};

/**
 * Checks if {@code entry} is an immediate child of {@code directory}.
 *
 * @param {Entry} entry The presumptive child.
 * @param {DirectoryEntry|FilesAppEntry} directory The presumptive
 *     parent.
 * @return {!Promise<boolean>} Resolves with true if {@code directory} is
 *     parent of {@code entry}.
 */
util.isChildEntry = (entry, directory) => {
  return new Promise((resolve, reject) => {
    if (!entry || !directory) {
      resolve(false);
    }

    entry.getParent(parent => {
      resolve(util.isSameEntry(parent, directory));
    }, reject);
  });
};

/**
 * Checks if the child entry is a descendant of another entry. If the entries
 * point to the same file or directory, then returns false.
 *
 * @param {!DirectoryEntry|!FilesAppEntry} ancestorEntry The ancestor
 *     directory entry. Can be a fake.
 * @param {!Entry|!FilesAppEntry} childEntry The child entry. Can be a fake.
 * @return {boolean} True if the child entry is contained in the ancestor path.
 */
util.isDescendantEntry = (ancestorEntry, childEntry) => {
  if (!ancestorEntry.isDirectory) {
    return false;
  }

  // For EntryList and VolumeEntry they can contain entries from different
  // files systems, so we should check its getUIChildren.
  const entryList = util.toEntryList(ancestorEntry);
  if (entryList.getUIChildren) {
    // VolumeEntry has to check to root entry descendant entry.
    const nativeEntry = entryList.getNativeEntry();
    if (nativeEntry &&
        util.isSameFileSystem(nativeEntry.filesystem, childEntry.filesystem)) {
      return util.isDescendantEntry(
          /** @type {!DirectoryEntry} */ (nativeEntry), childEntry);
    }

    return entryList.getUIChildren().some(ancestorChild => {
      if (util.isSameEntry(ancestorChild, childEntry)) {
        return true;
      }

      // root entry might not be resolved yet.
      const volumeEntry =
          /** @type {DirectoryEntry} */ (ancestorChild.getNativeEntry());
      return volumeEntry &&
          (util.isSameEntry(volumeEntry, childEntry) ||
           util.isDescendantEntry(volumeEntry, childEntry));
    });
  }

  if (!util.isSameFileSystem(ancestorEntry.filesystem, childEntry.filesystem)) {
    return false;
  }
  if (util.isSameEntry(ancestorEntry, childEntry)) {
    return false;
  }
  if (util.isFakeEntry(ancestorEntry) || util.isFakeEntry(childEntry)) {
    return false;
  }

  // Check if the ancestor's path with trailing slash is a prefix of child's
  // path.
  let ancestorPath = ancestorEntry.fullPath;
  if (ancestorPath.slice(-1) !== '/') {
    ancestorPath += '/';
  }
  return childEntry.fullPath.indexOf(ancestorPath) === 0;
};

/**
 * The last URL with visitURL().
 * @private {string}
 */
let lastVisitedURL;

/**
 * Visit the URL.
 *
 * If the browser is opening, the url is opened in a new tab, otherwise the url
 * is opened in a new window.
 *
 * @param {!string} url URL to visit.
 */
util.visitURL = url => {
  lastVisitedURL = url;
  // openURL opens URLs in the primary browser (ash vs lacros) as opposed to
  // window.open which always opens URLs in ash-chrome.
  chrome.fileManagerPrivate.openURL(url);
};

/**
 * Return the last URL visited with visitURL().
 *
 * @return {string} The last URL visited.
 */
util.getLastVisitedURL = () => {
  return lastVisitedURL;
};

/**
 * Returns normalized current locale, or default locale - 'en'.
 * @return {string} Current locale
 */
util.getCurrentLocaleOrDefault = () => {
  const locale = str('UI_LOCALE') || 'en';
  return locale.replace(/_/g, '-');
};

/**
 * Converts array of entries to an array of corresponding URLs.
 * @param {Array<Entry>} entries Input array of entries.
 * @return {!Array<string>} Output array of URLs.
 */
util.entriesToURLs = entries => {
  return entries.map(entry => {
    // When building file_manager_base.js, cachedUrl is not referred other than
    // here. Thus closure compiler raises an error if we refer the property like
    // entry.cachedUrl.
    return entry['cachedUrl'] || entry.toURL();
  });
};

/**
 * Converts array of URLs to an array of corresponding Entries.
 *
 * @param {Array<string>} urls Input array of URLs.
 * @param {function(!Array<!Entry>, !Array<!URL>)=} opt_callback Completion
 *     callback with array of success Entries and failure URLs.
 * @return {Promise} Promise fulfilled with the object that has entries property
 *     and failureUrls property. The promise is never rejected.
 */
util.URLsToEntries = (urls, opt_callback) => {
  const promises = urls.map(url => {
    return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url))
        .then(
            entry => {
              return {entry: entry};
            },
            failureUrl => {
              // Not an error. Possibly, the file is not accessible anymore.
              console.warn('Failed to resolve the file with url: ' + url + '.');
              return {failureUrl: url};
            });
  });
  const resultPromise = Promise.all(promises).then(results => {
    const entries = [];
    const failureUrls = [];
    for (let i = 0; i < results.length; i++) {
      if ('entry' in results[i]) {
        entries.push(results[i].entry);
      }
      if ('failureUrl' in results[i]) {
        failureUrls.push(results[i].failureUrl);
      }
    }
    return {
      entries: entries,
      failureUrls: failureUrls,
    };
  });

  // Invoke the callback. If opt_callback is specified, resultPromise is still
  // returned and fulfilled with a result.
  if (opt_callback) {
    resultPromise
        .then(result => {
          opt_callback(result.entries, result.failureUrls);
        })
        .catch(error => {
          console.warn(
              'util.URLsToEntries is failed.',
              error.stack ? error.stack : error);
        });
  }

  return resultPromise;
};

/**
 * Converts a url into an {!Entry}, if possible.
 *
 * @param {string} url
 *
 * @return {!Promise<!Entry>} Promise Resolves with the corresponding
 *     {!Entry} if possible, else rejects.
 */
util.urlToEntry = url => {
  return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url));
};

/**
 * Returns whether the window is teleported or not.
 * @param {Window} window Window.
 * @return {Promise<boolean>} Whether the window is teleported or not.
 */
util.isTeleported = window => {
  return new Promise(onFulfilled => {
    window.chrome.fileManagerPrivate.getProfiles(
        (profiles, currentId, displayedId) => {
          onFulfilled(currentId !== displayedId);
        });
  });
};

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 *
 * @param {string} message Test message to send.
 */
util.testSendMessage = message => {
  const test = chrome.test || window.top.chrome.test;
  if (test) {
    test.sendMessage(message);
  }
};

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * util.splitExtension('abc.ext') -> ['abc', '.ext']
 * util.splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * util.splitExtension('a/b') -> ['a/b', '']
 * util.splitExtension('.cshrc') -> ['', '.cshrc']
 * util.splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 *
 * @param {string} path Path to be extracted.
 * @return {Array<string>} Filename and extension of the given path.
 */
util.splitExtension = path => {
  let dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/')) {
    dotPosition = -1;
  }

  const filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  const extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
};

/**
 * Returns the localized name of the root type.
 * @param {!EntryLocation} locationInfo Location info.
 * @return {string} The localized name.
 */
util.getRootTypeLabel = locationInfo => {
  switch (locationInfo.rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return locationInfo.volumeInfo.label;
    case VolumeManagerCommon.RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    case VolumeManagerCommon.RootType.SHARED_DRIVE:
    // |locationInfo| points to either the root directory of an individual Team
    // Drive or sub-directory under it, but not the Shared Drives grand
    // directory. Every Shared Drive and its sub-directories always have
    // individual names (locationInfo.hasFixedLabel is false). So
    // getRootTypeLabel() is used by PathComponent.computeComponentsFromEntry()
    // to display the ancestor name in the breadcrumb like this:
    //   Shared Drives > ABC Shared Drive > Folder1
    //   ^^^^^^^^^^^
    // By this reason, we return the label of the Shared Drives grand root here.
    case VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT:
      return str('DRIVE_SHARED_DRIVES_LABEL');
    case VolumeManagerCommon.RootType.COMPUTER:
    case VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT:
      return str('DRIVE_COMPUTERS_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT:
      return str('DRIVE_DIRECTORY_LABEL');
    case VolumeManagerCommon.RootType.RECENT:
      return str('RECENT_ROOT_LABEL');
    case VolumeManagerCommon.RootType.CROSTINI:
      return str('LINUX_FILES_ROOT_LABEL');
    case VolumeManagerCommon.RootType.MY_FILES:
      return str('MY_FILES_ROOT_LABEL');
    case VolumeManagerCommon.RootType.TRASH:
      return str('TRASH_ROOT_LABEL');
    case VolumeManagerCommon.RootType.MEDIA_VIEW:
      const mediaViewRootType =
          VolumeManagerCommon.getMediaViewRootTypeFromVolumeId(
              locationInfo.volumeInfo.volumeId);
      switch (mediaViewRootType) {
        case VolumeManagerCommon.MediaViewRootType.IMAGES:
          return str('MEDIA_VIEW_IMAGES_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.VIDEOS:
          return str('MEDIA_VIEW_VIDEOS_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.AUDIO:
          return str('MEDIA_VIEW_AUDIO_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.DOCUMENTS:
          return str('MEDIA_VIEW_DOCUMENTS_ROOT_LABEL');
      }
      console.error('Unsupported media view root type: ' + mediaViewRootType);
      return locationInfo.volumeInfo.label;
    case VolumeManagerCommon.RootType.ARCHIVE:
    case VolumeManagerCommon.RootType.REMOVABLE:
    case VolumeManagerCommon.RootType.MTP:
    case VolumeManagerCommon.RootType.PROVIDED:
    case VolumeManagerCommon.RootType.ANDROID_FILES:
    case VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER:
    case VolumeManagerCommon.RootType.SMB:
    case VolumeManagerCommon.RootType.GUEST_OS:
      return locationInfo.volumeInfo.label;
    default:
      console.error('Unsupported root type: ' + locationInfo.rootType);
      return locationInfo.volumeInfo.label;
  }
};

/**
 * Returns the localized/i18n name of the entry.
 *
 * @param {EntryLocation} locationInfo
 * @param {!Entry|!FilesAppEntry} entry The entry to be retrieve the name of.
 * @return {string} The localized name.
 */
util.getEntryLabel = (locationInfo, entry) => {
  if (locationInfo) {
    if (locationInfo.hasFixedLabel) {
      return util.getRootTypeLabel(locationInfo);
    }

    if (entry.filesystem && entry.filesystem.root === entry) {
      return util.getRootTypeLabel(locationInfo);
    }
  }

  // Special case for MyFiles/Downloads, MyFiles/PvmDefault and MyFiles/Camera.
  if (locationInfo &&
      locationInfo.rootType == VolumeManagerCommon.RootType.DOWNLOADS) {
    if (entry.fullPath == '/Downloads') {
      return str('DOWNLOADS_DIRECTORY_LABEL');
    }
    if (entry.fullPath == '/PvmDefault') {
      return str('PLUGIN_VM_DIRECTORY_LABEL');
    }
    if (entry.fullPath == '/Camera') {
      return str('CAMERA_DIRECTORY_LABEL');
    }
  }

  return entry.name;
};

/**
 * Returns true if the given |entry| matches any of the special entries:
 *
 *  - "My Files"/{Downloads,PvmDefault,Camera} directories, or
 *  - "Play Files"/{<any-directory>,DCIM/Camera} directories, or
 *  - "Linux Files" root "/" directory
 *  - "Guest OS" root "/" directory
 *
 * which cannot be modified such as deleted/cut or renamed.
 *
 * @param {!VolumeManager} volumeManager
 * @param {(Entry|FakeEntry)} entry Entry or a fake entry.
 * @return {boolean}
 */
util.isNonModifiable = (volumeManager, entry) => {
  if (!entry) {
    return false;
  }

  if (util.isFakeEntry(entry)) {
    return true;
  }

  if (!volumeManager) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo) {
    return false;
  }

  const volumeType = volumeInfo.volumeType;

  if (volumeType === VolumeManagerCommon.RootType.DOWNLOADS) {
    if (!entry.isDirectory) {
      return false;
    }

    const fullPath = entry.fullPath;

    if (fullPath === '/Downloads') {
      return true;
    }

    if (fullPath === '/PvmDefault' && util.isPluginVmEnabled()) {
      return true;
    }

    if (fullPath === '/Camera') {
      return true;
    }

    return false;
  }

  if (volumeType === VolumeManagerCommon.RootType.ANDROID_FILES) {
    if (!entry.isDirectory) {
      return false;
    }

    const fullPath = entry.fullPath;

    if (fullPath === '/') {
      return true;
    }

    const isRootDirectory = fullPath === ('/' + entry.name);
    if (isRootDirectory) {
      return true;
    }

    if (fullPath === '/DCIM/Camera') {
      return true;
    }

    return false;
  }

  if (volumeType === VolumeManagerCommon.RootType.CROSTINI) {
    return entry.fullPath === '/';
  }

  if (volumeType === VolumeManagerCommon.RootType.GUEST_OS) {
    return entry.fullPath === '/';
  }

  return false;
};

/**
 * Checks if an API call returned an error, and if yes then prints it.
 */
util.checkAPIError = () => {
  if (chrome.runtime.lastError) {
    console.warn(chrome.runtime.lastError.message);
  }
};

/**
 * Makes a promise which will be fulfilled |ms| milliseconds later.
 * @param {number} ms The delay in milliseconds.
 * @return {!Promise}
 */
util.delay = ms => {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
};

/**
 * Makes a promise which will be rejected if the given |promise| is not resolved
 * or rejected for |ms| milliseconds.
 * @param {!Promise} promise A promise which needs to be timed out.
 * @param {number} ms Delay for the timeout in milliseconds.
 * @param {string=} opt_message Error message for the timeout.
 * @return {!Promise} A promise which can be rejected by timeout.
 */
util.timeoutPromise = (promise, ms, opt_message) => {
  return Promise.race([
    promise,
    util.delay(ms).then(() => {
      throw new Error(opt_message || 'Operation timed out.');
    }),
  ]);
};

/**
 * Returns true when copy image to clipboard is enabled.
 * @return {boolean}
 */
util.isCopyImageEnabled = () => {
  return loadTimeData.getBoolean('COPY_IMAGE_ENABLED');
};

/**
 * Whether the Files app integration with DLP (Data Loss Prevention) is enabled.
 * @returns {boolean}
 */
util.isDlpEnabled = () => {
  return loadTimeData.valueExists('DLP_ENABLED') &&
      loadTimeData.getBoolean('DLP_ENABLED');
};

/**
 * Whether the Files app Experimental flag is enabled.
 * @returns {boolean}
 */
util.isFilesAppExperimental = () => {
  return loadTimeData.valueExists('FILES_APP_EXPERIMENTAL') &&
      loadTimeData.getBoolean('FILES_APP_EXPERIMENTAL');
};

/**
 * Returns true if FuseBoxDebug flag is enabled.
 * @return {boolean}
 */
util.isFuseBoxDebugEnabled = () => {
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('FUSEBOX_DEBUG') &&
      loadTimeData.getBoolean('FUSEBOX_DEBUG');
};

/**
 * Returns true if GuestOsFiles flag is enabled.
 * @return {boolean}
 */
util.isGuestOsEnabled = () => {
  return loadTimeData.getBoolean('GUEST_OS');
};

/**
 * Returns true if Jelly flag is enabled.
 * @return {boolean}
 */
util.isJellyEnabled = () => {
  return loadTimeData.getBoolean('JELLY');
};

/**
 * Returns true if DriveFsMirroring flag is enabled.
 * @return {boolean}
 */
util.isMirrorSyncEnabled = () => {
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('DRIVEFS_MIRRORING') &&
      loadTimeData.getBoolean('DRIVEFS_MIRRORING');
};

/**
 * Returns true if filters in Recents view V2 is enabled.
 * @return {boolean}
 */
util.isRecentsFilterV2Enabled = () => {
  return loadTimeData.valueExists('FILTERS_IN_RECENTS_V2_ENABLED') &&
      loadTimeData.getBoolean('FILTERS_IN_RECENTS_V2_ENABLED');
};

/**
 * Returns true if search v2 feature flag is enabled.
 * @return {boolean}
 */
util.isSearchV2Enabled = () => {
  return loadTimeData.getBoolean('FILES_SEARCH_V2');
};

/**
 * Returns true if FilesSinglePartitionFormat flag is enabled.
 * @return {boolean}
 */
util.isSinglePartitionFormatEnabled = () => {
  return loadTimeData.getBoolean('FILES_SINGLE_PARTITION_FORMAT_ENABLED');
};

/**
 * Returns true if FilesTrash feature flag is enabled.
 * @returns {boolean}
 */
util.isTrashEnabled = () => {
  return loadTimeData.valueExists('FILES_TRASH_ENABLED') &&
      loadTimeData.getBoolean('FILES_TRASH_ENABLED');
};

/**
 * Returns true if InlineSyncStatus feature flag is enabled.
 * @returns {boolean}
 */
util.isInlineSyncStatusEnabled = () => {
  return loadTimeData.valueExists('INLINE_SYNC_STATUS') &&
      loadTimeData.getBoolean('INLINE_SYNC_STATUS');
};

/**
 * Retrieves all entries inside the given |rootEntry|.
 * @param {!DirectoryEntry} rootEntry
 * @param {function(!Array<!Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the read is completed.
 * @param {function(DOMError)} errorCallback Called when an error occurs.
 * @param {function():boolean} shouldStop Callback to check if the read process
 *     should stop or not. When this callback is called and it returns true,
 *     the remaining recursive reads will be aborted.
 * @param {number=} opt_maxDepth Max depth to delve directories recursively.
 *     If 0 is specified, only the rootEntry will be read. If -1 is specified
 *     or opt_maxDepth is unspecified, the depth of recursion is unlimited.
 */
util.readEntriesRecursively =
    (rootEntry, entriesCallback, successCallback, errorCallback, shouldStop,
     opt_maxDepth) => {
      let numRunningTasks = 0;
      let error = null;
      const maxDepth = opt_maxDepth === undefined ? -1 : opt_maxDepth;
      const maybeRunCallback = () => {
        if (numRunningTasks === 0) {
          if (shouldStop()) {
            errorCallback(createDOMError(util.FileError.ABORT_ERR));
          } else if (error) {
            errorCallback(error);
          } else {
            successCallback();
          }
        }
      };
      const processEntry = (entry, depth) => {
        const onError = fileError => {
          if (!error) {
            error = fileError;
          }
          numRunningTasks--;
          maybeRunCallback();
        };
        const onSuccess = entries => {
          if (shouldStop() || error || entries.length === 0) {
            numRunningTasks--;
            maybeRunCallback();
            return;
          }
          entriesCallback(entries);
          for (let i = 0; i < entries.length; i++) {
            if (entries[i].isDirectory &&
                (maxDepth === -1 || depth < maxDepth)) {
              processEntry(entries[i], depth + 1);
            }
          }
          // Read remaining entries.
          reader.readEntries(onSuccess, onError);
        };

        numRunningTasks++;
        const reader = entry.createReader();
        reader.readEntries(onSuccess, onError);
      };

      processEntry(rootEntry, 0);
    };

/**
 * Do not remove or modify.  Used in vm.CrostiniFiles tast tests at:
 * https://chromium.googlesource.com/chromiumos/platform/tast-tests
 *
 * Get all entries for the given volume.
 * @param {!VolumeInfo} volumeInfo
 * @return {!Promise<Object<Entry>>} all entries keyed by fullPath.
 */
util.getEntries = volumeInfo => {
  const root = volumeInfo.fileSystem.root;
  return new Promise((resolve, reject) => {
    const allEntries = {'/': root};
    function entriesCallback(someEntries) {
      someEntries.forEach(entry => {
        allEntries[entry.fullPath] = entry;
      });
    }
    function successCallback() {
      resolve(allEntries);
    }
    util.readEntriesRecursively(
        root, entriesCallback, successCallback, reject, () => false);
  });
};

/**
 * Executes a functions only when the context is not the incognito one in a
 * regular session. Returns a promise that when fulfilled informs us whether or
 * not the callback was invoked.
 * @param {function():void} callback
 * @return {!Promise<boolean>}
 */
util.doIfPrimaryContext = async (callback) => {
  const guestMode = await util.isInGuestMode();
  if (guestMode) {
    callback();
    return true;
  }
  return false;
};

/**
 * Casts an Entry to a FilesAppEntry, to access a FilesAppEntry-specific
 * property without Closure compiler complaining.
 * TODO(lucmult): Wrap Entry in a FilesAppEntry derived class and remove
 * this function. https://crbug.com/835203.
 * @param {Entry|FilesAppEntry} entry
 * @return {FilesAppEntry}
 */
util.toFilesAppEntry = entry => {
  return /** @type {FilesAppEntry} */ (entry);
};

/**
 * Casts an Entry to a EntryList, to access a FilesAppEntry-specific
 * property without Closure compiler complaining.
 * @param {Entry|FilesAppEntry} entry
 * @return {EntryList}
 */
util.toEntryList = entry => {
  return /** @type {EntryList} */ (entry);
};

/**
 * Returns true if entry is FileSystemEntry or FileSystemDirectoryEntry, it
 * returns false if it's FakeEntry or any one of the FilesAppEntry types.
 * TODO(lucmult): Wrap Entry in a FilesAppEntry derived class and remove
 * this function. https://crbug.com/835203.
 * @param {Entry|FilesAppEntry} entry
 * @return {boolean}
 */
util.isNativeEntry = entry => {
  entry = util.toFilesAppEntry(entry);
  // Only FilesAppEntry types has |type_name| attribute.
  return entry.type_name === undefined;
};

/**
 * For FilesAppEntry types that wraps a native entry, returns the native entry
 * to be able to send to fileManagerPrivate API.
 * @param {Entry|FilesAppEntry} entry
 * @return {Entry|FilesAppEntry}
 */
util.unwrapEntry = entry => {
  if (!entry) {
    return entry;
  }

  const nativeEntry = entry.getNativeEntry && entry.getNativeEntry();
  if (nativeEntry) {
    return nativeEntry;
  }

  return entry;
};

/** @return {boolean} */
util.isArcUsbStorageUIEnabled = () => {
  return loadTimeData.valueExists('ARC_USB_STORAGE_UI_ENABLED') &&
      loadTimeData.getBoolean('ARC_USB_STORAGE_UI_ENABLED');
};

/** @return {boolean} */
util.isArcVirtioBlkForDataEnabled = () => {
  return loadTimeData.valueExists('ARC_ENABLE_VIRTIO_BLK_FOR_DATA') &&
      loadTimeData.getBoolean('ARC_ENABLE_VIRTIO_BLK_FOR_DATA');
};

/** @return {boolean} */
util.isPluginVmEnabled = () => {
  return loadTimeData.valueExists('PLUGIN_VM_ENABLED') &&
      loadTimeData.getBoolean('PLUGIN_VM_ENABLED');
};

/**
 * Used for logs and debugging. It tries to tell what type is the entry, its
 * path and URL.
 *
 * @param {Entry|FilesAppEntry} entry
 * @return {string}
 */
util.entryDebugString = (entry) => {
  if (entry === null) {
    return 'entry is null';
  }
  if (entry === undefined) {
    return 'entry is undefined';
  }
  let typeName = '';
  if (entry.constructor && entry.constructor.name) {
    typeName = entry.constructor.name;
  } else {
    typeName = Object.prototype.toString.call(entry);
  }
  let entryDescription = '(' + typeName + ') ';
  if (entry.fullPath) {
    entryDescription = entryDescription + entry.fullPath + ' ';
  }
  if (entry.toURL) {
    entryDescription = entryDescription + entry.toURL();
  }
  return entryDescription;
};

/**
 * Returns true if all entries belong to the same volume. If there are no
 * entries it also returns false.
 *
 * @param {!Array<Entry|FilesAppEntry>} entries
 * @param {!VolumeManager} volumeManager
 * @return boolean
 */
util.isSameVolume = (entries, volumeManager) => {
  if (!entries.length) {
    return false;
  }

  const firstEntry = entries[0];
  if (!firstEntry) {
    return false;
  }
  const volumeInfo = volumeManager.getVolumeInfo(firstEntry);

  for (let i = 1; i < entries.length; i++) {
    if (!entries[i]) {
      return false;
    }
    const volumeInfoToCompare = volumeManager.getVolumeInfo(assert(entries[i]));
    if (!volumeInfoToCompare ||
        volumeInfoToCompare.volumeId !== volumeInfo.volumeId) {
      return false;
    }
  }

  return true;
};

/**
 * Returns the Files app modal dialog used to embed any files app dialog
 * that derives from cr.ui.dialogs.
 *
 * @return {!HTMLDialogElement}
 */
util.getFilesAppModalDialogInstance = () => {
  let dialogElement = document.querySelector('#files-app-modal-dialog');

  if (!dialogElement) {  // Lazily create the files app dialog instance.
    dialogElement = document.createElement('dialog');
    dialogElement.id = 'files-app-modal-dialog';
    document.body.appendChild(dialogElement);
  }

  return /** @type {!HTMLDialogElement} */ (dialogElement);
};

util.isDriveDssPinEnabled = () => {
  return loadTimeData.valueExists('DRIVE_DSS_PIN_ENABLED') &&
      loadTimeData.getBoolean('DRIVE_DSS_PIN_ENABLED');
};

/**
 *
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} left
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} right
 * @returns {boolean}
 */
util.descriptorEqual = function(left, right) {
  return left.appId === right.appId && left.taskType === right.taskType &&
      left.actionId === right.actionId;
};

/**
 * Create a taskID which is a string unique-ID for a task. This is temporary
 * and will be removed once we use task.descriptor everywhere instead.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
 * @returns {string}
 */
util.makeTaskID = function({appId, taskType, actionId}) {
  return `${appId}|${taskType}|${actionId}`;
};

/**
 * Returns a new promise which, when fulfilled carries a boolean indicating
 * whether the app is in the guest mode. Typical use:
 *
 * util.isInGuestMode().then(
 *     (guest) => { if (guest) { ... in guest mode } }
 * );
 * @return {Promise<boolean>}
 */
util.isInGuestMode = async () => {
  const profiles = await promisify(chrome.fileManagerPrivate.getProfiles);
  return profiles.length > 0 && profiles[0].profileId === '$guest';
};

/**
 * Get the locale based week start from the load time data.
 * @returns {number}
 */
util.getLocaleBasedWeekStart = () => {
  return loadTimeData.valueExists('WEEK_START_FROM') ?
      loadTimeData.getInteger('WEEK_START_FROM') :
      0;
};

/**
 * Returns a boolean indicating whether the volume is a GuestOs volume. And
 * ANDROID_FILES type volume can also be a GuestOs volume if we are using
 * virtio-blk.
 * @param {VolumeManagerCommon.VolumeType} type
 * @return {boolean}
 */
util.isGuestOs = type => {
  return type === VolumeManagerCommon.VolumeType.GUEST_OS ||
      (type === VolumeManagerCommon.VolumeType.ANDROID_FILES &&
       util.isArcVirtioBlkForDataEnabled());
};

/**
 * A kind of error that represents user electing to cancel an operation. We use
 * this specialization to differentiate between system errors and errors
 * generated through legitimate user actions.
 */
class UserCanceledError extends Error {}

/**
 * Returns whether the given value is null or undefined.
 * @param {*} value
 * @returns {boolean}
 */
util.isNullOrUndefined = (value) => value === null || value === undefined;

export {util, UserCanceledError};
