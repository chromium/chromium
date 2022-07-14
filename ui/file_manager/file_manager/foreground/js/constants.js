// Copyright 2017 The Chromium Authors. All rights reserved.
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
];

/**
 * Path for files_quick_view.html file.  Allow override for testing.
 * @type {string}
 */
constants.FILES_QUICK_VIEW_HTML = 'foreground/elements/files_quick_view.html';

/**
 * Path for drive_welcome.css file.  Allow override for testing.
 * @type {string}
 */
constants.DRIVE_WELCOME_CSS = 'foreground/css/drive_welcome.css';

/**
 * Path for photos_welcome.css file.
 * @type {string}
 */
constants.PHOTOS_WELCOME_CSS = 'foreground/css/photos_welcome.css';

/**
 * Path for holding_space_welcome.css file. Allow override for testing.
 * @type {string}
 */
constants.HOLDING_SPACE_WELCOME_CSS =
    'foreground/css/holding_space_welcome.css';

/**
 * Name of the default crostini VM.
 * @const
 */
constants.DEFAULT_CROSTINI_VM = 'termina';

/**
 * Name of the Plugin VM.
 * @const
 */
constants.PLUGIN_VM = 'PvmDefault';

/**
 * DOMError type for crostini connection failure.
 * @const {string}
 */
constants.CROSTINI_CONNECT_ERR = 'CrostiniConnectErr';
