// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeEntry, FilesAppEntry} from '../files_app_entry_interfaces.js';
import {VolumeManager} from '../volume_manager.js';

/**
 * FileOperationManager: manager of file operations. Implementations of this
 * interface must @extends {cr.EventTarget} or implement the EventTarget API on
 * their own.
 *
 * @interface
 */
export class FileOperationManager extends EventTarget {
  /**
   * Store a reference to our owning File Manager.
   * @param {Object} fileManager reference to the 'foreground' app.
   */
  setFileManager(fileManager) {}

  /**
   * Says if there are any tasks in the queue.
   * @return {boolean} True, if there are any tasks.
   */
  hasQueuedTasks() {}

  /**
   * Requests the specified task to be canceled.
   * @param {string} taskId ID of task to be canceled.
   */
  requestTaskCancel(taskId) {}

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
   * Kick off pasting.
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry} targetEntry The destination entry of the target
   *     directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @param {string=} opt_taskId If the corresponding item has already created
   *     at another places, we need to specify the ID of the item. If the
   *     item is not created, FileOperationManager generates new ID.
   */
  paste(sourceEntries, targetEntry, isMove, opt_taskId) {}

  /**
   * Returns true if all entries will use trash for delete.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries.
   * @return {boolean}
   */
  willUseTrash(volumeManager, entries) {}

  /**
   * Schedules the files deletion.
   *
   * @param {!Array<!Entry>} entries The entries.
   * @param {boolean=} permanentlyDelete if true, entries will be deleted rather
   *     than moved to trash.
   */
  deleteEntries(entries, permanentlyDelete = false) {}

  /**
   * Schedules the files to be restored.
   *
   * @param {!Array<!FilesAppEntry>} entries The trash entries.
   */
  restoreDeleted(entries) {}

  /**
   * Schedules the Trash to be emptied.
   */
  emptyTrash() {}

  /**
   * Creates a zip file for the selection of files.
   *
   * @param {!Array<!Entry>} selectionEntries The selected entries.
   * @param {!DirectoryEntry} dirEntry The directory containing the selection.
   */
  zipSelection(selectionEntries, dirEntry) {}

  /**
   * Notifies File Manager that an extraction operation has finished.
   *
   * @param {number} taskId The unique task id for the IO operation.
   */
  notifyExtractDone(taskId) {}

  /**
   * Called when an IOTask finished with a NEED_PASSWORD status.
   * Delegate it to the task controller to deal with it.
   *
   * @param {number} taskId The unique task id for the IO operation.
   */
  handleMissingPassword(taskId) {}

  /**
   * Writes file to destination dir.
   *
   * @param {!File} file The file entry to be written.
   * @param {!DirectoryEntry} destination The destination dir.
   * @return {!Promise<!FileEntry>}
   */
  async writeFile(file, destination) {}

  /**
   * Generates new task ID.
   *
   * @return {string} New task ID.
   */
  generateTaskId() {}
}
