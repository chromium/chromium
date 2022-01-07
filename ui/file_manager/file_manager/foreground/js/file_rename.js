// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain renaming utility functions used only
 * by the files app frontend.
 */

import {getEntry, getParentEntry, moveEntryTo, validatePathNameLength} from '../../common/js/api.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

/**
 * Renames the entry to newName.
 * @param {!Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @return {Promise<!Entry>} The renamed entry.
 */
export async function renameEntry(entry, newName) {
  // Before moving, we need to check if there is an existing entry at
  // parent/newName, since moveTo will overwrite it.
  // Note that this way has some timing issue. After existing check,
  // a new entry may be created in the background. However, there is no way not
  // to overwrite the existing file, unfortunately. The risk should be low,
  // assuming the unsafe period is very short.

  const parent = await getParentEntry(entry);

  try {
    await getEntry(parent, newName, entry.isFile, {create: false});
  } catch (error) {
    if (error.name == util.FileError.NOT_FOUND_ERR) {
      return moveEntryTo(entry, parent, newName);
    }

    // Unexpected error found.
    throw error;
  }

  // The entry with the name already exists.
  throw util.createDOMError(util.FileError.PATH_EXISTS_ERR);
}

/**
 * Converts DOMError response from renameEntry() to error message
 * @param {DOMError} error
 * @param {!Entry} entry
 * @param {string} newName
 * @return {string}
 */
export function getRenameErrorMessage(error, entry, newName) {
  if (error &&
      (error.name == util.FileError.PATH_EXISTS_ERR ||
       error.name == util.FileError.TYPE_MISMATCH_ERR)) {
    // Check the existing entry is file or not.
    // 1) If the entry is a file:
    //   a) If we get PATH_EXISTS_ERR, a file exists.
    //   b) If we get TYPE_MISMATCH_ERR, a directory exists.
    // 2) If the entry is a directory:
    //   a) If we get PATH_EXISTS_ERR, a directory exists.
    //   b) If we get TYPE_MISMATCH_ERR, a file exists.
    return strf(
        (entry.isFile && error.name == util.FileError.PATH_EXISTS_ERR) ||
                (!entry.isFile &&
                 error.name == util.FileError.TYPE_MISMATCH_ERR) ?
            'FILE_ALREADY_EXISTS' :
            'DIRECTORY_ALREADY_EXISTS',
        newName);
  }

  return strf(
      'ERROR_RENAMING', entry.name, util.getFileErrorString(error.name));
}

/**
 * Verifies the user entered name for file or folder to be created or
 * renamed to. Name restrictions must correspond to File API restrictions
 * (see DOMFilePath::isValidPath). Curernt WebKit implementation is
 * out of date (spec is
 * http://dev.w3.org/2009/dap/file-system/file-dir-sys.html, 8.3) and going to
 * be fixed. Shows message box if the name is invalid.
 *
 * It also verifies if the name length is in the limit of the filesystem.
 *
 * @param {!DirectoryEntry} parentEntry The entry of the parent directory.
 * @param {string} name New file or folder name.
 * @param {boolean} areHiddenFilesVisible Whether to report the hidden file name
 *     error or not.
 * @return {Promise} Fulfills on success, throws error message otherwise.
 */
export async function validateFileName(
    parentEntry, name, areHiddenFilesVisible) {
  const testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
  if (testResult) {
    throw Error(strf('ERROR_INVALID_CHARACTER', testResult[0]));
  }
  if (/^\s*$/i.test(name)) {
    throw Error(str('ERROR_WHITESPACE_NAME'));
  }
  if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name)) {
    throw Error(str('ERROR_RESERVED_NAME'));
  }
  if (!areHiddenFilesVisible && /\.crdownload$/i.test(name)) {
    throw Error(str('ERROR_RESERVED_NAME'));
  }
  if (!areHiddenFilesVisible && name[0] == '.') {
    throw Error(str('ERROR_HIDDEN_NAME'));
  }

  const isValid = await validatePathNameLength(parentEntry, name);
  if (!isValid) {
    throw Error(str('ERROR_LONG_NAME'));
  }
}

/**
 * Verifies the user entered name for external drive to be
 * renamed to. Name restrictions must correspond to the target filesystem
 * restrictions.
 *
 * It also verifies that name length is in the limits of the filesystem.
 *
 * This function throws if the new label is invalid, else it completes.
 *
 * @param {string} name New external drive name.
 * @param {!VolumeManagerCommon.FileSystemType} fileSystem
 */
export function validateExternalDriveName(name, fileSystem) {
  // Verify if entered name for external drive respects restrictions provided by
  // the target filesystem

  const nameLength = name.length;
  const lengthLimit = VolumeManagerCommon.FileSystemTypeVolumeNameLengthLimit;

  // Verify length for the target file system type
  if (lengthLimit.hasOwnProperty(fileSystem) &&
      nameLength > lengthLimit[fileSystem]) {
    throw Error(
        strf('ERROR_EXTERNAL_DRIVE_LONG_NAME', lengthLimit[fileSystem]));
  }

  // Checks if the name contains only alphanumeric characters or allowed
  // special characters. This needs to stay in sync with
  // cros-disks/filesystem_label.cc on the ChromeOS side.
  const validCharRegex = /[a-zA-Z0-9 \!\#\$\%\&\(\)\-\@\^\_\`\{\}\~]/;
  for (let i = 0; i < nameLength; i++) {
    if (!validCharRegex.test(name[i])) {
      throw Error(strf('ERROR_EXTERNAL_DRIVE_INVALID_CHARACTER', name[i]));
    }
  }
}
