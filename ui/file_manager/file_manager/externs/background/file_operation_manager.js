// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppDirEntry} from '../files_app_entry_interfaces.js';

/**
 * FileOperationManager: manager of file operations.
 * @interface
 */
export class FileOperationManager {
  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FilesAppDirEntry} targetEntry The destination entry
of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {Promise<!Entry[]>} Promise fulfilled with the filtered entry. This
is not
   *     rejected.
   */
  // @ts-ignore: error TS6133: 'isMove' is declared but its value is never read.
  filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {
    return Promise.resolve([]);
  }

  /**
   * Writes file to destination dir.
   *
   * @param {!File} file The file entry to be written.
   * @param {!DirectoryEntry} destination The destination dir.
   * @return {!Promise<!FileEntry>}
   */
  // @ts-ignore: error TS6133: 'destination' is declared but its value is never
  // read.
  async writeFile(file, destination) {
    return /** @type {FileEntry} */ ({});
  }
}
