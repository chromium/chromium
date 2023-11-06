// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isSameEntry} from '../../common/js/entry_utils.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {fileOperationUtil} from './file_operation_util.js';

/**
 * FileOperationManagerImpl: implementation of {FileOperationManager}.
 *
 * @implements {FileOperationManager}
 */
export class FileOperationManagerImpl {
  constructor() {
    /**
     * @private @type {?VolumeManager}
     */
    this.volumeManager_ = null;

    /**
     * @private @type {number}
     */
    this.taskIdCounter_ = 0;
  }

  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FakeEntry} targetEntry The destination entry of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {!Promise<Array<Entry>>} Promise fulfilled with the filtered entry.
   *     This is not rejected.
   */
  async filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {
    if (!isMove) {
      return sourceEntries;
    }

    // Check all file entries and keeps only those need sharing operation.
    // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any' type.
    const processEntry = entry => {
      return new Promise(resolve => {
        entry.getParent(
            // @ts-ignore: error TS7006: Parameter 'inParentEntry' implicitly
            // has an 'any' type.
            inParentEntry => {
              if (!isSameEntry(inParentEntry, targetEntry)) {
                resolve(entry);
              } else {
                resolve(null);
              }
            },
            // @ts-ignore: error TS7006: Parameter 'error' implicitly has an
            // 'any' type.
            error => {
              console.warn(error.stack || error);
              resolve(null);
            });
      });
    };

    // Call processEntry for each item of sourceEntries.
    const result = await Promise.all(sourceEntries.map(processEntry));

    // Remove null entries.
    return result.filter(entry => !!entry);
  }

  /**
   * Writes file to destination dir. This function is called when an image is
   * dragged from a web page. In this case there is no FileSystem Entry to copy
   * or move, just the JS File object with attached Blob. This operation does
   * not use EventRouter or queue the task since it is not possible to track
   * progress of the FileWriter.write().
   *
   * @param {!File} file The file entry to be written.
   * @param {!DirectoryEntry} dir The destination directory to write to.
   * @return {!Promise<!FileEntry>}
   */
  async writeFile(file, dir) {
    const name = await fileOperationUtil.deduplicatePath(dir, file.name);
    return new Promise((resolve, reject) => {
      dir.getFile(name, {create: true, exclusive: true}, f => {
        f.createWriter(writer => {
          writer.onwriteend = () => resolve(f);
          writer.onerror = reject;
          writer.write(file);
        }, reject);
      }, reject);
    });
  }
}
