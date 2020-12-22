// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test implementation of chrome.file[ManagerPrivate|System].
 *
 * These APIs are provided natively to a chrome app, but since we are
 * running as a regular web page, we must provide test implementations.
 */

/** @type {?MockVolumeManager} */
let mockVolumeManager = null;

if (window.test === undefined && window.isSWA) {
  // eslint-disable-next-line
  var test = test || {};

  test.Event = class {
    constructor() {
      this.listeners_ = [];
    }

    /** @param {function()} callback */
    addListener(callback) {
      this.listeners_.push(callback);
    }

    /** @param {function()} callback */
    removeListener(callback) {
      this.listeners_ = this.listeners_.filter(l => l !== callback);
    }

    /** @param {...*} args */
    dispatchEvent(...args) {
      setTimeout(() => {
        for (const listener of this.listeners_) {
          listener(...args);
        }
      }, 0);
    }
  };
} else {
  mockVolumeManager = new MockVolumeManager();
  window.webkitResolveLocalFileSystemURL =
      MockVolumeManager.resolveLocalFileSystemURL.bind(null, mockVolumeManager);
}

/**
 * Suppress compiler warning for overwriting chrome.fileManagerPrivate.
 * @suppress {checkTypes}
 */
chrome.fileManagerPrivate = {
  CrostiniEventType: {
    ENABLE: 'enable',
    DISABLE: 'disable',
    SHARE: 'share',
    UNSHARE: 'unshare',
  },
  FormatFileSystemType: {
    VFAT: 'vfat',
    EXFAT: 'exfat',
    NTFS: 'ntfs',
  },
  DriveConnectionStateType: {
    ONLINE: 'ONLINE',
    OFFLINE: 'OFFLINE',
    METERED: 'METERED',
  },
  DriveOfflineReason: {
    NOT_READY: 'NOT_READY',
    NO_NETWORK: 'NO_NETWORK',
    NO_SERVICE: 'NO_SERVICE',
  },
  InspectionType: {
    NORMAL: 'normal',
    CONSOLE: 'console',
    ELEMENT: 'element',
    BACKGROUND: 'background',
  },
  SearchType: {
    ALL: 'ALL',
    SHARED_WITH_ME: 'SHARED_WITH_ME',
    EXCLUDE_DIRECTORIES: 'EXCLUDE_DIRECTORIES',
    OFFLINE: 'OFFLINE',
  },
  Verb: {
    OPEN_WITH: 'open_with',
    ADD_TO: 'add_to',
    PACK_WITH: 'pack_with',
    SHARE_WITH: 'share_with',
  },
  currentId_: 'test@example.com',
  displayedId_: 'test@example.com',
  preferences_: {
    cellularDisabled: true,
    driveEnabled: true,
    searchSuggestEnabled: true,
    timezone: 'Australia/Sydney',
    use24hourClock: false,
    arcEnabled: false,
    arcRemovableMediaAccessEnabled: false,
  },
  profiles_: [{
    displayName: 'Test User',
    isCurrentProfile: true,
    profileId: 'test@example.com'
  }],
  token_: 'token',
  SourceRestriction: {
    ANY_SOURCE: 'any_source',
    NATIVE_SOURCE: 'native_source',
  },
  RecentFileType: {
    ALL: 'all',
    AUDIO: 'audio',
    IMAGE: 'image',
    VIDEO: 'video',
  },
  addFileWatch: (entry, callback) => {
    // Returns success.
    setTimeout(callback, 0, true);
  },
  enableExternalFileScheme: () => {},
  executeTask: (taskId, entries, callback) => {
    // Returns opened|message_sent|failed|empty.
    setTimeout(callback, 0, 'failed');
  },
  getContentMetadata: (entry, mimeType, includeImages, callback) => {
    setTimeout(callback, 0);
  },
  getContentMimeType: (entry, callback) => {
    setTimeout(callback, 0, '');
  },
  getDriveConnectionState: (callback) => {
    setTimeout(callback, 0, mockVolumeManager.getDriveConnectionState());
  },
  getEntryProperties: (entries, names, callback) => {
    // Returns chrome.fileManagerPrivate.EntryProperties[].
    const results = [];
    entries.forEach(entry => {
      const props = {};
      names.forEach(name => {
        props[name] = entry.metadata[name];
      });
      results.push(props);
    });
    setTimeout(callback, 0, results);
  },
  getFileTasks: (entries, callback) => {
    // Returns chrome.fileManagerPrivate.FileTask[].
    const results = [];
    // Support for view-in-browser on single text file used by QuickView.
    if (entries.length == 1 && entries[0].metadata &&
        entries[0].metadata.contentMimeType == 'text/plain') {
      results.push({
        taskId: 'hhaomjibdihmijegdhdafkllkbggdgoj|file|view-in-browser',
        title: '__MSG_OPEN_ACTION__',
        isDefault: true,
      });
    }
    setTimeout(callback, 0, results);
  },
  getCrostiniSharedPaths: (observeFirstForSession, vmName, callback) => {
    // Returns Entry[], firstForSession.
    setTimeout(callback, 0, [], observeFirstForSession);
  },
  getLinuxPackageInfo: (entry, callback) => {
    // Returns chrome.fileManagerPrivate.LinuxPackageInfo.
    setTimeout(callback, 0, {
      name: 'dummy-package',
      version: '1.0',
    });
  },
  getPreferences: (callback) => {
    setTimeout(callback, 0, chrome.fileManagerPrivate.preferences_);
  },
  getProfiles: (callback) => {
    // Returns profiles, currentId, displayedId
    setTimeout(
        callback, 0, chrome.fileManagerPrivate.profiles_,
        chrome.fileManagerPrivate.currentId_,
        chrome.fileManagerPrivate.displayedId_);
  },
  getProviders: (callback) => {
    // Returns chrome.fileManagerPrivate.Provider[].
    setTimeout(callback, 0, []);
  },
  getRecentFiles: (restriction, fileType, callback) => {
    // Returns Entry[].
    setTimeout(callback, 0, []);
  },
  getSizeStats: (volumeId, callback) => {
    // chrome.fileManagerPrivate.MountPointSizeStats { totalSize: double,
    // remainingSize: double }
    setTimeout(callback, 0, {totalSize: 16e9, remainingSize: 8e9});
  },
  getStrings: (callback) => {
    // Returns map of strings.
    setTimeout(callback, 0, loadTimeData.data_);
  },
  getVolumeMetadataList: (callback) => {
    const list = [];
    for (let i = 0; i < mockVolumeManager.volumeInfoList.length; i++) {
      list.push(mockVolumeManager.volumeInfoList.item(i));
    }
    setTimeout(callback, 0, list);
  },
  grantAccess: (entryUrls, callback) => {
    setTimeout(callback, 0);
  },
  importCrostiniImage: (entry) => {},
  // Simulate startup of vm and container by taking 1s.
  mountCrostiniDelay_: 1000,
  mountCrostini: (callback) => {
    setTimeout(() => {
      test.mountCrostini();
      callback();
    }, chrome.fileManagerPrivate.mountCrostiniDelay_);
  },
  onAppsUpdated: new test.Event(),
  onCopyProgress: new test.Event(),
  onCrostiniChanged: new test.Event(),
  onDeviceChanged: new test.Event(),
  onDirectoryChanged: new test.Event(),
  onDriveConnectionStatusChanged: new test.Event(),
  onDriveSyncError: new test.Event(),
  onFileTransfersUpdated: new test.Event(),
  onPinTransfersUpdated: new test.Event(),
  onMountCompleted: new test.Event(),
  onPreferencesChanged: new test.Event(),
  onTabletModeChanged: new test.Event(),
  openInspector: (type) => {},
  openSettingsSubpage: (sub_page) => {},
  removeFileWatch: (entry, callback) => {
    setTimeout(callback, 0, true);
  },
  removeMount(volumeId) {
    chrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
      status: 'success',
      eventType: 'unmount',
      volumeMetadata: {
        volumeId: volumeId,
      },
    });
  },
  requestWebStoreAccessToken: (callback) => {
    setTimeout(callback, 0, chrome.fileManagerPrivate.token_);
  },
  resolveIsolatedEntries: (entries, callback) => {
    setTimeout(callback, 0, entries);
  },
  searchDriveMetadata: (searchParams, callback) => {
    // Returns chrome.fileManagerPrivate.DriveMetadataSearchResult[].
    // chrome.fileManagerPrivate.DriveMetadataSearchResult { entry: Entry,
    // highlightedBaseName: string }
    setTimeout(callback, 0, []);
  },
  sharePathsWithCrostini: (vmName, entries, persist, callback) => {
    setTimeout(callback, 0);
  },
  unsharePathWithCrostini: (vmName, entry, callback) => {
    setTimeout(callback, 0);
  },
  nextCopyId_: 0,
  startCopy: (entry, parentEntry, newName, callback) => {
    // Returns copyId immediately.
    const copyId = chrome.fileManagerPrivate.nextCopyId_++;
    callback(copyId);
    chrome.fileManagerPrivate.onCopyProgress.listeners_.forEach(l => {
      l(copyId, {type: 'begin_copy_entry', sourceUrl: entry.toURL()});
    });
    entry.copyTo(
        parentEntry, newName,
        // Success.
        (copied) => {
          chrome.fileManagerPrivate.onCopyProgress.listeners_.forEach(l => {
            l(copyId, {
              type: 'end_copy_entry',
              sourceUrl: entry.toURL(),
              destinationUrl: copied.toURL()
            });
          });
          chrome.fileManagerPrivate.onCopyProgress.listeners_.forEach(l => {
            l(copyId, {
              type: 'success',
              sourceUrl: entry.toURL(),
              destinationUrl: copied.toURL()
            });
          });
        },
        // Error.
        (error) => {
          chrome.fileManagerPrivate.onCopyProgress.listeners_.forEach(l => {
            l(copyId, {type: 'error', error: error});
          });
        });
  },
  validatePathNameLength: (parentEntry, name, callback) => {
    setTimeout(callback, 0, true);
  },
  isTabletModeEnabled: (callback) => {
    setTimeout(callback, 0, false);
  },
};

/**
 * Suppress compiler warning for overwriting chrome.fileSystem.
 * @suppress {checkTypes}
 */
chrome.fileSystem = {
  requestFileSystem: (options, callback) => {
    let fs = null;
    for (let i = 0; i < mockVolumeManager.volumeInfoList.length; i++) {
      const volume = mockVolumeManager.volumeInfoList.item(i);
      if (volume.volumeId === options.volumeId) {
        fs = volume.fileSystem;
        break;
      }
    }
    setTimeout(callback, 0, fs);
  },
};
