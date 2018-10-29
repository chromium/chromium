// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @constructor
 * @implements {VolumeInfo}
 * @struct
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType The type of the volume.
 * @param {string} volumeId ID of the volume.
 * @param {FileSystem} fileSystem The file system object for this volume.
 * @param {(string|undefined)} error The error if an error is found.
 * @param {(string|undefined)} deviceType The type of device
 *     ('usb'|'sd'|'optical'|'mobile'|'unknown') (as defined in
 *     chromeos/disks/disk_mount_manager.cc). Can be undefined.
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
 *     type indentifier.
 * @param {!chrome.fileManagerPrivate.IconSet} iconSet Set of icons for this
 *     volume.
 */
function VolumeInfoImpl(
    volumeType, volumeId, fileSystem, error, deviceType, devicePath, isReadOnly,
    isReadOnlyRemovableDevice, profile, label, providerId, hasMedia,
    configurable, watchable, source, diskFileSystemType, iconSet) {
  this.volumeType_ = volumeType;
  this.volumeId_ = volumeId;
  this.fileSystem_ = fileSystem;
  this.label_ = label;
  this.displayRoot_ = null;
  this.teamDriveDisplayRoot_ = null;
  this.computersDisplayRoot_ = null;

  /**
   * @type {FilesAppEntry} an entry to be used as prefix of this volume on
   *     breadcrumbs, e.g. "My Files > Downloads", "My Files" is a prefixEntry
   *     on "Downloads" VolumeInfo.
   */
  this.prefixEntry_ = null;

  /** @type {Object<!FakeEntry>} */
  this.fakeEntries_ = {};

  /** @type {Promise<!DirectoryEntry>} */
  this.displayRootPromise_ = null;

  if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_OFFLINE] =
        new FakeEntry(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'),
            VolumeManagerCommon.RootType.DRIVE_OFFLINE);

    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME] =
        new FakeEntry(
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
}

VolumeInfoImpl.prototype = /** @struct */ {
  /**
   * @return {VolumeManagerCommon.VolumeType} Volume type.
   */
  get volumeType() {
    return this.volumeType_;
  },
  /**
   * @return {string} Volume ID.
   */
  get volumeId() {
    return this.volumeId_;
  },
  /**
   * @return {FileSystem} File system object.
   */
  get fileSystem() {
    return this.fileSystem_;
  },
  /**
   * @return {DirectoryEntry} Display root path. It is null before finishing to
   * resolve the entry.
   */
  get displayRoot() {
    return this.displayRoot_;
  },
  /**
   * @return {DirectoryEntry} The display root path of Team Drives directory.
   * It is null before finishing to resolve the entry. Valid only for Drive
   * volume.
   */
  get teamDriveDisplayRoot() {
    return this.teamDriveDisplayRoot_;
  },
  /**
   * @return {DirectoryEntry} The display root path of Computers directory.
   * It is null before finishing to resolve the entry. Valid only for Drive
   * volume.
   */
  get computersDisplayRoot() {
    return this.computersDisplayRoot_;
  },
  /**
   * @return {Object<!FakeEntry>} Fake entries.
   */
  get fakeEntries() {
    return this.fakeEntries_;
  },
  /**
   * @return {(string|undefined)} Error identifier.
   */
  get error() {
    return this.error_;
  },
  /**
   * @return {(string|undefined)} Device type identifier.
   */
  get deviceType() {
    return this.deviceType_;
  },
  /**
   * @return {(string|undefined)} Device identifier.
   */
  get devicePath() {
    return this.devicePath_;
  },
  /**
   * @return {boolean} Whether read only or not.
   */
  get isReadOnly() {
    return this.isReadOnly_;
  },
  /**
   * @return {boolean} Whether the device is read-only removable device or not.
   */
  get isReadOnlyRemovableDevice() {
    return this.isReadOnlyRemovableDevice_;
  },
  /**
   * @return {!{displayName:string, isCurrentProfile:boolean}} Profile data.
   */
  get profile() {
    return this.profile_;
  },
  /**
   * @return {string} Label for the volume.
   */
  get label() {
    return this.label_;
  },
  /**
   * @return {(string|undefined)} Id of a provider for this volume.
   */
  get providerId() {
    return this.providerId_;
  },
  /**
   * @return {boolean} True if the volume contains media.
   */
  get hasMedia() {
    return this.hasMedia_;
  },
  /**
   * @return {boolean} True if the volume is configurable.
   */
  get configurable() {
    return this.configurable_;
  },
  /**
   * @return {boolean} True if the volume is watchable.
   */
  get watchable() {
    return this.watchable_;
  },
  /**
   * @return {VolumeManagerCommon.Source} Source of the volume's data.
   */
  get source() {
    return this.source_;
  },
  /**
   * @return {VolumeManagerCommon.FileSystemType} File system type identifier.
   */
  get diskFileSystemType() {
    return this.diskFileSystemType_;
  },
  /**
   * @return {chrome.fileManagerPrivate.IconSet} Set of icons for this volume.
   */
  get iconSet() {
    return this.iconSet_;
  },
  /**
   * @type {FilesAppEntry} an entry to be used as prefix of this volume on
   *     breadcrumbs, e.g. "My Files > Downloads", "My Files" is a prefixEntry
   *     on "Downloads" VolumeInfo.
   */
  get prefixEntry() {
    return this.prefixEntry_;
  },
  set prefixEntry(entry) {
    this.prefixEntry_ = entry;
  },
};

