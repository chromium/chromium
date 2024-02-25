// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import type {FileData} from '../../state/state.js';

import {getFileTypeForName, getFinalExtension} from './file_types_base.js';
import {FileExtensionType, MIME_TO_TYPE} from './file_types_data.js';
import type {VolumeEntry} from './files_app_entry_types.js';
import {RootType, VolumeType} from './volume_manager_types.js';

export {FileExtensionType};

// All supported file types are now defined in
// ui/file_manager/base/gn/file_types.json5.

/** A special type for directory. */
const DIRECTORY = {
  translationKey: 'FOLDER',
  type: '.folder',
  icon: 'folder',
  subtype: '',
  extensions: undefined,
  mime: undefined,
  encrypted: undefined,
  originalMimeType: undefined,
};

/**
 * Returns the file path extension for a given file.
 *
 * @param entry Reference to the file.
 * @return The extension including a leading '.', or empty string if not found.
 */
export function getExtension(entry: Entry|FilesAppEntry): string {
  // No extension for a directory.
  if (entry.isDirectory) {
    return '';
  }

  return getFinalExtension(entry.name);
}

/**
 * Gets the file type object for a given entry. If mime type is provided, then
 * uses it with higher priority than the extension.
 *
 * @param entry Reference to the entry.
 * @param mimeType Optional mime type for the entry.
 * @return The matching descriptor or a placeholder.
 */
export function getType(
    entry: Entry|FilesAppEntry, mimeType?: string): FileExtensionType {
  if (entry.isDirectory) {
    const volumeInfo = (entry as VolumeEntry).volumeInfo;
    // For removable partitions, use the file system type.
    if (volumeInfo && volumeInfo.diskFileSystemType) {
      return {
        translationKey: '',
        type: 'partition',
        subtype: volumeInfo.diskFileSystemType,
        icon: '',
        extensions: undefined,
        mime: undefined,
        encrypted: undefined,
        originalMimeType: undefined,
      };
    }

    return DIRECTORY;
  }

  if (mimeType) {
    const cseMatch = mimeType.match(
        /^application\/vnd.google-gsuite.encrypted; content="([a-z\/.-]+)"$/);
    if (cseMatch) {
      const type = {...getType(entry, cseMatch[1])};
      type.encrypted = true;
      type.originalMimeType = cseMatch[1];
      return type;
    }
  }

  if (mimeType && MIME_TO_TYPE.has(mimeType)) {
    return MIME_TO_TYPE.get(mimeType) as FileExtensionType;
  }

  return getFileTypeForName(entry.name);
}

/**
 * Gets the media type for a given file.
 *
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return The value of 'type' property from one of the elements in the knows
 *     file types (file_types.json5) or undefined.
 */
export function getMediaType(
    entry: Entry|FilesAppEntry, mimeType?: string): string {
  return getType(entry, mimeType).type;
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if audio file.
 */
export function isAudio(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getMediaType(entry, mimeType) === 'audio';
}

/**
 * Returns whether the |entry| is image file that can be opened in browser.
 * Note that it returns false for RAW images.
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if image file.
 */
export function isImage(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getMediaType(entry, mimeType) === 'image';
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if video file.
 */
export function isVideo(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getMediaType(entry, mimeType) === 'video';
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if document file.
 */
export function isDocument(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  const type = getMediaType(entry, mimeType);
  return type === 'document' || type === 'hosted' || type === 'text';
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if raw file.
 */
export function isRaw(entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getMediaType(entry, mimeType) === 'raw';
}

/**
 * @param entry Reference to the file
 * @param mimeType Optional mime type for this file.
 * @return Whether or not this is a PDF file.
 */
export function isPDF(entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getType(entry, mimeType).subtype === 'PDF';
}

/**
 * Files with more pixels won't have preview.
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return True if type is in specified set.
 */
export function isType(
    types: string[], entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  const type = getMediaType(entry, mimeType);
  return !!type && types.indexOf(type) !== -1;
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return Returns true if the file is hosted.
 */
export function isHosted(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  return getType(entry, mimeType).type === 'hosted';
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @return Returns true if the file is encrypted with CSE.
 */
export function isEncrypted(
    entry: Entry|FilesAppEntry, mimeType?: string): boolean {
  const type = getType(entry, mimeType);
  return type.encrypted !== undefined && type.encrypted;
}

/**
 * @param entry Reference to the file.
 * @param mimeType Optional mime type for the file.
 * @param rootType The root type of the entry.
 * @return Returns string that represents the file icon. It refers to a file
 *     'images/filetype_' + icon + '.png'.
 */
export function getIcon(
    entry: Entry|FilesAppEntry|VolumeEntry|FileData, mimeType?: string,
    rootType?: RootType): string {
  // Handles the FileData and FilesAppEntry types.
  if (entry && 'iconName' in entry) {
    return entry.iconName;
  }

  let icon;
  // Handles other types of entries.
  if (entry) {
    const ventry = entry as Entry | FilesAppEntry;
    const fileType = getType(ventry, mimeType);
    const overridenIcon = getIconOverrides(ventry, rootType);
    icon = overridenIcon || fileType.icon || fileType.type;
  }
  return icon || 'unknown';
}

/**
 * Returns a string to be used as an attribute value to customize the entry
 * icon.
 *
 * @param rootType The root type of the entry.
 */
export function getIconOverrides(
    entry: Entry|FilesAppEntry, rootType?: RootType): string {
  if (!rootType) {
    return '';
  }

  // Overrides per RootType and defined by fullPath.
  const overrides: Partial<Record<RootType, Record<string, string>>> = {
    [RootType.DOWNLOADS]: {
      '/Camera': 'camera-folder',
      '/Downloads': VolumeType.DOWNLOADS,
      '/PvmDefault': 'plugin_vm',
    },
  };

  const root = overrides[rootType];
  if (!root) {
    return '';
  }

  return root[entry.fullPath] ?? '';
}
