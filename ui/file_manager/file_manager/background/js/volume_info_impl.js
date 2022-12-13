// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @final
 * @implements {VolumeInfo}
 */
export class VolumeInfoImpl {
  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType The type of the volume.
   * @param {string} volumeId ID of the volume.
   * @param {FileSystem} fileSystem The file system object for this volume.
   * @param {(string|undefined)} error The error if an error is found.
   * @param {(string|undefined)} deviceType The type of device
   *     ('usb'|'sd'|'optical'|'mobile'|'unknown') (as defined in
   *     chromeos/ash/components/disks/disk_mount_manager.cc). Can be undefined.
   * @param {(string|undefined)} devicePath Identifier of the device that the
   *     volume belongs to. Can be undefined.
   * @param {boolean} isReadOnly True if the volume is read only.
   * @param {boolean} isReadOnlyRemovableDevice True if the volume is read only
   *     removable device.
   * @param {!{displayName:string, isCurrentProfile:boolean}} profile Profile
   *     information.
   * @param {string} label Label of the volume.
   * @param {(string|undefined)} providerId Id of the provider for this volume.
   *     Undefined for non-FSP volumes.
   * @param {boolean} hasMedia When true the volume has been identified
   *     as containing media such as photos or videos.
   * @param {boolean} configurable When true, then the volume can be configured.
   * @param {boolean} watchable When true, then the volume can be watched.
   * @param {VolumeManagerCommon.Source} source Source of the volume's data.
   * @param {VolumeManagerCommon.FileSystemType} diskFileSystemType File system
   *     type identifier.
   * @param {!chrome.fileManagerPrivate.IconSet} iconSet Set of icons for this
   *     volume.
   * @param {(string|undefined)} driveLabel Drive label of the volume. Removable
   *     partitions belonging to the same device will share the same drive
   *     label.
   * @param {(string|undefined)} remoteMountPath The path on the remote host
   *     where this volume is mounted, for crostini this is the user's homedir
   *     (/home/<username>).
   * @param {(chrome.fileManagerPrivate.VmType|undefined)} vmType If this is a
   *     GuestOS volume, the type of the VM which owns the volume.
   */
  constructor(
      volumeType, volumeId, fileSystem, error, deviceType, devicePath,
      isReadOnly, isReadOnlyRemovableDevice, profile, label, providerId,
      hasMedia, configurable, watchable, source, diskFileSystemType, iconSet,
      driveLabel, remoteMountPath, vmType) {
    this.volumeType_ = volumeType;
    this.volumeId_ = volumeId;
    this.fileSystem_ = fileSystem;
    this.label_ = label;
    this.displayRoot_ = null;
    this.sharedDriveDisplayRoot_ = null;
    this.computersDisplayRoot_ = null;
    this.vmType_ = vmType;

    /**
     * @private {?FilesAppEntry} an entry to be used as prefix of this volume on
     *     breadcrumbs, e.g. "My Files > Downloads", "My Files" is a prefixEntry
     *     on "Downloads" VolumeInfo.
     */
    this.prefixEntry_ = null;

    /** @private @const {!Object<!FakeEntry>} */
    this.fakeEntries_ = {};

    if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_OFFLINE] =
          new FakeEntryImpl(
              str('DRIVE_OFFLINE_COLLECTION_LABEL'),
              VolumeManagerCommon.RootType.DRIVE_OFFLINE);

