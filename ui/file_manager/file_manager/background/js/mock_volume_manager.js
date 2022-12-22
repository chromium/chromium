// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {EntryLocationImpl} from './entry_location_impl.js';
import {VolumeInfoImpl} from './volume_info_impl.js';
import {VolumeInfoListImpl} from './volume_info_list_impl.js';
import {volumeManagerFactory} from './volume_manager_factory.js';
import {VolumeManagerImpl} from './volume_manager_impl.js';

/**
 * Mock class for VolumeManager.
 * @final
 * @implements {VolumeManager}
 */
export class MockVolumeManager {
  constructor() {
    /** @const {!VolumeInfoList} */
    this.volumeInfoList = new VolumeInfoListImpl();

    /** @type {!chrome.fileManagerPrivate.DriveConnectionState} */
    this.driveConnectionState = {
      type: /** @type {!chrome.fileManagerPrivate.DriveConnectionStateType} */ (
          'ONLINE'),
      hasCellularNetworkAccess: false,
      canPinHostedFiles: false,
    };

    // Create Drive.   Drive attempts to resolve FilesSystemURLs for '/root',
    // '/team_drives' and '/Computers' during initialization. Create a
    // filesystem with those entries now, and mock
    // webkitResolveLocalFileSystemURL.
    const driveFs = new MockFileSystem(
        VolumeManagerCommon.VolumeType.DRIVE,
        'filesystem:' + VolumeManagerCommon.VolumeType.DRIVE);
    driveFs.populate(['/root/', '/team_drives/', '/Computers/']);

    // Mock window.webkitResolve to return entries.
    const orig = window.webkitResolveLocalFileSystemURL;
    window.webkitResolveLocalFileSystemURL = (url, success) => {
      const match = url.match(/^filesystem:drive(\/.*)/);
      if (match) {
        const path = match[1];
        const entry = driveFs.entries[path];
        if (entry) {
          return setTimeout(success, 0, entry);
        }
      }
      throw new DOMException('Unknown drive url: ' + url, 'NotFoundError');
    };

    // Create Drive, swap entries back in, revert window.webkitResolve.
    const drive = this.createVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE,
        VolumeManagerCommon.RootType.DRIVE, str('DRIVE_DIRECTORY_LABEL'));
    /** @type {MockFileSystem} */ (drive.fileSystem)
        .populate(Object.values(driveFs.entries));
    window.webkitResolveLocalFileSystemURL = orig;

