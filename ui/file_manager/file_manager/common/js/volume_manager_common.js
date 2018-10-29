// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for common types.
 */
var VolumeManagerCommon = {};

/**
 * Paths that can be handled by the dialog opener in native code.
 * @enum {string}
 * @const
 */
var AllowedPaths = {
  NATIVE_PATH: 'nativePath',
  NATIVE_OR_DRIVE_PATH: 'nativeOrDrivePath',
  ANY_PATH: 'anyPath'
};

/**
 * Type of a file system.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.FileSystemType = {
  UNKNOWN: '',
  VFAT: 'vfat',
  EXFAT: 'exfat',
  NTFS: 'ntfs',
  HFSPLUS: 'hfsplus',
  EXT2: 'ext2',
  EXT3: 'ext3',
  EXT4: 'ext4',
  ISO9660: 'iso9660',
  UDF: 'udf',
};

/**
 * Volume name length limits by file system type
 * @enum {number}
 * @const
 */
VolumeManagerCommon.FileSystemTypeVolumeNameLengthLimit = {
  VFAT: 11,
  EXFAT: 15,
};

/**
 * Type of a root directory.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.RootType = {
  // Root for a downloads directory.
  DOWNLOADS: 'downloads',

  // Root for a mounted archive volume.
  ARCHIVE: 'archive',

  // Root for a removable volume.
  REMOVABLE: 'removable',

  // Root for a drive volume.
  DRIVE: 'drive',

  // The grand root entry of Team Drives in Drive volume.
  TEAM_DRIVES_GRAND_ROOT: 'team_drives_grand_root',

  // Root directory of a Team Drive.
  TEAM_DRIVE: 'team_drive',

  // Root for a MTP volume.
  MTP: 'mtp',

  // Root for a provided volume.
  PROVIDED: 'provided',

  // Root for entries that is not located under RootType.DRIVE. e.g. shared
  // files.
  DRIVE_OTHER: 'drive_other',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE: 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT: 'drive_recent',

  // Root for media views.
  MEDIA_VIEW: 'media_view',

  // Fake root for the mixed "Recent" view.
  RECENT: 'recent',

  // 'Google Drive' fake parent entry of 'My Drive', 'Shared with me' and
  // 'Offline'.
  DRIVE_FAKE_ROOT: 'drive_fake_root',

  // Root for crostini 'Linux files'.
  CROSTINI: 'crostini',

  // Root for android files.
  ANDROID_FILES: 'android_files',

  // My Files root, which aggregates DOWNLOADS, ANDROID_FILES and CROSTINI.
  MY_FILES: 'my_files',

  // The grand root entry of My Computers in Drive volume.
  COMPUTERS_GRAND_ROOT: 'computers_grand_root',

  // Root directory of a Computer.
  COMPUTER: 'computer',
};
Object.freeze(VolumeManagerCommon.RootType);

/**
 * Keep the order of this in sync with FileManagerRootType in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 *
 * @type {!Array<VolumeManagerCommon.RootType>}
 * @const
 */
VolumeManagerCommon.RootTypesForUMA = [
  VolumeManagerCommon.RootType.DOWNLOADS,
  VolumeManagerCommon.RootType.ARCHIVE,
  VolumeManagerCommon.RootType.REMOVABLE,
  VolumeManagerCommon.RootType.DRIVE,
  VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT,
  VolumeManagerCommon.RootType.TEAM_DRIVE,
  VolumeManagerCommon.RootType.MTP,
  VolumeManagerCommon.RootType.PROVIDED,
  VolumeManagerCommon.RootType.DRIVE_OTHER,
  VolumeManagerCommon.RootType.DRIVE_OFFLINE,
  VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
  VolumeManagerCommon.RootType.DRIVE_RECENT,
  VolumeManagerCommon.RootType.MEDIA_VIEW,
  VolumeManagerCommon.RootType.RECENT,
  VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT,
  VolumeManagerCommon.RootType.CROSTINI,
  VolumeManagerCommon.RootType.ANDROID_FILES,
  VolumeManagerCommon.RootType.MY_FILES,
  VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT,
  VolumeManagerCommon.RootType.COMPUTER,
];
console.assert(
    Object.keys(VolumeManagerCommon.RootType).length ===
        VolumeManagerCommon.RootTypesForUMA.length,
    'Members in RootTypesForUMA do not match them in RootTypes.');

