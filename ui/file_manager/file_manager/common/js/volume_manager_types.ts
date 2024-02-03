// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import {VolumeType} from './shared_types.js';

export {AllowedPaths, VolumeType} from './shared_types.js';


/** Type of a file system. */
export enum FileSystemType {
  UNKNOWN = '',
  VFAT = 'vfat',
  EXFAT = 'exfat',
  NTFS = 'ntfs',
  HFSPLUS = 'hfsplus',
  EXT2 = 'ext2',
  EXT3 = 'ext3',
  EXT4 = 'ext4',
  ISO9660 = 'iso9660',
  UDF = 'udf',
  FUSEBOX = 'fusebox',
}

/** Volume name length limits by file system type. */
export const FileSystemTypeVolumeNameLengthLimit: Record<string, number> = {
  [FileSystemType.VFAT]: 11,
  [FileSystemType.EXFAT]: 15,
  [FileSystemType.NTFS]: 32,
};


/**
 * Type of a navigation root.
 *
 * Navigation root are the top-level entries in the navigation tree, in the
 * left hand side.
 *
 * This must be kept synchronised with the VolumeManagerRootType variant in
 * tools/metrics/histograms/metadata/file/histograms.xml.
 */
export enum RootType {
  // Root for a downloads directory.
  DOWNLOADS = 'downloads',

  // Root for a mounted archive volume.
  ARCHIVE = 'archive',

  // Root for a removable volume.
  REMOVABLE = 'removable',

  // Root for a drive volume.
  DRIVE = 'drive',

  // The grand root entry of Shared Drives in Drive volume.
  SHARED_DRIVES_GRAND_ROOT = 'shared_drives_grand_root',

  // Root directory of a Shared Drive.
  SHARED_DRIVE = 'team_drive',

  // Root for a MTP volume.
  MTP = 'mtp',

  // Root for a provided volume.
  PROVIDED = 'provided',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE = 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME = 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT = 'drive_recent',

  // Root for media views.
  MEDIA_VIEW = 'media_view',

  // Root for documents providers.
  DOCUMENTS_PROVIDER = 'documents_provider',

  // Fake root for the mixed "Recent" view.
  RECENT = 'recent',

  // 'Google Drive' fake parent entry of 'My Drive', 'Shared with me' and
  // 'Offline'.
  DRIVE_FAKE_ROOT = 'drive_fake_root',

  // Root for crostini 'Linux files'.
  CROSTINI = 'crostini',

  // Root for mountable Guest OSs.
  GUEST_OS = 'guest_os',

  // Root for android files.
  ANDROID_FILES = 'android_files',

  // My Files root, which aggregates DOWNLOADS, ANDROID_FILES and CROSTINI.
  MY_FILES = 'my_files',

  // The grand root entry of My Computers in Drive volume.
  COMPUTERS_GRAND_ROOT = 'computers_grand_root',

  // Root directory of a Computer.
  COMPUTER = 'computer',

  // Root directory of an external media folder under computers grand root.
  EXTERNAL_MEDIA = 'external_media',

  // Root directory of an SMB file share.
  SMB = 'smb',

  // Trash.
  TRASH = 'trash',
}

/**
 * Keep the order of this in sync with FileManagerRootType in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for
 * each root type should never be renumbered nor reused in this array.
 */
export const RootTypesForUMA: Array<RootType|string> = [
  RootType.DOWNLOADS,                  // 0
  RootType.ARCHIVE,                    // 1
  RootType.REMOVABLE,                  // 2
  RootType.DRIVE,                      // 3
  RootType.SHARED_DRIVES_GRAND_ROOT,   // 4
  RootType.SHARED_DRIVE,               // 5
  RootType.MTP,                        // 6
  RootType.PROVIDED,                   // 7
  'DEPRECATED_DRIVE_OTHER',            // 8
  RootType.DRIVE_OFFLINE,              // 9
  RootType.DRIVE_SHARED_WITH_ME,       // 10
  RootType.DRIVE_RECENT,               // 11
  RootType.MEDIA_VIEW,                 // 12
  RootType.RECENT,                     // 13
  RootType.DRIVE_FAKE_ROOT,            // 14
  'DEPRECATED_ADD_NEW_SERVICES_MENU',  // 15
  RootType.CROSTINI,                   // 16
  RootType.ANDROID_FILES,              // 17
  RootType.MY_FILES,                   // 18
  RootType.COMPUTERS_GRAND_ROOT,       // 19
  RootType.COMPUTER,                   // 20
  RootType.EXTERNAL_MEDIA,             // 21
  RootType.DOCUMENTS_PROVIDER,         // 22
  RootType.SMB,                        // 23
  'DEPRECATED_RECENT_AUDIO',           // 24
  'DEPRECATED_RECENT_IMAGES',          // 25
  'DEPRECATED_RECENT_VIDEOS',          // 26
  RootType.TRASH,                      // 27
  RootType.GUEST_OS,                   // 28
];

