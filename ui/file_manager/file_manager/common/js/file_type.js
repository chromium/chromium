// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {FileData} from '../../externs/ts/state.js';

import {getFileTypeForName, getFinalExtension} from './file_types_base.js';
import {EXTENSION_TO_TYPE, FileExtensionType, MIME_TO_TYPE} from './file_types_data.js';
import {VolumeEntry} from './files_app_entry_types.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Namespace object for file type utility functions.
 */
export function FileType() {}

export {FileExtensionType};

// All supported file types are now defined in
// ui/file_manager/base/gn/file_types.json5.

/**
 * A special type for directory.
 * @type{!FileExtensionType}
 * @const
 */
FileType.DIRECTORY = {
  translationKey: 'FOLDER',
  type: '.folder',
  icon: 'folder',
  subtype: '',
};

/**
 * Returns the file path extension for a given file.
 *
 * @param {Entry|FilesAppEntry} entry Reference to the file.
 * @return {string} The extension including a leading '.', or empty string if
 *     not found.
 */
FileType.getExtension = entry => {
  // No extension for a directory.
  if (entry.isDirectory) {
    return '';
  }

  return getFinalExtension(entry.name);
};

/**
 * Gets the file type object for a given entry. If mime type is provided, then
 * uses it with higher priority than the extension.
 *
 * @param {(Entry|FilesAppEntry)} entry Reference to the entry.
 * @param {string=} opt_mimeType Optional mime type for the entry.
 * @return {!FileExtensionType} The matching descriptor or a placeholder.
 */
FileType.getType = (entry, opt_mimeType) => {
  if (entry.isDirectory) {
    // For removable partitions, use the file system type.
    if (/** @type {VolumeEntry}*/ (entry).volumeInfo &&
        /** @type {VolumeEntry}*/ (entry).volumeInfo.diskFileSystemType) {
      return {
        translationKey: '',
        type: 'partition',
        subtype: assert(
            /** @type {VolumeEntry}*/ (entry).volumeInfo.diskFileSystemType),
        icon: '',
      };
    }
    return FileType.DIRECTORY;
  }

  if (opt_mimeType) {
    const cseMatch = opt_mimeType.match(
        /^application\/vnd.google-gsuite.encrypted; content="([a-z\/.-]+)"$/);
    if (cseMatch) {
      const type = /** @type {FileExtensionType} */ (
          {...FileType.getType(entry, cseMatch[1])});
      type.encrypted = true;
      type.originalMimeType = cseMatch[1];
      return type;
    }
  }

  if (opt_mimeType && MIME_TO_TYPE.has(opt_mimeType)) {
    return MIME_TO_TYPE.get(opt_mimeType);
  }

  return getFileTypeForName(entry.name);
};

/**
 * Gets the media type for a given file.
 *
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {string} The value of 'type' property from one of the elements in
 *     the knows file types (file_types.json5) or undefined.
 */
FileType.getMediaType = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).type;
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if audio file.
 */
FileType.isAudio = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'audio';
};

/**
 * Returns whether the |entry| is image file that can be opened in browser.
 * Note that it returns false for RAW images.
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if image file.
 */
FileType.isImage = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'image';
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if video file.
 */
FileType.isVideo = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'video';
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if document file.
 */
FileType.isDocument = (entry, opt_mimeType) => {
  const type = FileType.getMediaType(entry, opt_mimeType);
  return type === 'document' || type === 'hosted' || type === 'text';
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if raw file.
 */
FileType.isRaw = (entry, opt_mimeType) => {
  return FileType.getMediaType(entry, opt_mimeType) === 'raw';
};

/**
 * @param {Entry} entry Reference to the file
 * @param {string=} opt_mimeType Optional mime type for this file.
 * @return {boolean} Whether or not this is a PDF file.
 */
FileType.isPDF = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).subtype === 'PDF';
};

/**
 * Files with more pixels won't have preview.
 * @param {!Array<string>} types
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} True if type is in specified set
 */
FileType.isType = (types, entry, opt_mimeType) => {
  const type = FileType.getMediaType(entry, opt_mimeType);
  return !!type && types.indexOf(type) !== -1;
};

/**
 * @param {Entry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} Returns true if the file is hosted.
 */
FileType.isHosted = (entry, opt_mimeType) => {
  return FileType.getType(entry, opt_mimeType).type === 'hosted';
};

/**
 * @param {Entry|FilesAppEntry} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {boolean} Returns true if the file is encrypted with CSE.
 */
FileType.isEncrypted = (entry, opt_mimeType) => {
  const type = FileType.getType(entry, opt_mimeType);
  return type.encrypted !== undefined && type.encrypted;
};

/**
 * @param {Entry|VolumeEntry|FileData} entry Reference to the file.
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @param {VolumeManagerCommon.RootType=} opt_rootType The root type of the
 *     entry.
 * @return {string} Returns string that represents the file icon.
 *     It refers to a file 'images/filetype_' + icon + '.png'.
 */
FileType.getIcon = (entry, opt_mimeType, opt_rootType) => {
  let icon;
  // Handles the FileData and FilesAppEntry types.
  if (entry && entry.iconName) {
    return entry.iconName;
  }
  // Handles other types of entries.
  if (entry) {
    entry = /** @type {!Entry|!VolumeEntry} */ (entry);
    const fileType = FileType.getType(entry, opt_mimeType);
    const overridenIcon = FileType.getIconOverrides(entry, opt_rootType);
    icon = overridenIcon || fileType.icon || fileType.type;
  }
  return icon || 'unknown';
};

/**
 * Returns a string to be used as an attribute value to customize the entry
 * icon.
 *
 * @param {Entry|FilesAppEntry} entry
 * @param {VolumeManagerCommon.RootType=} opt_rootType The root type of the
 *     entry.
 * @return {string}
 */
FileType.getIconOverrides = (entry, opt_rootType) => {
  // Overrides per RootType and defined by fullPath.
  const overrides = {
    [VolumeManagerCommon.RootType.DOWNLOADS]: {
      '/Camera': 'camera-folder',
      '/Downloads': VolumeManagerCommon.VolumeType.DOWNLOADS,
      '/PvmDefault': 'plugin_vm',
    },
  };
  const root = overrides[opt_rootType];
  return root ? root[entry.fullPath] : '';
};
