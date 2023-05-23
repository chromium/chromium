// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for common constants used in Files app.
 * @namespace
 */
export const constants = {};

/**
 * @const {!Array<string>}
 */
constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES = [
  'canPin',
  'hosted',
  'pinned',
];

/**
 * The list of executable file extensions.
 *
 * @const
 * @type {Array<string>}
 */
constants.EXECUTABLE_EXTENSIONS = Object.freeze([
  '.exe',
  '.lnk',
  '.deb',
  '.dmg',
  '.jar',
  '.msi',
]);

/**
 * These metadata is expected to be cached to accelerate computeAdditional.
 * See: crbug.com/458915.
 * @const {!Array<string>}
 */
constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES = [
  'availableOffline',
  'contentMimeType',
  'hosted',
  'canPin',
];

/**
 * Metadata property names used by FileTable and FileGrid.
 * These metadata is expected to be cached.
 * TODO(sashab): Store capabilities as a set of flags to save memory. See
 * https://crbug.com/849997
 *
 * @const {!Array<string>}
 */
constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES = [
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
];

/**
 * Metadata properties used to inform the user about DLP (Data Leak Prevention)
 * Files restrictions. These metadata is expected to be cached.
 *
 * @const {!Array<string>}
 */
constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES = [
  'isDlpRestricted',
  'sourceUrl',
  'isRestrictedForDestination',
];

/**
 * Name of the default crostini VM: crostini::kCrostiniDefaultVmName
 * @const
 */
constants.DEFAULT_CROSTINI_VM = 'termina';

/**
 * Name of the Plugin VM: plugin_vm::kPluginVmName.
 * @const
 */
constants.PLUGIN_VM = 'PvmDefault';

/**
 * Name of the default bruschetta VM: bruschetta::kBruschettaVmName
 * @const
 */
constants.DEFAULT_BRUSCHETTA_VM = 'bru';

/**
 * DOMError type for crostini connection failure.
 * @const {string}
 */
constants.CROSTINI_CONNECT_ERR = 'CrostiniConnectErr';

/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * URLs.
 * @const {string}
 */
constants.FSP_ACTION_HIDDEN_ONEDRIVE_URL = 'HIDDEN_ONEDRIVE_URL';

/**
 * ID of the fake fileSystemProvider custom action containing OneDrive document
 * User Emails.
 * @const {string}
 */
constants.FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL = 'HIDDEN_ONEDRIVE_USER_EMAIL';

/**
 * All icon types.
 */
constants.ICON_TYPES = {
  ANDROID_FILES: 'android_files',
  ARCHIVE: 'archive',
  AUDIO: 'audio',
  BRUSCHETTA: 'bruschetta',
  BULK_PINNING_DONE: 'bulk_pinning_done',
  BULK_PINNING_OFFLINE: 'bulk_pinning_offline',
  CAMERA_FOLDER: 'camera-folder',
  CANT_PIN: 'cant-pin',
  CHECK: 'check',
  CLOUD_DONE: 'cloud_done',
  CLOUD_ERROR: 'cloud_error',
  CLOUD_OFFLINE: 'cloud_offline',
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
  ENCRYPTED: 'encrypted',
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
 * @const
 * @type {string}
 */
constants.ODFS_EXTENSION_ID = 'gnnndjlaomemikopnjhhnoombakkkkdg';
