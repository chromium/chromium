// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain renaming utility functions used only
 * by the files app frontend.
 */

import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

/**
 * Renames the entry to newName.
 * @param {Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @param {function(Entry)} successCallback Callback invoked when the rename
 *     is successfully done.
 * @param {function(DOMError)} errorCallback Callback invoked when an error
 *     is found.
 */
export function renameEntry(entry, newName, successCallback, errorCallback) {
  entry.getParent(parentEntry => {
    const parent = /** @type {!DirectoryEntry} */ (parentEntry);

    // Before moving, we need to check if there is an existing entry at
    // parent/newName, since moveTo will overwrite it.
    // Note that this way has some timing issue. After existing check,
    // a new entry may be create on background. However, there is no way not to
    // overwrite the existing file, unfortunately. The risk should be low,
    // assuming the unsafe period is very short.
    (entry.isFile ? parent.getFile : parent.getDirectory)
        .call(
            parent, newName, {create: false},
            entry => {
              // The entry with the name already exists.
              errorCallback(
                  util.createDOMError(util.FileError.PATH_EXISTS_ERR));
            },
            error => {
              if (error.name != util.FileError.NOT_FOUND_ERR) {
                // Unexpected error is found.
                errorCallback(error);
                return;
              }

              // No existing entry is found.
              entry.moveTo(parent, newName, successCallback, errorCallback);
            });
  }, errorCallback);
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
 * @param {boolean} filterHiddenOn Whether to report the hidden file name error
 *     or not.
 * @return {Promise} Promise fulfilled on success, or rejected with the error
 *     message.
 */
export function validateFileName(parentEntry, name, filterHiddenOn) {
  const testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
  if (testResult) {
    return Promise.reject(strf('ERROR_INVALID_CHARACTER', testResult[0]));
  } else if (/^\s*$/i.test(name)) {
    return Promise.reject(str('ERROR_WHITESPACE_NAME'));
  } else if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name)) {
    return Promise.reject(str('ERROR_RESERVED_NAME'));
  } else if (filterHiddenOn && /\.crdownload$/i.test(name)) {
    return Promise.reject(str('ERROR_RESERVED_NAME'));
  } else if (filterHiddenOn && name[0] == '.') {
    return Promise.reject(str('ERROR_HIDDEN_NAME'));
  }

  return new Promise((fulfill, reject) => {
    chrome.fileManagerPrivate.validatePathNameLength(
        parentEntry, name, valid => {
          if (valid) {
            fulfill(null);
          } else {
            reject(str('ERROR_LONG_NAME'));
          }
        });
  });
}

/**
 * Verifies the user entered name for external drive to be
 * renamed to. Name restrictions must correspond to the target filesystem
 * restrictions.
 *
 * It also verifies that name length is in the limits of the filesystem.
 *
 * @param {string} name New external drive name.
 * @param {!VolumeManagerCommon.FileSystemType} fileSystem
 * @return {Promise} Promise fulfilled on success, or rejected with the error
 *     message.
 */
export function validateExternalDriveName(name, fileSystem) {
  // Verify if entered name for external drive respects restrictions provided by
  // the target filesystem

  const nameLength = name.length;
  const lengthLimit = VolumeManagerCommon.FileSystemTypeVolumeNameLengthLimit;

  // Verify length for the target file system type
  if (lengthLimit.hasOwnProperty(fileSystem) &&
      nameLength > lengthLimit[fileSystem]) {
    return Promise.reject(
        strf('ERROR_EXTERNAL_DRIVE_LONG_NAME', lengthLimit[fileSystem]));
  }

  // Checks if the name contains only alphanumeric characters or allowed special
  // characters. This needs to stay in sync with cros-disks/filesystem_label.cc
  // on the ChromeOS side.
  const validCharRegex = /[a-zA-Z0-9 \!\#\$\%\&\(\)\-\@\^\_\`\{\}\~]/;
  for (let i = 0; i < nameLength; i++) {
    if (!validCharRegex.test(name[i])) {
      return Promise.reject(
          strf('ERROR_EXTERNAL_DRIVE_INVALID_CHARACTER', name[i]));
    }
  }

  return Promise.resolve();
}
