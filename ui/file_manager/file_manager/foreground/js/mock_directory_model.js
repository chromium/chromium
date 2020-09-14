// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @returns {!FileFilter} fake for unittests.
 */
function createFakeFileFilter() {
  /**
   * FileFilter fake.
   */
  class FakeFileFilter extends cr.EventTarget {
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
function createFakeDirectoryModel() {
  /**
   * DirectoryModel fake.
   */
  class FakeDirectoryModel extends cr.EventTarget {
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
