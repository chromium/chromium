// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileErrorToDomError, FileOperationErrorType} from '../../common/js/util.js';

/**
 * Utilities for file operations.
 */
const fileOperationUtil = {};

/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 */
export class FileOperationError {
  /**
   * @param {FileOperationErrorType} code Error type.
   * @param {string|Entry|DOMError} data Additional data.
   */
  constructor(code, data) {
    this.code = code;
    this.data = data;
  }
}

/**
 * Resolves a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param {DirectoryEntry} root The root of the filesystem to search.
 * @param {string} path The path to be resolved.
 * @return {!Promise<DirectoryEntry|FileEntry>} Promise fulfilled with the
 *     resolved entry, or rejected with FileError.
 */
fileOperationUtil.resolvePath = (root, path) => {
  if (path === '' || path === '/') {
    return Promise.resolve(root);
  }
  return new Promise(root.getFile.bind(root, path, {create: false}))
      .catch(error => {
        if (error.name === FileErrorToDomError.TYPE_MISMATCH_ERR) {
          // Bah.  It's a directory, ask again.
          return new Promise(
              root.getDirectory.bind(root, path, {create: false}));
        } else {
          return Promise.reject(error);
        }
      });
};

/**
 * Checks if an entry exists at |relativePath| in |dirEntry|.
 * If exists, tries to deduplicate the path by inserting parenthesized number,
 * such as " (1)", before the extension. If it still exists, tries the
 * deduplication again by increasing the number.
 * For example, suppose "file.txt" is given, "file.txt", "file (1).txt",
 * "file (2).txt", ... will be tried.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)=} opt_successCallback Callback run with the
 *     deduplicated path on success.
 * @param {function(FileOperationError)=} opt_errorCallback
 *     Callback run on error.
 * @return {!Promise<string>} Promise fulfilled with available path.
 */
fileOperationUtil.deduplicatePath =
    (dirEntry, relativePath, opt_successCallback, opt_errorCallback) => {
      // Crack the path into three part. The parenthesized number (if exists)
      // will be replaced by incremented number for retry. For example, suppose
      // |relativePath| is "file (10).txt", the second check path will be
      // "file (11).txt".
      const match = /^(.*?)(?: \((\d+)\))?(\.[^.]*?)?$/.exec(relativePath);
      // @ts-ignore: error TS18047: 'match' is possibly 'null'.
      const prefix = match[1];
      // @ts-ignore: error TS18047: 'match' is possibly 'null'.
      const ext = match[3] || '';

      // Check to see if the target exists.
      // @ts-ignore: error TS7006: Parameter 'copyNumber' implicitly has an
      // 'any' type.
      const resolvePath = (trialPath, copyNumber) => {
        return fileOperationUtil.resolvePath(dirEntry, trialPath)
            .then(
                () => {
                  const newTrialPath = prefix + ' (' + copyNumber + ')' + ext;
                  return resolvePath(newTrialPath, copyNumber + 1);
                },
                error => {
                  // We expect to be unable to resolve the target file, since
                  // we're going to create it during the copy.  However, if the
                  // resolve fails with anything other than NOT_FOUND, that's
                  // trouble.
                  if (error.name === FileErrorToDomError.NOT_FOUND_ERR) {
                    return trialPath;
                  } else {
                    return Promise.reject(error);
                  }
                });
      };

      // @ts-ignore: error TS7006: Parameter 'error' implicitly has an 'any'
      // type.
      const promise = resolvePath(relativePath, 1).catch(error => {
        if (error instanceof Error) {
          return Promise.reject(error);
        }
        return Promise.reject(new FileOperationError(
            FileOperationErrorType.FILESYSTEM_ERROR, error));
      });
      if (opt_successCallback) {
        promise.then(opt_successCallback, opt_errorCallback);
      }
      return promise;
    };

export {fileOperationUtil};
