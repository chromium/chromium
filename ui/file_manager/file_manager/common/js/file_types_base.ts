// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EXTENSION_TO_TYPE, type FileExtensionType} from './file_types_data.js';

/**
 * A special placeholder for unknown types with no extension.
 */
const PLACEHOLDER: FileExtensionType = {
  translationKey: 'NO_EXTENSION_FILE_TYPE',
  type: 'UNKNOWN',
  icon: '',
  subtype: '',
  extensions: undefined,
  mime: undefined,
  encrypted: undefined,
  originalMimeType: undefined,
};

/**
 * Returns the final extension of a file name, check for the last two dots
 * to distinguish extensions like ".tar.gz" and ".gz".
 */
export function getFinalExtension(fileName: string): string {
  if (!fileName) {
    return '';
  }
  const lowerCaseFileName = fileName.toLowerCase();
  const parts = lowerCaseFileName.split('.');
  // No dot, so no extension.
  if (parts.length === 1) {
    return '';
  }
  // Only one dot, so only 1 extension.
  if (parts.length === 2) {
    return `.${parts.pop()}`;
  }
  // More than 1 dot/extension: e.g. ".tar.gz".
  const last = `.${parts.pop()}`;
  const secondLast = `.${parts.pop()}`;
  const doubleExtension = `${secondLast}${last}`;
  if (EXTENSION_TO_TYPE.has(doubleExtension)) {
    return doubleExtension;
  }
  // Double extension doesn't exist in the map, return the single one.
  return last;
}

/**
 * Gets the file type object for a given file name (base name). Use getType()
 * if possible, since this method can't recognize directories.
 */
export function getFileTypeForName(name: string): FileExtensionType {
  const extension = getFinalExtension(name);
  if (EXTENSION_TO_TYPE.has(extension)) {
    return EXTENSION_TO_TYPE.get(extension) as FileExtensionType;
  }

  // Unknown file type.
  if (extension === '') {
    return PLACEHOLDER;
  }

  // subtype is the extension excluding the first dot.
  return {
    translationKey: 'GENERIC_FILE_TYPE',
    type: 'UNKNOWN',
    subtype: extension.substr(1).toUpperCase(),
    icon: '',
    extensions: undefined,
    mime: undefined,
    encrypted: undefined,
    originalMimeType: undefined,
  };
}