/**
 * Error type of VolumeManager.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.VolumeError = {
  /* Internal errors */
  TIMEOUT: 'timeout',

  /* System events */
  UNKNOWN: 'error_unknown',
  INTERNAL: 'error_internal',
  INVALID_ARGUMENT: 'error_invalid_argument',
  INVALID_PATH: 'error_invalid_path',
  ALREADY_MOUNTED: 'error_path_already_mounted',
  PATH_NOT_MOUNTED: 'error_path_not_mounted',
  DIRECTORY_CREATION_FAILED: 'error_directory_creation_failed',
  INVALID_MOUNT_OPTIONS: 'error_invalid_mount_options',
  INVALID_UNMOUNT_OPTIONS: 'error_invalid_unmount_options',
  INSUFFICIENT_PERMISSIONS: 'error_insufficient_permissions',
  MOUNT_PROGRAM_NOT_FOUND: 'error_mount_program_not_found',
  MOUNT_PROGRAM_FAILED: 'error_mount_program_failed',
  INVALID_DEVICE_PATH: 'error_invalid_device_path',
  UNKNOWN_FILESYSTEM: 'error_unknown_filesystem',
  UNSUPPORTED_FILESYSTEM: 'error_unsupported_filesystem',
  INVALID_ARCHIVE: 'error_invalid_archive'
};
Object.freeze(VolumeManagerCommon.VolumeError);

/**
 * List of connection types of drive.
 *
 * Keep this in sync with the kDriveConnectionType* constants in
 * private_api_dirve.cc.
 *
 * @enum {string}
 * @const
 */
VolumeManagerCommon.DriveConnectionType = {
  OFFLINE: 'offline',  // Connection is offline or drive is unavailable.
  METERED: 'metered',  // Connection is metered. Should limit traffic.
  ONLINE: 'online'     // Connection is online.
};
Object.freeze(VolumeManagerCommon.DriveConnectionType);

/**
 * List of reasons of DriveConnectionType.
 *
 * Keep this in sync with the kDriveConnectionReason constants in
 * private_api_drive.cc.
 *
 * @enum {string}
 * @const
 */
VolumeManagerCommon.DriveConnectionReason = {
  NOT_READY: 'not_ready',    // Drive is not ready or authentication is failed.
  NO_NETWORK: 'no_network',  // Network connection is unavailable.
  NO_SERVICE: 'no_service'   // Drive service is unavailable.
};
Object.freeze(VolumeManagerCommon.DriveConnectionReason);

/**
 * The type of each volume.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.VolumeType = {
  DRIVE: 'drive',
  DOWNLOADS: 'downloads',
  REMOVABLE: 'removable',
  ARCHIVE: 'archive',
  MTP: 'mtp',
  PROVIDED: 'provided',
  MEDIA_VIEW: 'media_view',
  CROSTINI: 'crostini',
  ANDROID_FILES: 'android_files',
  MY_FILES: 'my_files',
};

/**
 * Source of each volume's data.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.Source = {
  FILE: 'file',
  DEVICE: 'device',
  NETWORK: 'network',
  SYSTEM: 'system'
};

/**
 * Returns if the volume is linux native file system or not. Non-native file
 * system does not support few operations (e.g. load unpacked extension).
 * @param {VolumeManagerCommon.VolumeType} type
 * @return {boolean}
 */
VolumeManagerCommon.VolumeType.isNative = function(type) {
  return type === VolumeManagerCommon.VolumeType.DOWNLOADS ||
      type === VolumeManagerCommon.VolumeType.ANDROID_FILES ||
      type === VolumeManagerCommon.VolumeType.CROSTINI ||
      type === VolumeManagerCommon.VolumeType.REMOVABLE ||
      type === VolumeManagerCommon.VolumeType.ARCHIVE;
};

Object.freeze(VolumeManagerCommon.VolumeType);

/**
 * Obtains volume type from root type.
 * @param {VolumeManagerCommon.RootType} rootType RootType
 * @return {VolumeManagerCommon.VolumeType}
 */
