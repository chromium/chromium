// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeEntry, FilesAppEntry} from '../files_app_entry_interfaces.js';
import {VolumeManager} from '../volume_manager.js';

/**
 * FileOperationManager: manager of file operations.
 * @interface
 */
export class FileOperationManager {
  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FakeEntry} targetEntry The destination entry of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {Promise} Promise fulfilled with the filtered entry. This is not
   *     rejected.
   */
  filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {}

  /**
   * Writes file to destination dir.
   *
   * @param {!File} file The file entry to be written.
   * @param {!DirectoryEntry} destination The destination dir.
   * @return {!Promise<!FileEntry>}
   */
  async writeFile(file, destination) {}
}
