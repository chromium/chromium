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
