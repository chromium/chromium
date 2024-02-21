// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain renaming utility functions used only
 * by the files app frontend.
 */

import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {getEntry, getParentEntry, moveEntryTo, validatePathNameLength} from '../../common/js/api.js';
import {createDOMError} from '../../common/js/dom_utils.js';
import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {getFileErrorString, str, strf} from '../../common/js/translations.js';
import {FileErrorToDomError} from '../../common/js/util.js';
import type {FileSystemType} from '../../common/js/volume_manager_types.js';
import {FileSystemTypeVolumeNameLengthLimit} from '../../common/js/volume_manager_types.js';

/**
 * Verifies name for file, folder, or removable root to be created or renamed.
 * Names are restricted according to the target filesystem.
 *
 * @param entry The entry to be named.
 * @param name New file, folder, or removable root name.
 * @param areHiddenFilesVisible Whether to report hidden file name errors or
 *     not.
 * @param volumeInfo Volume information about the target entry.
 * @param isRemovableRoot Whether the target is a removable root.
 * @return Fulfills on success, throws error message otherwise.
 */
export async function validateEntryName(
    entry: Entry|FilesAppEntry, name: string, areHiddenFilesVisible: boolean,
    volumeInfo: null|VolumeInfo, isRemovableRoot: boolean) {
  if (isRemovableRoot) {
    const diskFileSystemType = volumeInfo && volumeInfo.diskFileSystemType;
    assert(diskFileSystemType);
    validateExternalDriveName(name, diskFileSystemType);
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
 * @param name New external drive name.
 */
export function validateExternalDriveName(
    name: string, fileSystem: FileSystemType) {
  // Verify if entered name for external drive respects restrictions
  // provided by the target filesystem.

  const nameLength = name.length;
  const lengthLimit = FileSystemTypeVolumeNameLengthLimit;

  // Verify length for the target file system type.
  if (lengthLimit.hasOwnProperty(fileSystem) &&
      nameLength > lengthLimit[fileSystem]!) {
    throw Error(
        strf('ERROR_EXTERNAL_DRIVE_LONG_NAME', lengthLimit[fileSystem]));
  }

  // Checks if the name contains only alphanumeric characters or allowed
  // special characters. This needs to stay in sync with
  // cros-disks/filesystem_label.cc on the ChromeOS side.
  const validCharRegex = /[a-zA-Z0-9 \!\#\$\%\&\(\)\-\@\^\_\`\{\}\~]/;
  for (const n of name) {
    if (!validCharRegex.test(n)) {
      throw Error(strf('ERROR_EXTERNAL_DRIVE_INVALID_CHARACTER', n));
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
 * @param parentEntry The entry of the parent directory.
 * @param name New file or folder name.
 * @param areHiddenFilesVisible Whether to report the hidden file name error or
 *     not.
 * @return Fulfills on success, throws error message otherwise.
 */
export async function validateFileName(
    parentEntry: FilesAppDirEntry|DirectoryEntry, name: string,
    areHiddenFilesVisible: boolean) {
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
  if (!areHiddenFilesVisible && name[0] === '.') {
    throw Error(str('ERROR_HIDDEN_NAME'));
  }

  const isValid = await validatePathNameLength(parentEntry, name);
  if (!isValid) {
    throw Error(str('ERROR_LONG_NAME'));
  }
}

/**
 * Renames file, folder, or removable root with newName.
 * @param entry The entry to be renamed.
 * @param newName The new name.
 * @param volumeInfo Volume information about the target entry.
 * @param isRemovableRoot Whether the target is a removable root.
 * @return Resolves the renamed entry if successful, else throws error message.
 */
export async function renameEntry(
    entry: Entry|FilesAppEntry, newName: string, volumeInfo: null|VolumeInfo,
    isRemovableRoot: boolean): Promise<Entry|FilesAppEntry> {
  if (isRemovableRoot) {
    chrome.fileManagerPrivate.renameVolume(volumeInfo!.volumeId, newName);
    return entry;
  }
  return renameFile(entry, newName);
}

/**
 * Renames the entry to newName.
 * @param entry The entry to be renamed.
 * @param newName The new name.
 * @return Resolves the renamed entry if successful, else throws error message.
 */
export async function renameFile(
    entry: Entry|FilesAppEntry, newName: string): Promise<Entry|FilesAppEntry> {
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
    } catch (error: any) {
      if (error.name === FileErrorToDomError.NOT_FOUND_ERR) {
        return moveEntryTo(entry, parent, newName);
      }

      // Unexpected error found.
      throw error;
    }

    // The entry with the name already exists.
    throw createDOMError(FileErrorToDomError.PATH_EXISTS_ERR);
  } catch (error: any) {
    throw getRenameErrorMessage(error, entry, newName);
  }
}

/**
 * Converts DOMError response from renameEntry() to error message.
 */
function getRenameErrorMessage(
    error: DOMError, entry: Entry|FilesAppEntry, newName: string): Error {
  if (error &&
      (error.name === FileErrorToDomError.PATH_EXISTS_ERR ||
       error.name === FileErrorToDomError.TYPE_MISMATCH_ERR)) {
    // Check the existing entry is file or not.
    // 1) If the entry is a file:
    //   a) If we get PATH_EXISTS_ERR, a file exists.
    //   b) If we get TYPE_MISMATCH_ERR, a directory exists.
    // 2) If the entry is a directory:
    //   a) If we get PATH_EXISTS_ERR, a directory exists.
    //   b) If we get TYPE_MISMATCH_ERR, a file exists.
    return Error(strf(
        (entry.isFile && error.name === FileErrorToDomError.PATH_EXISTS_ERR) ||
                (!entry.isFile &&
                 error.name === FileErrorToDomError.TYPE_MISMATCH_ERR) ?
            'FILE_ALREADY_EXISTS' :
            'DIRECTORY_ALREADY_EXISTS',
        newName));
  }

  return Error(
      strf('ERROR_RENAMING', entry.name, getFileErrorString(error.name)));
}
