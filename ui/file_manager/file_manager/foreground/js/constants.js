// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for common constnats used in Files app.
 * @namespace
 */
const constants = {};

/**
 * @const {!Array<string>}
 */
constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES = ['hosted', 'pinned'];

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
  'shared',
  'size',
  'canCopy',
  'canDelete',
  'canRename',
  'canAddChildren',
  'canShare',
  'isMachineRoot',
  'isExternalMedia',
  'isArbitrarySyncFolder',
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
 * Name of the default crostini VM.
 * @const
 */
constants.DEFAULT_CROSTINI_VM = 'termina';

/**
 * Name of the Plugin VM.
 * @const
 */
constants.PLUGIN_VM = 'PvmDefault';