/** Error type of VolumeManager. */
export enum VolumeError {
  /* Internal errors */
  TIMEOUT = 'timeout',

  /* System events */
  SUCCESS = 'success',
  IN_PROGRESS = 'in_progress',
  UNKNOWN_ERROR = 'unknown_error',
  INTERNAL_ERROR = 'internal_error',
  INVALID_ARGUMENT = 'invalid_argument',
  INVALID_PATH = 'invalid_path',
  PATH_ALREADY_MOUNTED = 'path_already_mounted',
  PATH_NOT_MOUNTED = 'path_not_mounted',
  DIRECTORY_CREATION_FAILED = 'directory_creation_failed',
  INVALID_MOUNT_OPTIONS = 'invalid_mount_options',
  INSUFFICIENT_PERMISSIONS = 'insufficient_permissions',
  MOUNT_PROGRAM_NOT_FOUND = 'mount_program_not_found',
  MOUNT_PROGRAM_FAILED = 'mount_program_failed',
  INVALID_DEVICE_PATH = 'invalid_device_path',
  UNKNOWN_FILESYSTEM = 'unknown_filesystem',
  UNSUPPORTED_FILESYSTEM = 'unsupported_filesystem',
  NEED_PASSWORD = 'need_password',
  CANCELLED = 'cancelled',
  BUSY = 'busy',
}

/** Source of each volume's data. */
export enum Source {
  FILE = 'file',
  DEVICE = 'device',
  NETWORK = 'network',
  SYSTEM = 'system',
}

/**
 * @returns if the volume is linux native file system or not. Non-native file
 * system does not support few operations (e.g. load unpacked extension).
 */
export function isNative(type: VolumeType): boolean {
  return type === VolumeType.DOWNLOADS || type === VolumeType.DRIVE ||
      type === VolumeType.ANDROID_FILES || type === VolumeType.CROSTINI ||
      type === VolumeType.GUEST_OS || type === VolumeType.REMOVABLE ||
      type === VolumeType.ARCHIVE || type === VolumeType.SMB;
}

/** Gets volume type from root type. */
export function getVolumeTypeFromRootType(rootType: RootType): VolumeType {
  switch (rootType) {
    case RootType.DOWNLOADS:
      return VolumeType.DOWNLOADS;
    case RootType.ARCHIVE:
      return VolumeType.ARCHIVE;
    case RootType.REMOVABLE:
      return VolumeType.REMOVABLE;
    case RootType.DRIVE:
    case RootType.SHARED_DRIVES_GRAND_ROOT:
    case RootType.SHARED_DRIVE:
    case RootType.DRIVE_OFFLINE:
    case RootType.DRIVE_SHARED_WITH_ME:
    case RootType.DRIVE_RECENT:
    case RootType.COMPUTERS_GRAND_ROOT:
    case RootType.COMPUTER:
    case RootType.DRIVE_FAKE_ROOT:
    case RootType.EXTERNAL_MEDIA:
      return VolumeType.DRIVE;
    case RootType.MTP:
      return VolumeType.MTP;
    case RootType.PROVIDED:
      return VolumeType.PROVIDED;
    case RootType.MEDIA_VIEW:
      return VolumeType.MEDIA_VIEW;
    case RootType.DOCUMENTS_PROVIDER:
      return VolumeType.DOCUMENTS_PROVIDER;
    case RootType.CROSTINI:
      return VolumeType.CROSTINI;
    case RootType.GUEST_OS:
      return VolumeType.GUEST_OS;
    case RootType.ANDROID_FILES:
      return VolumeType.ANDROID_FILES;
    case RootType.MY_FILES:
      return VolumeType.MY_FILES;
    case RootType.SMB:
      return VolumeType.SMB;
    case RootType.TRASH:
      return VolumeType.TRASH;
  }

  assertNotReached('Unknown root type: ' + rootType);
}

