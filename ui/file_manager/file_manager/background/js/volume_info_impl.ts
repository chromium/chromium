// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';

// To avoid the import being elided, closure requires this name here because of
// the @implements.
export const _unused = VolumeInfo;

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 * @implements {VolumeInfo}
 */
export class VolumeInfoImpl implements VolumeInfo {
  private displayRoot_: DirectoryEntry|null = null;
  private sharedDriveDisplayRoot_: DirectoryEntry|null = null;
  private computersDisplayRoot_: DirectoryEntry|null = null;
  /**
   * An entry to be used as prefix of this volume on breadcrumbs, e.g. "My Files
   * > Downloads", "My Files" is a prefixEntry on "Downloads" VolumeInfo.
   */
  private prefixEntry_: FilesAppEntry|null = null;
  private fakeEntries_: Record<VolumeManagerCommon.RootType, FakeEntry> = {};
  private displayRootPromise_: Promise<DirectoryEntry>;
  /**
   * `volumeType` is the type of the volume.
   * `volumeId` is the ID of the volume.
   * `fileSystem` is the file system object for this volume.
   * `error` is the error if an error is found.
   * `deviceType` is the type of device
   *     ('usb'|'sd'|'optical'|'mobile'|'unknown') (as defined in
   *     chromeos/ash/components/disks/disk_mount_manager.cc). Can be undefined.
   * `devicePath` is the dentifier of the device that the
   *     volume belongs to. Can be undefined.
   * `isReadOnly` is true if the volume is read only.
   * `isReadOnlyRemovableDevice` is true if the volume is read only
   *     removable device.
   * `profile` is the profile information.
   * `label` is the abel of the volume.
   * `providerId` is the Id of the provider for this volume.
   *     Undefined for non-FSP volumes.
   * `hasMedia` is true when the volume has been identified
   *     as containing media such as photos or videos.
   * `configurable` is true when the volume can be configured.
   * `watchable` is true when the volume can be watched.
   * `source` is the source of the volume's data.
   * `diskFileSystemType` is the file system type identifier.
   * `iconSet` is the set of icons for this volume.
   * `driveLabel` is the drive label of the volume. Removable
   *     partitions belonging to the same device will share the same drive
   *     label.
   * `remoteMountPath` is the path on the remote host
   *     where this volume is mounted, for crostini this is the user's homedir
   *     (/home/<username>).
   * `vmType` is the type of the VM which owns the volume if this is a
   *     GuestOS volume.
   */
  constructor(
      private volumeType_: VolumeManagerCommon.VolumeType,
      private volumeId_: string, private fileSystem_: FileSystem,
      // Note: This represents if the mounting of the volume is successfully
      // done or not. (If error is empty string, the mount is successfully
      // done).
      // TODO(hidehiko): Rename to make this more understandable.
      private error_: (string|undefined),
      private deviceType_: (string|undefined),
      private devicePath_: (string|undefined), private isReadOnly_: boolean,
      private isReadOnlyRemovableDevice_: boolean,
      private profile_: {displayName: string, isCurrentProfile: boolean},
      private label_: string, private providerId_: (string|undefined),
      private hasMedia_: boolean, private configurable_: boolean,
      private watchable_: boolean, private source_: VolumeManagerCommon.Source,
      private diskFileSystemType_: VolumeManagerCommon.FileSystemType,
      private iconSet_: chrome.fileManagerPrivate.IconSet,
      private driveLabel_: (string|undefined),
      private remoteMountPath_: (string|undefined),
      private vmType_: (chrome.fileManagerPrivate.VmType|undefined)) {
    this.displayRoot_ = null;
    this.sharedDriveDisplayRoot_ = null;
    this.computersDisplayRoot_ = null;

    this.prefixEntry_ = null;

    this.fakeEntries_ = {};

    if (volumeType_ === VolumeManagerCommon.VolumeType.DRIVE) {
      if (!util.isDriveFsBulkPinningEnabled()) {
        this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_OFFLINE] =
            new FakeEntryImpl(
                str('DRIVE_OFFLINE_COLLECTION_LABEL'),
                VolumeManagerCommon.RootType.DRIVE_OFFLINE);
      }

      this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME] =
          new FakeEntryImpl(
              str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
              VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME);
    }

    this.displayRootPromise_ = this.resolveDisplayRootImpl_();
  }

  get volumeType(): VolumeManagerCommon.VolumeType {
    return this.volumeType_;
  }

  get volumeId(): string {
    return this.volumeId_;
  }

  get fileSystem(): FileSystem {
    return this.fileSystem_;
  }

  /** Display root path. It is null before finishing to resolve the entry. */
  get displayRoot(): DirectoryEntry {
    return this.displayRoot_!;
  }

  /**
   * The display root path of Shared Drives directory. It is null before
   * finishing to resolve the entry. Valid only for Drive volume.
   */
  get sharedDriveDisplayRoot(): DirectoryEntry {
    return this.sharedDriveDisplayRoot_!;
  }

  /**
   * The display root path of Computers directory. It is null before finishing
   * to resolve the entry. Valid only for Drive volume.
   */
  get computersDisplayRoot(): DirectoryEntry {
    return this.computersDisplayRoot_!;
  }

  get fakeEntries(): Record<VolumeManagerCommon.RootType, FakeEntry> {
    return this.fakeEntries_;
  }

  get error(): string|undefined {
    return this.error_;
  }

  get deviceType(): string|undefined {
    return this.deviceType_;
  }

  get devicePath(): string|undefined {
    return this.devicePath_;
  }

  get isReadOnly(): boolean {
    return this.isReadOnly_;
  }

  /**
   * Whether the device is read-only removable device or not.
   */
  get isReadOnlyRemovableDevice(): boolean {
    return this.isReadOnlyRemovableDevice_;
  }

  get profile(): {displayName: string, isCurrentProfile: boolean} {
    return this.profile_;
  }

  /**
   * Label for the volume.
   */
  get label(): string {
    return this.label_;
  }

  get providerId(): string|undefined {
    return this.providerId_;
  }

  get hasMedia(): boolean {
    return this.hasMedia_;
  }

  /**
   * True if the volume is configurable.
   */
  get configurable(): boolean {
    return this.configurable_;
  }

  /**
   * True if the volume is watchable.
   */
  get watchable(): boolean {
    return this.watchable_;
  }

  /**
   * Source of the volume's data.
   */
  get source(): VolumeManagerCommon.Source {
    return this.source_;
  }

  /**
   * File system type identifier.
   */
  get diskFileSystemType(): VolumeManagerCommon.FileSystemType {
    return this.diskFileSystemType_;
  }

  /**
   * Set of icons for this volume.
   */
  get iconSet(): chrome.fileManagerPrivate.IconSet {
    return this.iconSet_;
  }

  /**
   * Drive label for the volume. Removable partitions belonging to the same
   * physical media device will share the same drive label.
   */
  get driveLabel(): string|undefined {
    return this.driveLabel_;
  }

  /**
   * The path on the remote host where this volume is mounted, for crostini this
   * is the user's homedir (/home/<username>).
   */
  get remoteMountPath(): string|undefined {
    return this.remoteMountPath_;
  }

  /**
   * An entry to be used as prefix of this volume on breadcrumbs, e.g. "My Files
   * > Downloads", "My Files" is a prefixEntry on "Downloads" VolumeInfo.
   */
  get prefixEntry(): FilesAppEntry|null {
    return this.prefixEntry_!;
  }

  set prefixEntry(entry: FilesAppEntry|null) {
    this.prefixEntry_ = entry;
  }

  /**
   * If this is a GuestOS volume, the type of the VM which owns this volume.
   */
  get vmType(): chrome.fileManagerPrivate.VmType|undefined {
    return this.vmType_;
  }

  /**
   * Returns a promise to the entry for the given URL
   */
  private static resolveFileSystemUrl_(url: string): Promise<Entry> {
    return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url));
  }

  /**
   * Sets |sharedDriveDisplayRoot_| if team drives are enabled.
   *
   * The return value will resolve once this operation is complete.
   */
  private resolveSharedDrivesRoot_(): Promise<void> {
    return VolumeInfoImpl
        .resolveFileSystemUrl_(
            this.fileSystem_.root.toURL() +
            VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME)
        .then(
            sharedDrivesRoot => {
              this.sharedDriveDisplayRoot_ = sharedDrivesRoot as DirectoryEntry;
            },
            error => {
              if (error.name != 'NotFoundError') {
                throw error;
              }
            });
  }

  /**
   * Sets |computersDisplayRoot_| if Computers are enabled.
   *
   * If Computers are not enabled, resolveFileSystemUrl_ will return a
   * 'NotFoundError' which will be caught here. Any other errors will be
   * rethrown.
   *
   * The return value will resolve once this operation is complete.
   */
  private resolveComputersRoot_(): Promise<void> {
    return VolumeInfoImpl
        .resolveFileSystemUrl_(
            this.fileSystem_.root.toURL() +
            VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME)
        .then(
            (computersRoot) => {
              this.computersDisplayRoot_ = computersRoot as DirectoryEntry;
            },
            (error) => {
              if (error.name != 'NotFoundError') {
                throw error;
              }
            });
  }

  /**
   * Returns a promise that resolves when the display root is resolved.
   */
  private async resolveDisplayRootImpl_(): Promise<DirectoryEntry> {
    if (!this.fileSystem_) {
      return Promise.reject(this.error);
    }

    if (this.volumeType !== VolumeManagerCommon.VolumeType.DRIVE) {
      this.displayRoot_ = this.fileSystem_.root;
      return Promise.resolve(this.displayRoot_);
    }

    // For Drive, we need to resolve.
    const displayRootURL = this.fileSystem_.root.toURL() + 'root';
    const [displayRoot] = await Promise.all([
      VolumeInfoImpl.resolveFileSystemUrl_(displayRootURL),
      this.resolveSharedDrivesRoot_(),
      this.resolveComputersRoot_(),
    ]);
    // Store the obtained displayRoot.
    this.displayRoot_ = displayRoot as DirectoryEntry;
    return this.displayRoot_;
  }

  resolveDisplayRoot(
      optOnSuccess?: (dirEntry: DirectoryEntry) => void,
      optOnFailure?: (a: any) => void): Promise<DirectoryEntry> {
    if (optOnSuccess) {
      this.displayRootPromise_.then(optOnSuccess, optOnFailure);
    }
    return assert(this.displayRootPromise_);
  }
}
