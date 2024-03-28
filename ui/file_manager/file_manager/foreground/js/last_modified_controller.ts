// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRecentRootType} from '../../common/js/entry_utils.js';

import type {DirectoryModel} from './directory_model.js';
import type {FileTable} from './ui/file_table.js';

/**
 * Controls last modified column in the file table.
 */
export class LastModifiedController {
  constructor(
      private fileTable_: FileTable, private directoryModel_: DirectoryModel) {
    this.directoryModel_.addEventListener(
        'cur-dir-scan-started', this.onScanStarted_.bind(this));
  }

  /**
   * Handles directory scan start.
   */
  private onScanStarted_() {
    // If the current directory is Recent root, request FileTable to use
    // modificationByMeTime instead of modificationTime in last modified column.
    const useModificationByMeTime =
        isRecentRootType(this.directoryModel_.getCurrentRootType());
    this.fileTable_.setUseModificationByMeTime(useModificationByMeTime);
    this.directoryModel_.getFileList().setUseModificationByMeTime(
        useModificationByMeTime);
  }
}