/**
 * Returns a promise to the entry for the given URL
 * @param {string} url The filesystem URL
 * @return {!Promise<Entry>}
 */
VolumeInfoImpl.resolveFileSystemUrl_ = function(url) {
  return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url));
};

/**
 * Sets |teamDriveDisplayRoot_| if team drives are enabled.
 *
 * The return value will resolve once this operation is complete.
 * @return {!Promise<void>}
 */
VolumeInfoImpl.prototype.resolveTeamDrivesRoot_ = function() {
  return VolumeInfoImpl
      .resolveFileSystemUrl_(
          this.fileSystem_.root.toURL() +
          VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_NAME)
      .then(
          teamDrivesRoot => {
            this.teamDriveDisplayRoot_ = teamDrivesRoot;
          },
          error => {
            if (error.name != 'NotFoundError') {
              throw error;
            }
          });
};

/**
 * Sets |computersDisplayRoot_| if Computers are enabled.
 *
 * If Computers are not enabled, resolveFileSystemUrl_ will return a
 * 'NotFoundError' which will be caught here. Any other errors will be rethrown.
 *
 * The return value will resolve once this operation is complete.
 * @return {!Promise<void>}
 */
VolumeInfoImpl.prototype.resolveComputersRoot_ = function() {
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
};

/**
 * @override
 */
VolumeInfoImpl.prototype.resolveDisplayRoot = function(opt_onSuccess,
                                                       opt_onFailure) {
  if (!this.displayRootPromise_) {
    // TODO(mtomasz): Do not add VolumeInfo which failed to resolve root, and
    // remove this if logic. Call opt_onSuccess() always, instead.
    if (this.volumeType !== VolumeManagerCommon.VolumeType.DRIVE) {
      if (this.fileSystem_)
        this.displayRootPromise_ = /** @type {Promise<!DirectoryEntry>} */ (
            Promise.resolve(this.fileSystem_.root));
      else
        this.displayRootPromise_ = /** @type {Promise<!DirectoryEntry>} */ (
            Promise.reject(this.error));
    } else {
      // For Drive, we need to resolve.
      var displayRootURL = this.fileSystem_.root.toURL() + 'root';
      this.displayRootPromise_ =
          Promise
              .all([
                VolumeInfoImpl.resolveFileSystemUrl_(displayRootURL),
                this.resolveTeamDrivesRoot_(),
                this.resolveComputersRoot_(),
              ])
              .then(([root]) => {
                return root;
              });
    }

    // Store the obtained displayRoot.
    this.displayRootPromise_.then(function(displayRoot) {
      this.displayRoot_ = displayRoot;
    }.bind(this));
  }
  if (opt_onSuccess)
    this.displayRootPromise_.then(opt_onSuccess, opt_onFailure);
  return this.displayRootPromise_;
};
