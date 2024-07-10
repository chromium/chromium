// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES = [
  'canPin',
  'hosted',
  'pinned',
] as const;

/**
 * These metadata is expected to be cached to accelerate computeAdditional.
 * See: crbug.com/458915.
 */
export const FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES = [
  'availableOffline',
  'contentMimeType',
  'hosted',
  'canPin',
] as const;

/**
 * Metadata property names used by FileTable and FileGrid.
 * These metadata is expected to be cached.
 * TODO(sashab): Store capabilities as a set of flags to save memory. See
 * https://crbug.com/849997
 *
 */
export const LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES = [
  'availableOffline',
  'contentMimeType',
  'customIconUrl',
  'hosted',
  'modificationTime',
  'modificationByMeTime',
  'pinned',
  'shared',
  'size',
  'canCopy',
  'canDelete',
  'canRename',
  'canAddChildren',
  'canShare',
  'canPin',
  'isMachineRoot',
  'isExternalMedia',
  'isArbitrarySyncFolder',
] as const;

/**
 * Metadata properties used to inform the user about DLP (Data Leak Prevention)
 * Files restrictions. These metadata is expected to be cached.
 */
export const DLP_METADATA_PREFETCH_PROPERTY_NAMES = [
  'isDlpRestricted',
  'sourceUrl',
  'isRestrictedForDestination',
] as const;

/**
 * Name of the default crostini VM: crostini::kCrostiniDefaultVmName
 */
export const DEFAULT_CROSTINI_VM = 'termina';

/**
 * Name of the Plugin VM: plugin_vm::kPluginVmName.
 */
export const PLUGIN_VM = 'PvmDefault';

/**
 * Name of the default bruschetta VM: bruschetta::kBruschettaVmName
 */
export const DEFAULT_BRUSCHETTA_VM = 'bru';

/**
 * DOMError type for crostini connection failure.
 */
export const CROSTINI_CONNECT_ERR = 'CrostiniConnectErr';

/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * URLs.
 */
export const FSP_ACTION_HIDDEN_ONEDRIVE_URL = 'HIDDEN_ONEDRIVE_URL';

/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * User Emails.
 */
export const FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL =
    'HIDDEN_ONEDRIVE_USER_EMAIL';

// TODO(b/330786891): Remove this once it's no longer needed for backwards
// compatibility with ODFS.
/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * Reauthentication Required state.
 */
export const FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED =
    'HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED';

/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * Account state.
 */
export const FSP_ACTION_HIDDEN_ONEDRIVE_ACCOUNT_STATE =
    'HIDDEN_ONEDRIVE_ACCOUNT_STATE';

/**
 * An array of IDs of fake fileSystemProvider custom actions for ODFS.
 */
export const FSP_ACTIONS_HIDDEN = [
  FSP_ACTION_HIDDEN_ONEDRIVE_URL,
  FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL,
  FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED,
  FSP_ACTION_HIDDEN_ONEDRIVE_ACCOUNT_STATE,
];

/**
 * All icon types.
 */
export const ICON_TYPES = {
  ANDROID_FILES: 'android_files',
  ARCHIVE: 'archive',
  AUDIO: 'audio',
  // Explicitly request the icon to be 0x0. Used to avoid the scenario where a
  // `type` is not specifically supplied vs. actually wanting a blank icon.
  BLANK: 'blank',
  BRUSCHETTA: 'bruschetta',
  BULK_PINNING_BATTERY_SAVER: 'bulk_pinning_battery_saver',
  BULK_PINNING_DONE: 'bulk_pinning_done',
  BULK_PINNING_OFFLINE: 'bulk_pinning_offline',
  CAMERA_FOLDER: 'camera-folder',
  CANT_PIN: 'cant-pin',
  CHECK: 'check',
  CLOUD_DONE: 'cloud_done',
  CLOUD_ERROR: 'cloud_error',
  CLOUD_OFFLINE: 'cloud_offline',
  CLOUD_PAUSED: 'cloud_paused',
  CLOUD_SYNC: 'cloud_sync',
  CLOUD: 'cloud',
  COMPUTER: 'computer',
  COMPUTERS_GRAND_ROOT: 'computers_grand_root',
  CROSTINI: 'crostini',
  DOWNLOADS: 'downloads',
  DRIVE_BULK_PINNING: 'drive_bulk_pinning',
  DRIVE_LOGO: 'drive_logo',
  DRIVE_OFFLINE: 'drive_offline',
  DRIVE_RECENT: 'drive_recent',
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',
  DRIVE: 'drive',
  ERROR: 'error',
  ERROR_BANNER: 'error_banner',
  EXCEL: 'excel',
  EXTERNAL_MEDIA: 'external_media',
  FOLDER: 'folder',
  GENERIC: 'generic',
  GOOGLE_DOC: 'gdoc',
  GOOGLE_DRAW: 'gdraw',
  GOOGLE_FORM: 'gform',
  GOOGLE_LINK: 'glink',
  GOOGLE_MAP: 'gmap',
  GOOGLE_SHEET: 'gsheet',
  GOOGLE_SITE: 'gsite',
  GOOGLE_SLIDES: 'gslides',
  GOOGLE_TABLE: 'gtable',
  IMAGE: 'image',
  MTP: 'mtp',
  MY_FILES: 'my_files',
  OFFLINE: 'offline',
  OPTICAL: 'optical',
  PDF: 'pdf',
  PLUGIN_VM: 'plugin_vm',
  POWERPOINT: 'ppt',
  RAW: 'raw',
  RECENT: 'recent',
  REMOVABLE: 'removable',
  SCRIPT: 'script',
  SD_CARD: 'sd',
  SERVICE_DRIVE: 'service_drive',
  SHARED_DRIVE: 'shared_drive',
  SHARED_DRIVES_GRAND_ROOT: 'shared_drives_grand_root',
  SHARED_FOLDER: 'shared_folder',
  SHORTCUT: 'shortcut',
  SITES: 'sites',
  SMB: 'smb',
  STAR: 'star',
  TEAM_DRIVE: 'team_drive',
  THUMBNAIL_GENERIC: 'thumbnail_generic',
  TINI: 'tini',
  TRASH: 'trash',
  UNKNOWN_REMOVABLE: 'unknown_removable',
  USB: 'usb',
  VIDEO: 'video',
  WORD: 'word',
};

/**
 * Extension ID for OneDrive FSP, also used as ProviderId.
 */
export const ODFS_EXTENSION_ID = 'gnnndjlaomemikopnjhhnoombakkkkdg';
