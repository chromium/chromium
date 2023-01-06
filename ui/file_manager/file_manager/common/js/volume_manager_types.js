// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

/**
 * Namespace for common types.
 */
const VolumeManagerCommon = {};

/**
 * Paths that can be handled by the dialog opener in native code.
 * @enum {string}
 * @const
 */
export const AllowedPaths = {
  NATIVE_PATH: 'nativePath',
  ANY_PATH: 'anyPath',
  ANY_PATH_OR_URL: 'anyPathOrUrl',
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
  'vfat': 11,
  'exfat': 15,
  'ntfs': 32,
};

/**
 * Type of a navigation root.
 *
 * Navigation root are the top-level entries in the navigation tree, in the left
 * hand side.
 *
 * This must be kept synchronised with the VolumeManagerRootType variant in
 * tools/metrics/histograms/metadata/file/histograms.xml.
 *
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

  // The grand root entry of Shared Drives in Drive volume.
  SHARED_DRIVES_GRAND_ROOT: 'shared_drives_grand_root',

  // Root directory of a Shared Drive.
  SHARED_DRIVE: 'team_drive',

  // Root for a MTP volume.
  MTP: 'mtp',

  // Root for a provided volume.
  PROVIDED: 'provided',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE: 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT: 'drive_recent',

  // Root for media views.
  MEDIA_VIEW: 'media_view',

  // Root for documents providers.
  DOCUMENTS_PROVIDER: 'documents_provider',

  // Fake root for the mixed "Recent" view.
  RECENT: 'recent',

  // 'Google Drive' fake parent entry of 'My Drive', 'Shared with me' and
  // 'Offline'.
  DRIVE_FAKE_ROOT: 'drive_fake_root',

  // Root for crostini 'Linux files'.
  CROSTINI: 'crostini',

  // Root for mountable Guest OSs.
  GUEST_OS: 'guest_os',

  // Root for android files.
  ANDROID_FILES: 'android_files',

  // My Files root, which aggregates DOWNLOADS, ANDROID_FILES and CROSTINI.
  MY_FILES: 'my_files',

  // The grand root entry of My Computers in Drive volume.
  COMPUTERS_GRAND_ROOT: 'computers_grand_root',

  // Root directory of a Computer.
  COMPUTER: 'computer',

  // Root directory of an external media folder under computers grand root.
  EXTERNAL_MEDIA: 'external_media',

  // Root directory of an SMB file share.
  SMB: 'smb',

  // Trash.
  TRASH: 'trash',
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
  VolumeManagerCommon.RootType.DOWNLOADS,                 // 0
  VolumeManagerCommon.RootType.ARCHIVE,                   // 1
  VolumeManagerCommon.RootType.REMOVABLE,                 // 2
  VolumeManagerCommon.RootType.DRIVE,                     // 3
  VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT,  // 4
  VolumeManagerCommon.RootType.SHARED_DRIVE,              // 5
  VolumeManagerCommon.RootType.MTP,                       // 6
  VolumeManagerCommon.RootType.PROVIDED,                  // 7
  'DEPRECATED_DRIVE_OTHER',                               // 8
  VolumeManagerCommon.RootType.DRIVE_OFFLINE,             // 9
  VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,      // 10
  VolumeManagerCommon.RootType.DRIVE_RECENT,              // 11
  VolumeManagerCommon.RootType.MEDIA_VIEW,                // 12
  VolumeManagerCommon.RootType.RECENT,                    // 13
  VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT,           // 14
  'DEPRECATED_ADD_NEW_SERVICES_MENU',                     // 15
  VolumeManagerCommon.RootType.CROSTINI,                  // 16
  VolumeManagerCommon.RootType.ANDROID_FILES,             // 17
  VolumeManagerCommon.RootType.MY_FILES,                  // 18
  VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT,      // 19
  VolumeManagerCommon.RootType.COMPUTER,                  // 20
  VolumeManagerCommon.RootType.EXTERNAL_MEDIA,            // 21
  VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,        // 22
  VolumeManagerCommon.RootType.SMB,                       // 23
  'DEPRECATED_RECENT_AUDIO',                              // 24
  'DEPRECATED_RECENT_IMAGES',                             // 25
  'DEPRECATED_RECENT_VIDEOS',                             // 26
  VolumeManagerCommon.RootType.TRASH,                     // 27
  VolumeManagerCommon.RootType.GUEST_OS,                  // 28
];

/**
 * Error type of VolumeManager.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.VolumeError = {
  /* Internal errors */
  TIMEOUT: 'timeout',

  /* System events */
  UNKNOWN_ERROR: chrome.fileManagerPrivate.MountError.UNKNOWN_ERROR,
  INTERNAL_ERROR: chrome.fileManagerPrivate.MountError.INTERNAL_ERROR,
  INVALID_ARGUMENT: chrome.fileManagerPrivate.MountError.INVALID_ARGUMENT,
  INVALID_PATH: chrome.fileManagerPrivate.MountError.INVALID_PATH,
  PATH_ALREADY_MOUNTED:
      chrome.fileManagerPrivate.MountError.PATH_ALREADY_MOUNTED,
  PATH_NOT_MOUNTED: chrome.fileManagerPrivate.MountError.PATH_NOT_MOUNTED,
  DIRECTORY_CREATION_FAILED:
      chrome.fileManagerPrivate.MountError.DIRECTORY_CREATION_FAILED,
  INVALID_MOUNT_OPTIONS:
      chrome.fileManagerPrivate.MountError.INVALID_MOUNT_OPTIONS,
  INSUFFICIENT_PERMISSIONS:
      chrome.fileManagerPrivate.MountError.INSUFFICIENT_PERMISSIONS,
  MOUNT_PROGRAM_NOT_FOUND:
      chrome.fileManagerPrivate.MountError.MOUNT_PROGRAM_NOT_FOUND,
  MOUNT_PROGRAM_FAILED:
      chrome.fileManagerPrivate.MountError.MOUNT_PROGRAM_FAILED,
  INVALID_DEVICE_PATH: chrome.fileManagerPrivate.MountError.INVALID_DEVICE_PATH,
  UNKNOWN_FILESYSTEM: chrome.fileManagerPrivate.MountError.UNKNOWN_FILESYSTEM,
  UNSUPPORTED_FILESYSTEM:
      chrome.fileManagerPrivate.MountError.UNSUPPORTED_FILESYSTEM,
  NEED_PASSWORD: chrome.fileManagerPrivate.MountError.NEED_PASSWORD,
  CANCELLED: chrome.fileManagerPrivate.MountError.CANCELLED,
  BUSY: chrome.fileManagerPrivate.MountError.BUSY,
};
Object.freeze(VolumeManagerCommon.VolumeError);

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
  DOCUMENTS_PROVIDER: 'documents_provider',
  CROSTINI: 'crostini',
  GUEST_OS: 'guest_os',
  ANDROID_FILES: 'android_files',
  MY_FILES: 'my_files',
  SMB: 'smb',
  SYSTEM_INTERNAL: 'system_internal',
  TRASH: 'trash',
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
  SYSTEM: 'system',
};

