// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';

import {DirectoryModel} from './directory_model.js';
import {FileTable} from './ui/file_table.js';

/**
 * Controls last modified column in the file table.
 */
export class LastModifiedController {
  /**
   * @param {!FileTable} fileTable File table UI.
   * @param {!DirectoryModel} directoryModel Directory model.
   */
  constructor(fileTable, directoryModel) {
    /** @private @const {!FileTable} */
    this.fileTable_ = fileTable;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    this.directoryModel_.addEventListener(
        'scan-started', this.onScanStarted_.bind(this));
  }

  /**
   * Handles directory scan start.
   * @private
   */
  onScanStarted_() {
    // If the current directory is Recent root, request FileTable to use
    // modificationByMeTime instead of modificationTime in last modified column.
    const useModificationByMeTime =
        util.isRecentRootType(this.directoryModel_.getCurrentRootType());
    this.fileTable_.setUseModificationByMeTime(useModificationByMeTime);
    this.directoryModel_.getFileList().setUseModificationByMeTime(
        useModificationByMeTime);
  }
}