/** Gets root type from volume type. */
export function getRootTypeFromVolumeType(volumeType: VolumeType): RootType {
  switch (volumeType) {
    case VolumeType.ANDROID_FILES:
      return RootType.ANDROID_FILES;
    case VolumeType.ARCHIVE:
      return RootType.ARCHIVE;
    case VolumeType.CROSTINI:
      return RootType.CROSTINI;
    case VolumeType.GUEST_OS:
      return RootType.GUEST_OS;
    case VolumeType.DOWNLOADS:
      return RootType.DOWNLOADS;
    case VolumeType.DRIVE:
      return RootType.DRIVE;
    case VolumeType.MEDIA_VIEW:
      return RootType.MEDIA_VIEW;
    case VolumeType.DOCUMENTS_PROVIDER:
      return RootType.DOCUMENTS_PROVIDER;
    case VolumeType.MTP:
      return RootType.MTP;
    case VolumeType.MY_FILES:
      return RootType.MY_FILES;
    case VolumeType.PROVIDED:
      return RootType.PROVIDED;
    case VolumeType.REMOVABLE:
      return RootType.REMOVABLE;
    case VolumeType.SMB:
      return RootType.SMB;
    case VolumeType.TRASH:
      return RootType.TRASH;
  }

  assertNotReached('Unknown volume type: ' + volumeType);
}

/**
 * @returns whether the given `volumeType` is expected to provide third party
 * icons in the iconSet property of the volume.
 */
export function shouldProvideIcons(volumeType: VolumeType): boolean {
  switch (volumeType) {
    case VolumeType.ANDROID_FILES:
    case VolumeType.DOCUMENTS_PROVIDER:
    case VolumeType.PROVIDED:
      return true;
  }

  return false;
}

/**
 * List of media view root types.
 * Keep this in sync with constants in arc_media_view_util.cc.
 */
export enum MediaViewRootType {
  IMAGES = 'images_root',
  VIDEOS = 'videos_root',
  AUDIO = 'audio_root',
  DOCUMENTS = 'documents_root',
}

/** Gets volume type from root type. */
export function getMediaViewRootTypeFromVolumeId(volumeId: string):
    MediaViewRootType {
  return volumeId.split(':', 2)[1] as MediaViewRootType;
}

/**
 * An event name triggered when a user tries to mount the volume which is
 * already mounted. The event object must have a volumeId property.
 */
export const VOLUME_ALREADY_MOUNTED = 'volume_already_mounted';

export const SHARED_DRIVES_DIRECTORY_NAME = 'team_drives';
export const SHARED_DRIVES_DIRECTORY_PATH = '/' + SHARED_DRIVES_DIRECTORY_NAME;

/**
 * This is the top level directory name for Computers in drive that are using
 * the backup and sync feature.
 */
export const COMPUTERS_DIRECTORY_NAME = 'Computers';
export const COMPUTERS_DIRECTORY_PATH = '/' + COMPUTERS_DIRECTORY_NAME;

export const ARCHIVE_OPENED_EVENT_TYPE = 'archive_opened';

/** ID of the Google Photos DocumentsProvider volume. */
export const PHOTOS_DOCUMENTS_PROVIDER_VOLUME_ID =
    'documents_provider:com.google.android.apps.photos.photoprovider/com.google.android.apps.photos';

/**
 * ID of the MediaDocumentsProvider. All the files returned by ARC source in
 * Recents have this ID prefix in their filesystem.
 */
export const MEDIA_DOCUMENTS_PROVIDER_ID =
    'com.android.providers.media.documents';

/** Checks if a file entry is a Recent entry coming from ARC source. */
export function isRecentArcEntry(entry: Entry|null): boolean {
  return !!entry &&
      entry.filesystem.name.startsWith(MEDIA_DOCUMENTS_PROVIDER_ID);
}