      this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME] =
          new FakeEntryImpl(
              str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
              VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME);
    }

    // Note: This represents if the mounting of the volume is successfully done
    // or not. (If error is empty string, the mount is successfully done).
    // TODO(hidehiko): Rename to make this more understandable.
    this.error_ = error;
    this.deviceType_ = deviceType;
    this.devicePath_ = devicePath;
    this.isReadOnly_ = isReadOnly;
    this.isReadOnlyRemovableDevice_ = isReadOnlyRemovableDevice;
    this.profile_ = Object.freeze(profile);
    this.providerId_ = providerId;
    this.hasMedia_ = hasMedia;
    this.configurable_ = configurable;
    this.watchable_ = watchable;
    this.source_ = source;
    this.diskFileSystemType_ = diskFileSystemType;
    this.iconSet_ = iconSet;
    this.driveLabel_ = driveLabel;
    this.remoteMountPath_ = remoteMountPath;

    /** @private @const {Promise<!DirectoryEntry>} */
    this.displayRootPromise_ = this.resolveDisplayRootImpl_();
  }

  /**
   * @return {VolumeManagerCommon.VolumeType} Volume type.
   */
  get volumeType() {
    return this.volumeType_;
  }

  /**
   * @return {string} Volume ID.
   */
  get volumeId() {
    return this.volumeId_;
  }

  /**
   * @return {FileSystem} File system object.
   */
  get fileSystem() {
    return this.fileSystem_;
  }

  /**
   * @return {DirectoryEntry} Display root path. It is null before finishing to
   * resolve the entry.
   */
  get displayRoot() {
    return this.displayRoot_;
  }

  /**
   * @return {DirectoryEntry} The display root path of Shared Drives directory.
   * It is null before finishing to resolve the entry. Valid only for Drive
   * volume.
   */
  get sharedDriveDisplayRoot() {
    return this.sharedDriveDisplayRoot_;
  }

  /**
   * @return {DirectoryEntry} The display root path of Computers directory.
   * It is null before finishing to resolve the entry. Valid only for Drive
   * volume.
   */
  get computersDisplayRoot() {
    return this.computersDisplayRoot_;
  }

  /**
   * @return {Object<!FakeEntry>} Fake entries.
   */
  get fakeEntries() {
    return this.fakeEntries_;
  }

  /**
   * @return {(string|undefined)} Error identifier.
   */
  get error() {
    return this.error_;
  }

  /**
   * @return {(string|undefined)} Device type identifier.
   */
  get deviceType() {
    return this.deviceType_;
  }

  /**
   * @return {(string|undefined)} Device identifier.
   */
  get devicePath() {
    return this.devicePath_;
  }

  /**
   * @return {boolean} Whether read only or not.
   */
  get isReadOnly() {
    return this.isReadOnly_;
  }

  /**
   * @return {boolean} Whether the device is read-only removable device or not.
   */
  get isReadOnlyRemovableDevice() {
    return this.isReadOnlyRemovableDevice_;
  }

  /**
   * @return {!{displayName:string, isCurrentProfile:boolean}} Profile data.
   */
  get profile() {
    return this.profile_;
  }

  /**
   * @return {string} Label for the volume.
   */
  get label() {
    return this.label_;
  }

  /**
   * @return {(string|undefined)} Id of a provider for this volume.
   */
  get providerId() {
    return this.providerId_;
  }

  /**
   * @return {boolean} True if the volume contains media.
   */
  get hasMedia() {
    return this.hasMedia_;
  }

  /**
   * @return {boolean} True if the volume is configurable.
   */
  get configurable() {
    return this.configurable_;
  }

  /**
   * @return {boolean} True if the volume is watchable.
   */
  get watchable() {
    return this.watchable_;
  }

  /**
   * @return {VolumeManagerCommon.Source} Source of the volume's data.
   */
  get source() {
    return this.source_;
  }

  /**
   * @return {VolumeManagerCommon.FileSystemType} File system type identifier.
   */
  get diskFileSystemType() {
    return this.diskFileSystemType_;
  }

  /**
   * @return {chrome.fileManagerPrivate.IconSet} Set of icons for this volume.
   */
  get iconSet() {
    return this.iconSet_;
  }

  /**
   * @return {(string|undefined)} Drive label for the volume. Removable
   * partitions belonging to the same physical media device will share the
   * same drive label.
   */
  get driveLabel() {
    return this.driveLabel_;
  }

  /**
   * The path on the remote host where this volume is mounted, for crostini this
   * is the user's homedir (/home/<username>).
   * @return {(string|undefined)}
   */
  get remoteMountPath() {
    return this.remoteMountPath_;
  }

  /**
   * @type {FilesAppEntry} an entry to be used as prefix of this volume on
   *     breadcrumbs, e.g. "My Files > Downloads", "My Files" is a prefixEntry
   *     on "Downloads" VolumeInfo.
   */
  get prefixEntry() {
    return this.prefixEntry_;
  }

  set prefixEntry(entry) {
    this.prefixEntry_ = entry;
  }

  /**
   * @type {chrome.fileManagerPrivate.VmType|undefined} If this is a GuestOS
   *     volume, the type of the VM which owns this volume.
   */
  get vmType() {
    return this.vmType_;
  }

  /**
   * Returns a promise to the entry for the given URL
   * @param {string} url The filesystem URL
   * @return {!Promise<Entry>}
   * @private
   */
  static resolveFileSystemUrl_(url) {
    return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url));
  }

  /**
   * Sets |sharedDriveDisplayRoot_| if team drives are enabled.
   *
   * The return value will resolve once this operation is complete.
   * @return {!Promise<void>}
   */
  resolveSharedDrivesRoot_() {
    return VolumeInfoImpl
        .resolveFileSystemUrl_(
            this.fileSystem_.root.toURL() +
            VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME)
        .then(
            sharedDrivesRoot => {
              this.sharedDriveDisplayRoot_ = sharedDrivesRoot;
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
   * @return {!Promise<void>}
   */
  resolveComputersRoot_() {
    return VolumeInfoImpl
        .resolveFileSystemUrl_(
            this.fileSystem_.root.toURL() +
            VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME)
        .then(
            (computersRoot) => {
              this.computersDisplayRoot_ = computersRoot;
            },
            (error) => {
              if (error.name != 'NotFoundError') {
                throw error;
              }
            });
  }

  /**
   * Returns a promise that resolves when the display root is resolved.
   * @return {Promise<!DirectoryEntry>} Volume type.
   */
  resolveDisplayRootImpl_() {
    if (!this.fileSystem_) {
      return Promise.reject(this.error);
    }

    if (this.volumeType !== VolumeManagerCommon.VolumeType.DRIVE) {
      this.displayRoot_ = this.fileSystem_.root;
      return Promise.resolve(this.displayRoot_);
    }

    // For Drive, we need to resolve.
    const displayRootURL = this.fileSystem_.root.toURL() + 'root';
    return Promise
        .all([
          VolumeInfoImpl.resolveFileSystemUrl_(displayRootURL),
          this.resolveSharedDrivesRoot_(),
          this.resolveComputersRoot_(),
        ])
        .then(([displayRoot]) => {
          // Store the obtained displayRoot.
          this.displayRoot_ = displayRoot;
          return displayRoot;
        });
  }

  /**
   * @override
   */
  resolveDisplayRoot(opt_onSuccess, opt_onFailure) {
    if (opt_onSuccess) {
      this.displayRootPromise_.then(opt_onSuccess, opt_onFailure);
    }
    return assert(this.displayRootPromise_);
  }
}
