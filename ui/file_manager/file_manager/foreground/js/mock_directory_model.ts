// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {FakeEntry, FilesAppDirEntry} from '../../common/js/files_app_entry_types.js';
import type {RootType} from '../../common/js/volume_manager_types.js';
import type {FileKey} from '../../state/state.js';

import type {FileFilter} from './directory_contents.js';
import type {DirectoryModel} from './directory_model.js';
import type {FileListModel} from './file_list_model.js';

/**
 * @return fake for unittests.
 */
function createFakeFileFilter(): FileFilter {
  /**
   * FileFilter fake.
   */
  class FakeFileFilter extends EventTarget {
    /**
     * @param entry File entry.
     * @return True if the file should be shown.
     */
    filter(_entry: Entry): boolean {
      return true;
    }
  }

  return new FakeFileFilter() as unknown as FileFilter;
}

/**
 * @return fake for unittests.
 */
export function createFakeDirectoryModel(): DirectoryModel {
  /**
   * DirectoryModel fake.
   */
  class FakeDirectoryModel extends EventTarget {
    private fileFilter_ = createFakeFileFilter();
    constructor() {
      super();
    }

    /**
     * @return file filter.
     */
    getFileFilter(): FileFilter {
      return this.fileFilter_;
    }

    /**
     * @return Current directory.
     */
    getCurrentDirEntry(): DirectoryEntry|FakeEntry|FilesAppDirEntry|undefined {
      return undefined;
    }

    getCurrentVolumeInfo(): VolumeInfo|null {
      return null;
    }

    getCurrentRootType(): RootType|null {
      return null;
    }

    getFileList(): FileListModel|null {
      return null;
    }

    getCurrentFileKey(): FileKey|undefined {
      return undefined;
    }

    /**
     * @param dirEntry The entry of the new
     *     directory to be changed to.
     * @param opt_callback Executed if the directory loads
     *     successfully.
     */
    changeDirectoryEntry(
        _dirEntry: DirectoryEntry|FilesAppDirEntry, callback?: VoidCallback) {
      if (callback) {
        callback();
      }
    }

    isReadOnly() {
      return false;
    }

    getFileListSelection() {
      return new EventTarget();
    }
  }

  return new FakeDirectoryModel() as unknown as DirectoryModel;
}
