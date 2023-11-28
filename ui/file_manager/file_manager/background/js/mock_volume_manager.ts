// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {isComputersRoot, isFakeEntry, isSameEntry, isTeamDriveRoot} from '../../common/js/entry_utils.js';
import {MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {str} from '../../common/js/translations.js';
import {FileSystemType, getRootTypeFromVolumeType, RootType, Source, VolumeType} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';
import type {VolumeManager} from '../../externs/volume_manager.js';

import {EntryLocationImpl} from './entry_location_impl.js';
import {VolumeInfoImpl} from './volume_info_impl.js';
import {VolumeInfoListImpl} from './volume_info_list_impl.js';
import {volumeManagerFactory} from './volume_manager_factory.js';
import {VolumeManagerImpl} from './volume_manager_impl.js';

export const fakeMyFilesVolumeId = VolumeType.DOWNLOADS + ':test_mount_path';
export const fakeDriveVolumeId = VolumeType.DRIVE + ':test_mount_path';

let volumeManagerInstance: VolumeManager|null = null;

/**
 * Mock class for VolumeManager.
 */
export class MockVolumeManager implements VolumeManager {
  volumeInfoList: VolumeInfoList = new VolumeInfoListImpl();
  driveConnectionState: chrome.fileManagerPrivate.DriveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
  };

  constructor() {
    // Create Drive.   Drive attempts to resolve FilesSystemURLs for '/root',
    // '/team_drives' and '/Computers' during initialization. Create a
    // filesystem with those entries now, and mock
    // webkitResolveLocalFileSystemURL.
    const driveFs = new MockFileSystem(
        fakeDriveVolumeId, 'filesystem:' + fakeDriveVolumeId);
    driveFs.populate(['/root/', '/team_drives/', '/Computers/']);

    // Mock window.webkitResolve to return entries.
    const orig = window.webkitResolveLocalFileSystemURL;
    window.webkitResolveLocalFileSystemURL = (url, success) => {
      const rootURL = `filesystem:${fakeDriveVolumeId}`;
      const match = url.match(new RegExp(`^${rootURL}(\/.*)`));
      if (match) {
        const path = match[1]!;
        const entry = driveFs.entries[path];
        if (entry) {
          return setTimeout(success, 0, entry);
        }
      }
      throw new DOMException('Unknown drive url: ' + url, 'NotFoundError');
    };

    // Create Drive, swap entries back in, revert window.webkitResolve.
    const drive = this.createVolumeInfo(
        VolumeType.DRIVE, fakeDriveVolumeId, str('DRIVE_DIRECTORY_LABEL'));
    (drive.fileSystem as MockFileSystem)
        .populate(Object.values(driveFs.entries));
    window.webkitResolveLocalFileSystemURL = orig;

    // Create Downloads.
    this.createVolumeInfo(
        VolumeType.DOWNLOADS, fakeMyFilesVolumeId,
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
   */
  static installMockSingleton(singleton?: MockVolumeManager) {
    volumeManagerInstance = singleton || new MockVolumeManager();

    volumeManagerFactory.getInstance = async () => {
      assert(volumeManagerInstance);
      return volumeManagerInstance;
    };
  }

  /**
   * Creates, installs and returns a mock VolumeInfo instance.
   */
  createVolumeInfo(
      type: VolumeType, volumeId: string, label: string, providerId?: string,
      remoteMountPath?: string): VolumeInfo {
    const volumeInfo = MockVolumeManager.createMockVolumeInfo(
        type, volumeId, label, undefined, providerId, remoteMountPath);
    this.volumeInfoList.add(volumeInfo);
    return volumeInfo;
  }

  /**
   * Obtains location information from an entry.
   * Current implementation can handle only fake entries.
   *
   * @param entry A fake entry.
   * @return Location information.
   */
  getLocationInfo(entry: Entry|FilesAppEntry): EntryLocation|null {
    if (isFakeEntry(entry)) {
      const isReadOnly = entry.rootType !== RootType.RECENT &&
          entry.rootType !== RootType.TRASH;
      return new EntryLocationImpl(
          this.volumeInfoList.item(0), entry.rootType, /* isRootType= */ true,
          isReadOnly);
    }

    if (entry.filesystem?.name === fakeDriveVolumeId) {
      const volumeInfo = this.volumeInfoList.item(0);
      let rootType = RootType.DRIVE;
      let isRootEntry = entry.fullPath === '/root';
      if (entry.fullPath.startsWith('/team_drives')) {
        if (entry.fullPath === '/team_drives') {
          rootType = RootType.SHARED_DRIVES_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = RootType.SHARED_DRIVE;
          isRootEntry = isTeamDriveRoot(entry);
        }
      } else if (entry.fullPath.startsWith('/Computers')) {
        if (entry.fullPath === '/Computers') {
          rootType = RootType.COMPUTERS_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = RootType.COMPUTER;
          isRootEntry = isComputersRoot(entry);
        }
      } else if (/^\/\.(files|shortcut-targets)-by-id/.test(entry.fullPath)) {
        rootType = RootType.DRIVE_SHARED_WITH_ME;
      }
      return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, true);
    }

    const volumeInfo = this.getVolumeInfo(entry);
    // For filtered out volumes, its volume info won't exist in the volume info
    // list.
    if (!volumeInfo) {
      return null;
    }
    assert(volumeInfo.volumeType);
    const rootType = getRootTypeFromVolumeType(volumeInfo.volumeType);
    const isRootEntry = isSameEntry(entry, volumeInfo.fileSystem.root);
    return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, false);
  }

  /**
   * @param volumeType Volume type.
   * @return Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType: VolumeType): null|VolumeInfo {
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
   * @return Current drive connection state.
   */
  getDriveConnectionState(): chrome.fileManagerPrivate.DriveConnectionState {
    return this.driveConnectionState;
  }

  /**
   * Utility function to create a mock VolumeInfo.
   * @param type Volume type.
   * @param volumeId Volume id.
   * @param label Label.
   * @param devicePath Device path.
   * @param providerId Provider id.
   * @param remoteMountPath Remote mount path.
   * @return Created mock
   *     VolumeInfo.
   */
  static createMockVolumeInfo(
      type: VolumeType, volumeId: string, label?: string, devicePath?: string,
      providerId?: string, remoteMountPath?: string): VolumeInfo {
    const fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);

    let diskFileSystemType = FileSystemType.UNKNOWN;
    if (devicePath && devicePath.startsWith('fusebox')) {
      diskFileSystemType = 'fusebox' as FileSystemType;
    }

    let source = Source.NETWORK;
    if (type === VolumeType.ARCHIVE) {
      source = Source.FILE;
    } else if (type === VolumeType.REMOVABLE) {
      source = Source.DEVICE;
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
        source,                                     // source
        diskFileSystemType,                         // diskFileSystemType
        {icon16x16Url: '', icon32x32Url: ''},       // iconSet
        '',                                         // driveLabel
        remoteMountPath,                            // remoteMountPath
        undefined,                                  // vmType
    );


    return volumeInfo;
  }

  async mountArchive(_fileUrl: string, _password?: string):
      Promise<VolumeInfo> {
    throw new Error('Not implemented');
  }

  async cancelMounting(_fileUrl: string): Promise<void> {
    throw new Error('Not implemented');
  }

  async unmount(_volumeInfo: VolumeInfo): Promise<void> {
    throw new Error('Not implemented');
  }

  async configure(_volumeInfo: VolumeInfo): Promise<void> {
    throw new Error('Not implemented');
  }

  addEventListener(_type: string, _handler: (arg0: Event) => void) {
    throw new Error('Not implemented');
  }

  removeEventListener(_type: string, _handler: (arg0: Event) => void) {
    throw new Error('Not implemented');
  }

  dispatchEvent(_event: Event): boolean {
    throw new Error('Not implemented');
  }

  hasDisabledVolumes(): boolean {
    return false;
  }

  isDisabled(_volume: VolumeType): boolean {
    return false;
  }


  isAllowedVolume(_volumeInfo: VolumeInfo): boolean {
    return true;
  }

  getVolumeInfo(entry: Entry|FilesAppEntry): VolumeInfo|null {
    return VolumeManagerImpl.prototype.getVolumeInfo.call(this, entry);
  }

  getDefaultDisplayRoot(
      callback: (arg0: DirectoryEntry|FilesAppDirEntry|null) => void) {
    VolumeManagerImpl.prototype.getDefaultDisplayRoot.call(this, callback);
  }

  findByDevicePath(devicePath: string): VolumeInfo|null {
    return VolumeManagerImpl.prototype.findByDevicePath.call(this, devicePath);
  }

  async whenVolumeInfoReady(volumeId: string): Promise<VolumeInfo> {
    return VolumeManagerImpl.prototype.whenVolumeInfoReady.call(this, volumeId);
  }

  /**
   * Used to window.webkitResolveLocalFileSystemURL for testing. This
   * emulates the real function by parsing `url` and finding the matching entry
   * in `volumeManager`. E.g. filesystem:downloads:test_mount_path/dir/file.txt
   * will look up the volume with ID 'downloads:test_mount_path' for
   * /dir/file.txt.
   *
   * @param volumeManager VolumeManager to resolve URLs with.
   * @param url URL to resolve.
   * @param successCallback Success callback.
   * @param errorCallback Error callback.
   */
  static resolveLocalFileSystemUrl(
      volumeManager: VolumeManager, url: string,
      successCallback: (arg0: MockEntry) => void,
      errorCallback?: (arg0: FileError) => void) {
    const match = url.match(/^filesystem:(\w+):\w+(\/.*)/);
    if (match) {
      const volumeType = match[1]! as VolumeType;
      let path = match[2]!;
      const volume = volumeManager.getCurrentProfileVolumeInfo(volumeType);
      if (volume) {
        // Decode URI in file paths.
        path = path.split('/').map(decodeURIComponent).join('/');
        const entry = (volume.fileSystem as MockFileSystem).entries[path];
        if (entry) {
          setTimeout(successCallback, 0, entry);
          return;
        }
      }
    }
    const message =
        `MockVolumeManager.resolveLocalFileSystemUrl not found: ${url}`;
    console.warn(message);
    const error = new DOMException(message, 'NotFoundError');
    if (errorCallback) {
      setTimeout(errorCallback, 0, error);
    } else {
      throw error;
    }
  }
}
