// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppDirEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {FileListModel} from './file_list_model.js';

/**
 * @returns {!FileFilter} fake for unittests.
 */
function createFakeFileFilter() {
  /**
   * FileFilter fake.
   */
  class FakeFileFilter extends EventTarget {
    /**
     * @param {Entry} entry File entry.
     * @return {boolean} True if the file should be shown.
     */
    filter(entry) {
      return true;
    }
  }

  const filter = /** @type {!Object} */ (new FakeFileFilter());
  return /** @type {!FileFilter} */ (filter);
}

/**
 * @returns {!DirectoryModel} fake for unittests.
 */
export function createFakeDirectoryModel() {
  /**
   * DirectoryModel fake.
   */
  class FakeDirectoryModel extends EventTarget {
    constructor() {
      super();

      /** @private {!FileFilter} */
      this.fileFilter_ = createFakeFileFilter();

      /** @private {FilesAppDirEntry} */
      this.myFiles_ = null;
    }

    /**
     * @param {FilesAppDirEntry} myFilesEntry
     */
    setMyFiles(myFilesEntry) {
      this.myFiles_ = myFilesEntry;
    }

    /**
     * @return {!FileFilter} file filter.
     */
    getFileFilter() {
      return this.fileFilter_;
    }

    /**
     * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} Current directory.
     */
    getCurrentDirEntry() {
      return null;
    }

    /**
     * @returns {?VolumeInfo}
     */
    getCurrentVolumeInfo() {
      return null;
    }

    /**
     * @returns {?VolumeManagerCommon.RootType}
     */
    getCurrentRootType() {
      return null;
    }

    /**
     * @returns {?FileListModel}
     */
    getFileList() {
      return null;
    }

    /**
     * @param {!DirectoryEntry|!FilesAppDirEntry} dirEntry The entry of the new
     *     directory to be changed to.
     * @param {function()=} opt_callback Executed if the directory loads
     *     successfully.
     */
    changeDirectoryEntry(dirEntry, opt_callback) {
      if (opt_callback) {
        opt_callback();
      }
    }

    isReadOnly() {
      return false;
    }
  }

  const model = /** @type {!Object} */ (new FakeDirectoryModel());
  return /** @type {!DirectoryModel} */ (model);
}
