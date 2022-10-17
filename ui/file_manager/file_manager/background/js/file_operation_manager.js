// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {fileOperationUtil} from './file_operation_util.js';
import {Trash} from './trash.js';

/**
 * FileOperationManagerImpl: implementation of {FileOperationManager}.
 *
 * @implements {FileOperationManager}
 */
export class FileOperationManagerImpl {
  constructor() {
    /**
     * TODO(crbug.com/953256) Add closure annotation.
     * @private
     */
    this.fileManager_ = null;

    /**
     * @private {VolumeManager}
     */
    this.volumeManager_ = null;

    /**
     * @private {number}
     */
    this.taskIdCounter_ = 0;

    /**
     * @private {!Trash}
     * @const
     */
    this.trash_ = new Trash();
  }

  /**
   * Store a reference to our owning File Manager.
   * @param {Object} fileManager reference to the 'foreground' app.
   */
  setFileManager(fileManager) {
    this.fileManager_ = fileManager;
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
    const processEntry = entry => {
      return new Promise(resolve => {
        entry.getParent(
            inParentEntry => {
              if (!util.isSameEntry(inParentEntry, targetEntry)) {
                resolve(entry);
              } else {
                resolve(null);
              }
            },
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
   * Returns true if all entries will use trash for delete.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries.
   * @return {boolean}
   */
  willUseTrash(volumeManager, entries) {
    return entries.every(
        entry => this.trash_.shouldMoveToTrash(volumeManager, entry));
  }

  /**
   * Notifies File Manager that an extraction operation has finished.
   *
   * @param {number} taskId The unique task id for the IO operation.
   * @suppress {missingProperties}
   */
  notifyExtractDone(taskId) {
    // TODO(crbug.com/953256) Add closure annotation.
    // taskController is set asynchronously, this can be called on startup
    // if another SWA window is finishing an extract (crbug.com/1348432).
    if (this.fileManager_.taskController) {
      this.fileManager_.taskController.deleteExtractTaskDetails(taskId);
    }
  }

  /**
   * Called when an IOTask finished with a NEED_PASSWORD status.
   * Delegate it to the task controller to deal with it.
   *
   * @param {number} taskId The unique task id for the IO operation.
   * @suppress {missingProperties}
   */
  handleMissingPassword(taskId) {
    // TODO(crbug.com/953256) Add closure annotation.
    // null check is unlikely to be needed, but there's no guarantee
    // that taskController has been initialized on a password event.
    if (this.fileManager_.taskController) {
      this.fileManager_.taskController.handleMissingPassword(taskId);
    }
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