VolumeManagerCommon.getVolumeTypeFromRootType = function(rootType) {
  switch (rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return VolumeManagerCommon.VolumeType.DOWNLOADS;
    case VolumeManagerCommon.RootType.ARCHIVE:
      return VolumeManagerCommon.VolumeType.ARCHIVE;
    case VolumeManagerCommon.RootType.REMOVABLE:
      return VolumeManagerCommon.VolumeType.REMOVABLE;
    case VolumeManagerCommon.RootType.DRIVE:
    case VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT:
    case VolumeManagerCommon.RootType.TEAM_DRIVE:
    case VolumeManagerCommon.RootType.DRIVE_OTHER:
    case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
    case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
    case VolumeManagerCommon.RootType.DRIVE_RECENT:
    case VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT:
    case VolumeManagerCommon.RootType.COMPUTER:
      return VolumeManagerCommon.VolumeType.DRIVE;
    case VolumeManagerCommon.RootType.MTP:
      return VolumeManagerCommon.VolumeType.MTP;
    case VolumeManagerCommon.RootType.PROVIDED:
      return VolumeManagerCommon.VolumeType.PROVIDED;
    case VolumeManagerCommon.RootType.MEDIA_VIEW:
      return VolumeManagerCommon.VolumeType.MEDIA_VIEW;
    case VolumeManagerCommon.RootType.CROSTINI:
      return VolumeManagerCommon.VolumeType.CROSTINI;
    case VolumeManagerCommon.RootType.ANDROID_FILES:
      return VolumeManagerCommon.VolumeType.ANDROID_FILES;
    case VolumeManagerCommon.RootType.MY_FILES:
      return VolumeManagerCommon.VolumeType.MY_FILES;
  }
  assertNotReached('Unknown root type: ' + rootType);
};

/**
 * @param {VolumeManagerCommon.VolumeType} volumeType .
 * @return {VolumeManagerCommon.RootType}
 */
VolumeManagerCommon.getRootTypeFromVolumeType = function(volumeType) {
  switch (volumeType) {
    case VolumeManagerCommon.VolumeType.ANDROID_FILES:
      return VolumeManagerCommon.RootType.ANDROID_FILES;
    case VolumeManagerCommon.VolumeType.ARCHIVE:
      return VolumeManagerCommon.RootType.ARCHIVE;
    case VolumeManagerCommon.VolumeType.CROSTINI:
      return VolumeManagerCommon.RootType.CROSTINI;
    case VolumeManagerCommon.VolumeType.DOWNLOADS:
      return VolumeManagerCommon.RootType.DOWNLOADS;
    case VolumeManagerCommon.VolumeType.DRIVE:
      return VolumeManagerCommon.RootType.DRIVE;
    case VolumeManagerCommon.VolumeType.MEDIA_VIEW:
      return VolumeManagerCommon.RootType.MEDIA_VIEW;
    case VolumeManagerCommon.VolumeType.MTP:
      return VolumeManagerCommon.RootType.MTP;
    case VolumeManagerCommon.VolumeType.MY_FILES:
      return VolumeManagerCommon.RootType.MY_FILES;
    case VolumeManagerCommon.VolumeType.PROVIDED:
      return VolumeManagerCommon.RootType.PROVIDED;
    case VolumeManagerCommon.VolumeType.REMOVABLE:
      return VolumeManagerCommon.RootType.REMOVABLE;
  }
  assertNotReached('Unknown volume type: ' + volumeType);
};

/**
 * @typedef {{
 *   type: VolumeManagerCommon.DriveConnectionType,
 *   reason: (VolumeManagerCommon.DriveConnectionReason|undefined)
 * }}
 */
VolumeManagerCommon.DriveConnectionState;

/**
 * List of media view root types.
 *
 * Keep this in sync with constants in arc_media_view_util.cc.
 *
 * @enum {string}
 * @const
 */
VolumeManagerCommon.MediaViewRootType = {
  IMAGES: 'images_root',
  VIDEOS: 'videos_root',
  AUDIO: 'audio_root',
};
Object.freeze(VolumeManagerCommon.MediaViewRootType);

/**
 * Obtains volume type from root type.
 * @param {string} volumeId Volume ID.
 * @return {VolumeManagerCommon.MediaViewRootType}
 */
VolumeManagerCommon.getMediaViewRootTypeFromVolumeId = function(volumeId) {
  return /** @type {VolumeManagerCommon.MediaViewRootType} */ (
      volumeId.split(':', 2)[1]);
};

/**
  * An event name trigerred when a user tries to mount the volume which is
  * already mounted. The event object must have a volumeId property.
  * @const {string}
  */
VolumeManagerCommon.VOLUME_ALREADY_MOUNTED = 'volume_already_mounted';

VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_NAME = 'team_drives';
VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH =
    '/' + VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_NAME;

/**
 * This is the top level directory name for Computers in drive that are using
 * the backup and sync feature.
 * @const {string}
 */
VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME = 'Computers';
VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH =
    '/' + VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME;

/**
 * @const
 */
VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE = 'archive_opened';

/**
 * Creates an CustomEvent object for changing current directory when an archive
 * file is newly mounted, or when opened a one already mounted.
 * @param {!DirectoryEntry} mountPoint The root directory of the mounted
 *     volume.
 * @return {!CustomEvent}
 */
VolumeManagerCommon.createArchiveOpenedEvent = function(mountPoint) {
  return new CustomEvent(
      VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE,
      {detail: {mountPoint: mountPoint}});
};