/**
 * Returns if the volume is linux native file system or not. Non-native file
 * system does not support few operations (e.g. load unpacked extension).
 * @param {VolumeManagerCommon.VolumeType} type
 * @return {boolean}
 */
export function isNative(type) {
  return type === VolumeManagerCommon.VolumeType.DOWNLOADS ||
      type === VolumeManagerCommon.VolumeType.DRIVE ||
      type === VolumeManagerCommon.VolumeType.ANDROID_FILES ||
      type === VolumeManagerCommon.VolumeType.CROSTINI ||
      type === VolumeManagerCommon.VolumeType.GUEST_OS ||
      type === VolumeManagerCommon.VolumeType.REMOVABLE ||
      type === VolumeManagerCommon.VolumeType.ARCHIVE ||
      type === VolumeManagerCommon.VolumeType.SMB;
}

Object.freeze(VolumeManagerCommon.VolumeType);

/**
 * Obtains volume type from root type.
 * @param {VolumeManagerCommon.RootType} rootType RootType
 * @return {VolumeManagerCommon.VolumeType}
 */
VolumeManagerCommon.getVolumeTypeFromRootType = rootType => {
  switch (rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return VolumeManagerCommon.VolumeType.DOWNLOADS;
    case VolumeManagerCommon.RootType.ARCHIVE:
      return VolumeManagerCommon.VolumeType.ARCHIVE;
    case VolumeManagerCommon.RootType.REMOVABLE:
      return VolumeManagerCommon.VolumeType.REMOVABLE;
    case VolumeManagerCommon.RootType.DRIVE:
    case VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT:
    case VolumeManagerCommon.RootType.SHARED_DRIVE:
    case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
    case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
    case VolumeManagerCommon.RootType.DRIVE_RECENT:
    case VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT:
    case VolumeManagerCommon.RootType.COMPUTER:
    case VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT:
    case VolumeManagerCommon.RootType.EXTERNAL_MEDIA:
      return VolumeManagerCommon.VolumeType.DRIVE;
    case VolumeManagerCommon.RootType.MTP:
      return VolumeManagerCommon.VolumeType.MTP;
    case VolumeManagerCommon.RootType.PROVIDED:
      return VolumeManagerCommon.VolumeType.PROVIDED;
    case VolumeManagerCommon.RootType.MEDIA_VIEW:
      return VolumeManagerCommon.VolumeType.MEDIA_VIEW;
    case VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER:
      return VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER;
    case VolumeManagerCommon.RootType.CROSTINI:
      return VolumeManagerCommon.VolumeType.CROSTINI;
    case VolumeManagerCommon.RootType.GUEST_OS:
      return VolumeManagerCommon.VolumeType.GUEST_OS;
    case VolumeManagerCommon.RootType.ANDROID_FILES:
      return VolumeManagerCommon.VolumeType.ANDROID_FILES;
    case VolumeManagerCommon.RootType.MY_FILES:
      return VolumeManagerCommon.VolumeType.MY_FILES;
    case VolumeManagerCommon.RootType.SMB:
      return VolumeManagerCommon.VolumeType.SMB;
    case VolumeManagerCommon.RootType.TRASH:
      return VolumeManagerCommon.VolumeType.TRASH;
  }

  assertNotReached('Unknown root type: ' + rootType);
};

