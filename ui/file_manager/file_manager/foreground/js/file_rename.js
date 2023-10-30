// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain renaming utility functions used only
 * by the files app frontend.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';

import {getEntry, getParentEntry, moveEntryTo, validatePathNameLength} from '../../common/js/api.js';
import {createDOMError} from '../../common/js/dom_utils.js';
import {getFileErrorString, str, strf} from '../../common/js/translations.js';
import {FileErrorToDomError} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

/**
 * Verifies name for file, folder, or removable root to be created or renamed.
 * Names are restricted according to the target filesystem.
 *
 * @param {!Entry} entry The entry to be named.
 * @param {string} name New file, folder, or removable root name.
 * @param {boolean} areHiddenFilesVisible Whether to report hidden file
 *     name errors or not.
 * @param {?import("../../externs/volume_info.js").VolumeInfo} volumeInfo Volume
 *     information about the target entry.
 * @param {boolean} isRemovableRoot Whether the target is a removable root.
 * @return {!Promise<void>} Fulfills on success, throws error message otherwise.
 */
export async function validateEntryName(
    entry, name, areHiddenFilesVisible, volumeInfo, isRemovableRoot) {
  if (isRemovableRoot) {
    const diskFileSystemType = volumeInfo && volumeInfo.diskFileSystemType;
    // @ts-ignore: error TS2345: Argument of type 'string | null' is not
    // assignable to parameter of type 'string'.
    validateExternalDriveName(name, assert(diskFileSystemType));
  } else {
    const parentEntry = await getParentEntry(entry);
    await validateFileName(parentEntry, name, areHiddenFilesVisible);
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
  // Verify if entered name for external drive respects restrictions
  // provided by the target filesystem.

  const nameLength = name.length;
  const lengthLimit = VolumeManagerCommon.FileSystemTypeVolumeNameLengthLimit;

  // Verify length for the target file system type.
  if (lengthLimit.hasOwnProperty(fileSystem) &&
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{ vfat:
      // number; exfat: number; ntfs: number; }'.
      nameLength > lengthLimit[fileSystem]) {
    throw Error(
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{
        // vfat: number; exfat: number; ntfs: number; }'.
        strf('ERROR_EXTERNAL_DRIVE_LONG_NAME', lengthLimit[fileSystem]));
  }

  // Checks if the name contains only alphanumeric characters or allowed
  // special characters. This needs to stay in sync with
  // cros-disks/filesystem_label.cc on the ChromeOS side.
  const validCharRegex = /[a-zA-Z0-9 \!\#\$\%\&\(\)\-\@\^\_\`\{\}\~]/;
  for (let i = 0; i < nameLength; i++) {
    // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
    // assignable to parameter of type 'string'.
    if (!validCharRegex.test(name[i])) {
      throw Error(strf('ERROR_EXTERNAL_DRIVE_INVALID_CHARACTER', name[i]));
    }
  }
}

/**
 * Verifies the user entered name for file or folder to be created or
 * renamed to. Name restrictions must correspond to File API restrictions
 * (see DOMFilePath::isValidPath). Curernt WebKit implementation is
 * out of date (spec is
 * http://dev.w3.org/2009/dap/file-system/file-dir-sys.html, 8.3) and going
 * to be fixed. Shows message box if the name is invalid.
 *
 * It also verifies if the name length is in the limit of the filesystem.
 *
 * @param {!DirectoryEntry} parentEntry The entry of the parent directory.
 * @param {string} name New file or folder name.
 * @param {boolean} areHiddenFilesVisible Whether to report the hidden file
 *     name error or not.
 * @return {!Promise<void>} Fulfills on success, throws error message otherwise.
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
 * Renames file, folder, or removable root with newName.
 * @param {!Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @param {?import("../../externs/volume_info.js").VolumeInfo} volumeInfo Volume
 *     information about the target entry.
 * @param {boolean} isRemovableRoot Whether the target is a removable root.
 * @return {!Promise<!Entry>} Resolves the renamed entry if successful, else
 * throws error message.
 */
export async function renameEntry(entry, newName, volumeInfo, isRemovableRoot) {
  if (isRemovableRoot) {
    // @ts-ignore: error TS18047: 'volumeInfo' is possibly 'null'.
    chrome.fileManagerPrivate.renameVolume(volumeInfo.volumeId, newName);
    return entry;
  }
  return renameFile(entry, newName);
}

/**
 * Renames the entry to newName.
 * @param {!Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @return {!Promise<!Entry>} Resolves the renamed entry if successful, else
 * throws error message.
 */
export async function renameFile(entry, newName) {
  try {
    // Before moving, we need to check if there is an existing entry at
    // parent/newName, since moveTo will overwrite it.
    // Note that this way has a race condition. After existing check,
    // a new entry may be created in the background. However, there is no way
    // not to overwrite the existing file, unfortunately. The risk should be
    // low, assuming the unsafe period is very short.

    const parent = await getParentEntry(entry);

    try {
      await getEntry(parent, newName, entry.isFile, {create: false});
    } catch (error) {
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
      if (error.name == FileErrorToDomError.NOT_FOUND_ERR) {
        return moveEntryTo(entry, parent, newName);
      }

      // Unexpected error found.
      throw error;
    }

    // The entry with the name already exists.
    throw createDOMError(FileErrorToDomError.PATH_EXISTS_ERR);
  } catch (error) {
    // @ts-ignore: error TS2345: Argument of type 'unknown' is not assignable to
    // parameter of type 'DOMError'.
    throw getRenameErrorMessage(error, entry, newName);
  }
}

/**
 * Converts DOMError response from renameEntry() to error message.
 * @param {DOMError} error
 * @param {!Entry} entry
 * @param {string} newName
 * @return {!Error}
 */
function getRenameErrorMessage(error, entry, newName) {
  if (error &&
      (error.name == FileErrorToDomError.PATH_EXISTS_ERR ||
       error.name == FileErrorToDomError.TYPE_MISMATCH_ERR)) {
    // Check the existing entry is file or not.
    // 1) If the entry is a file:
    //   a) If we get PATH_EXISTS_ERR, a file exists.
    //   b) If we get TYPE_MISMATCH_ERR, a directory exists.
    // 2) If the entry is a directory:
    //   a) If we get PATH_EXISTS_ERR, a directory exists.
    //   b) If we get TYPE_MISMATCH_ERR, a file exists.
    return Error(strf(
        (entry.isFile && error.name == FileErrorToDomError.PATH_EXISTS_ERR) ||
                (!entry.isFile &&
                 error.name == FileErrorToDomError.TYPE_MISMATCH_ERR) ?
            'FILE_ALREADY_EXISTS' :
            'DIRECTORY_ALREADY_EXISTS',
        newName));
  }

  return Error(
      strf('ERROR_RENAMING', entry.name, getFileErrorString(error.name)));
}