    // Create Downloads.
    this.createVolumeInfo(
        VolumeManagerCommon.VolumeType.DOWNLOADS,
        VolumeManagerCommon.RootType.DOWNLOADS,
        str('DOWNLOADS_DIRECTORY_LABEL'));
  }

  getFuseBoxOnlyFilterEnabled() {
    return false;
  }

  getMediaStoreFilesOnlyFilterEnabled() {
    return false;
  }

  dispose() {}

  /**
   * Replaces the VolumeManager singleton with a MockVolumeManager.
   * @param {!MockVolumeManager=} opt_singleton
   */
  static installMockSingleton(opt_singleton) {
    MockVolumeManager.instance_ = opt_singleton || new MockVolumeManager();

    volumeManagerFactory.getInstance = () => {
      return Promise.resolve(MockVolumeManager.instance_);
    };
  }

  /**
   * Creates, installs and returns a mock VolumeInfo instance.
   *
   * @param {!VolumeManagerCommon.VolumeType} type
   * @param {string} volumeId
   * @param {string} label
   * @param {string=} providerId
   * @param {string=} remoteMountPath
   *
   * @return {!VolumeInfo}
   */
  createVolumeInfo(type, volumeId, label, providerId, remoteMountPath) {
    const volumeInfo = MockVolumeManager.createMockVolumeInfo(
        type, volumeId, label, undefined, providerId, remoteMountPath);
    this.volumeInfoList.add(volumeInfo);
    return volumeInfo;
  }

  /**
   * Obtains location information from an entry.
   * Current implementation can handle only fake entries.
   *
   * @param {!Entry|!FilesAppEntry} entry A fake entry.
   * @return {!EntryLocation} Location information.
   */
  getLocationInfo(entry) {
    if (util.isFakeEntry(entry)) {
      const isReadOnly =
          entry.rootType !== VolumeManagerCommon.RootType.RECENT &&
          entry.rootType !== VolumeManagerCommon.RootType.TRASH;
      return new EntryLocationImpl(
          this.volumeInfoList.item(0),
          /** @type {!FakeEntry} */ (entry).rootType, /* isRootType= */ true,
          isReadOnly);
    }

    if (entry.filesystem.name === VolumeManagerCommon.VolumeType.DRIVE) {
      const volumeInfo = this.volumeInfoList.item(0);
      let rootType = VolumeManagerCommon.RootType.DRIVE;
      let isRootEntry = entry.fullPath === '/root';
      if (entry.fullPath.startsWith('/team_drives')) {
        if (entry.fullPath === '/team_drives') {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
          isRootEntry = util.isTeamDriveRoot(entry);
        }
      } else if (entry.fullPath.startsWith('/Computers')) {
        if (entry.fullPath === '/Computers') {
          rootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.COMPUTER;
          isRootEntry = util.isComputersRoot(entry);
        }
      } else if (/^\/\.(files|shortcut-targets)-by-id/.test(entry.fullPath)) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;
      }
      return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, true);
    }

    const volumeInfo = this.getVolumeInfo(entry);
    const rootType = VolumeManagerCommon.getRootTypeFromVolumeType(
        assert(volumeInfo.volumeType));
    const isRootEntry = util.isSameEntry(entry, volumeInfo.fileSystem.root);
    return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, false);
  }

  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {?VolumeInfo} Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType) {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.profile.isCurrentProfile &&
          volumeInfo.volumeType === volumeType) {
        return volumeInfo;
      }
    }
    return null;
  }

  /**
   * @return {!chrome.fileManagerPrivate.DriveConnectionState} Current drive
   *     connection state.
   */
  getDriveConnectionState() {
    return this.driveConnectionState;
  }

  /**
   * Utility function to create a mock VolumeInfo.
   * @param {!VolumeManagerCommon.VolumeType} type Volume type.
   * @param {string} volumeId Volume id.
   * @param {string=} label Label.
   * @param {string=} devicePath Device path.
   * @param {string=} providerId Provider id.
   * @param {string=} remoteMountPath Remote mount path.
   * @return {!VolumeInfo} Created mock VolumeInfo.
   */
  static createMockVolumeInfo(
      type, volumeId, label, devicePath, providerId, remoteMountPath) {
    const fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);

    let diskFileSystemType = VolumeManagerCommon.FileSystemType.UNKNOWN;
    if (devicePath && devicePath.startsWith('fusebox')) {
      diskFileSystemType =
          /** @type VolumeManagerCommon.FileSystemType */ ('fusebox');
    }

    // If there's no label set it to volumeId to make it shorter to write
    // tests.
    const volumeInfo = new VolumeInfoImpl(
        type,
        volumeId,
        fileSystem,
        '',                                         // error
        '',                                         // deviceType
        devicePath || '',                           // devicePath
        false,                                      // isReadOnly
        false,                                      // isReadOnlyRemovableDevice
        {isCurrentProfile: true, displayName: ''},  // profile
        label || volumeId,                          // label
        providerId,                                 // providerId
        false,                                      // hasMedia
        false,                                      // configurable
        false,                                      // watchable
        VolumeManagerCommon.Source.NETWORK,         // source
        diskFileSystemType,                         // diskFileSystemType
        {},                                         // iconSet
        '',                                         // driveLabel
        remoteMountPath,                            // remoteMountPath
        undefined,                                  // vmType
    );


    return volumeInfo;
  }

  /**
   * @return {!Promise<!VolumeInfo>}
   */
  async mountArchive(fileUrl, password) {
    throw new Error('Not implemented');
  }

  async cancelMounting(fileUrl) {
    throw new Error('Not implemented');
  }

  async unmount(volumeInfo) {
    throw new Error('Not implemented');
  }

  async configure(volumeInfo) {
    throw new Error('Not implemented');
  }

  addEventListener(type, handler) {
    throw new Error('Not implemented');
  }

  removeEventListener(type, handler) {
    throw new Error('Not implemented');
  }

  /**
   * @return {boolean}
   */
  dispatchEvent(event) {
    throw new Error('Not implemented');
  }

  /**
   * @return {boolean}
   */
  hasDisabledVolumes() {
    return false;
  }

  /**
   * @return {boolean}
   */
  isDisabled(volume) {
    return false;
  }
}

/** @private {?VolumeManager} */
MockVolumeManager.instance_ = null;

MockVolumeManager.prototype.getVolumeInfo =
    VolumeManagerImpl.prototype.getVolumeInfo;

MockVolumeManager.prototype.getDefaultDisplayRoot =
    VolumeManagerImpl.prototype.getDefaultDisplayRoot;

MockVolumeManager.prototype.findByDevicePath =
    VolumeManagerImpl.prototype.findByDevicePath;

MockVolumeManager.prototype.whenVolumeInfoReady =
    VolumeManagerImpl.prototype.whenVolumeInfoReady;

/**
 * Used to override window.webkitResolveLocalFileSystemURL for testing. This
 * emulates the real function by parsing `url` and finding the matching entry
 * in `volumeManager`. E.g. filesystem:downloads/dir/file.txt will look up the
 * 'downloads' volume for /dir/file.txt.
 *
 * @param {VolumeManager} volumeManager VolumeManager to resolve URLs with.
 * @param {string} url URL to resolve.
 * @param {function(!MockEntry)} successCallback Success callback.
 * @param {function(!FileError)=} errorCallback Error callback.
 */
MockVolumeManager.resolveLocalFileSystemURL =
    (volumeManager, url, successCallback, errorCallback) => {
      const match = url.match(/^filesystem:(\w+)(\/.*)/);
      if (match) {
        const volumeType =
            /** @type {VolumeManagerCommon.VolumeType} */ (match[1]);
        let path = match[2];
        const volume = volumeManager.getCurrentProfileVolumeInfo(volumeType);
        if (volume) {
          // Decode URI in file paths.
          path = path.split('/').map(decodeURIComponent).join('/');
          const entry = volume.fileSystem.entries[path];
          if (entry) {
            setTimeout(successCallback, 0, entry);
            return;
          }
        }
      }
      const message =
          `MockVolumeManager.resolveLocalFileSystemURL not found: ${url}`;
      console.warn(message);
      const error = new DOMException(message, 'NotFoundError');
      if (errorCallback) {
        setTimeout(errorCallback, 0, error);
      } else {
        throw error;
      }
    };