/**
 * Obtains root type from volume type.
 * @param {VolumeManagerCommon.VolumeType} volumeType .
 * @return {VolumeManagerCommon.RootType}
 */
VolumeManagerCommon.getRootTypeFromVolumeType = volumeType => {
  switch (volumeType) {
    case VolumeManagerCommon.VolumeType.ANDROID_FILES:
      return VolumeManagerCommon.RootType.ANDROID_FILES;
    case VolumeManagerCommon.VolumeType.ARCHIVE:
      return VolumeManagerCommon.RootType.ARCHIVE;
    case VolumeManagerCommon.VolumeType.CROSTINI:
      return VolumeManagerCommon.RootType.CROSTINI;
    case VolumeManagerCommon.VolumeType.GUEST_OS:
      return VolumeManagerCommon.RootType.GUEST_OS;
    case VolumeManagerCommon.VolumeType.DOWNLOADS:
      return VolumeManagerCommon.RootType.DOWNLOADS;
    case VolumeManagerCommon.VolumeType.DRIVE:
      return VolumeManagerCommon.RootType.DRIVE;
    case VolumeManagerCommon.VolumeType.MEDIA_VIEW:
      return VolumeManagerCommon.RootType.MEDIA_VIEW;
    case VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER:
      return VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER;
    case VolumeManagerCommon.VolumeType.MTP:
      return VolumeManagerCommon.RootType.MTP;
    case VolumeManagerCommon.VolumeType.MY_FILES:
      return VolumeManagerCommon.RootType.MY_FILES;
    case VolumeManagerCommon.VolumeType.PROVIDED:
      return VolumeManagerCommon.RootType.PROVIDED;
    case VolumeManagerCommon.VolumeType.REMOVABLE:
      return VolumeManagerCommon.RootType.REMOVABLE;
    case VolumeManagerCommon.VolumeType.SMB:
      return VolumeManagerCommon.RootType.SMB;
    case VolumeManagerCommon.VolumeType.TRASH:
      return VolumeManagerCommon.RootType.TRASH;
  }

  assertNotReached('Unknown volume type: ' + volumeType);
};

/**
 * Returns true if the given |volumeType| is expected to provide third party
 * icons in the iconSet property of the volume.
 * @param {VolumeManagerCommon.VolumeType} volumeType
 * @return {boolean}
 */
VolumeManagerCommon.shouldProvideIcons = volumeType => {
  switch (volumeType) {
    case VolumeManagerCommon.VolumeType.ANDROID_FILES:
      return true;
    case VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER:
      return true;
    case VolumeManagerCommon.VolumeType.PROVIDED:
      return true;
  }

  if (!volumeType) {
    assertNotReached('Invalid volume type: ' + volumeType);
  }

  return false;
};

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
  DOCUMENTS: 'documents_root',
};
Object.freeze(VolumeManagerCommon.MediaViewRootType);

/**
 * Obtains volume type from root type.
 * @param {string} volumeId Volume ID.
 * @return {VolumeManagerCommon.MediaViewRootType}
 */
VolumeManagerCommon.getMediaViewRootTypeFromVolumeId = volumeId => {
  return /** @type {VolumeManagerCommon.MediaViewRootType} */ (
      volumeId.split(':', 2)[1]);
};

/**
 * An event name trigerred when a user tries to mount the volume which is
 * already mounted. The event object must have a volumeId property.
 * @const {string}
 */
VolumeManagerCommon.VOLUME_ALREADY_MOUNTED = 'volume_already_mounted';

VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME = 'team_drives';
VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH =
    '/' + VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME;

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
 * ID of the Google Photos DocumentsProvider volume.
 * @const {string}
 */
VolumeManagerCommon.PHOTOS_DOCUMENTS_PROVIDER_VOLUME_ID =
    'documents_provider:com.google.android.apps.photos.photoprovider/com.google.android.apps.photos';

/**
 * ID of the MediaDocumentsProvider. All the files returned by ARC source in
 * Recents have this ID prefix in their filesystem.
 * @const {string}
 */
VolumeManagerCommon.MEDIA_DOCUMENTS_PROVIDER_ID =
    'com.android.providers.media.documents';


/**
 * Creates an CustomEvent object for changing current directory when an archive
 * file is newly mounted, or when opened a one already mounted.
 * @param {!DirectoryEntry} mountPoint The root directory of the mounted
 *     volume.
 * @return {!CustomEvent<!DirectoryEntry>}
 */
VolumeManagerCommon.createArchiveOpenedEvent = mountPoint => {
  return new CustomEvent(
      VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE,
      {detail: {mountPoint: mountPoint}});
};

/**
 * Checks if a file entry is a Recent entry coming from ARC source.
 * @param {?Entry} entry
 * @return {boolean}
 */
VolumeManagerCommon.isRecentArcEntry = entry => {
  if (!entry) {
    return false;
  }
  return entry.filesystem.name.startsWith(
      VolumeManagerCommon.MEDIA_DOCUMENTS_PROVIDER_ID);
};

export {VolumeManagerCommon};
