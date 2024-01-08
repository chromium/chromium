// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {saveAppState, updateAppState} from '../../common/js/app_util.js';
import {isRecentRoot} from '../../common/js/entry_utils.js';
import {storage} from '../../common/js/storage.js';
import {DialogType} from '../../state/state.js';

import type {DirectoryChangeEvent, DirectoryModel} from './directory_model.js';
import {GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME} from './file_list_model.js';
import type {FileManagerUI} from './ui/file_manager_ui.js';
import type {FileTableColumnModel} from './ui/file_table.js';
import {ListType} from './ui/list_container.js';

export class AppStateController {
  private readonly viewOptionStorageKey_: string;
  private directoryModel_: DirectoryModel|null = null;
  private ui_: FileManagerUI|null = null;
  private viewOptions_: any = null;

  /**
   * Preferred sort field of file list. This will be ignored in the Recent
   * folder, since it always uses descendant order of date-mofidied.
   */
  private fileListSortField_: string|null = DEFAULT_SORT_FIELD;

  /**
   * Preferred sort direction of file list. This will be ignored in the Recent
   * folder, since it always uses descendant order of date-mofidied.
   */
  private fileListSortDirection_: string|null = DEFAULT_SORT_DIRECTION;

  constructor(dialogType: DialogType) {
    this.viewOptionStorageKey_ = 'file-manager-' + dialogType;
  }

  async loadInitialViewOptions(): Promise<void> {
    // Load initial view option.
    try {
      const values = await storage.local.getAsync(this.viewOptionStorageKey_);

      this.viewOptions_ = {};

      const value = values[this.viewOptionStorageKey_];
      if (!value) {
        return;
      }

      // Load the global default options.
      try {
        this.viewOptions_ = JSON.parse(value);
      } catch (ignore) {
      }

      // Override with window-specific options.
      if (window?.appState?.viewOptions) {
        for (const [key, value] of Object.entries(
                 window.appState.viewOptions)) {
          this.viewOptions_[key] = value;
        }
      }
    } catch (error) {
      this.viewOptions_ = {};
      console.warn(error);
    }
  }

  initialize(ui: FileManagerUI, directoryModel: DirectoryModel) {
    assert(this.viewOptions_);

    this.ui_ = ui;
    this.directoryModel_ = directoryModel;
    const {table} = ui.listContainer;

    // Register event listeners.
    table.addEventListener(
        'column-resize-end', this.saveViewOptions.bind(this));
    directoryModel.getFileList().addEventListener(
        'sorted', this.onFileListSorted_.bind(this));
    directoryModel.getFileFilter().addEventListener(
        'changed', this.onFileFilterChanged_.bind(this));
    directoryModel.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));

    // Restore preferences.
    ui.setCurrentListType(this.viewOptions_.listType || ListType.DETAIL);
    if (this.viewOptions_.sortField) {
      this.fileListSortField_ = this.viewOptions_.sortField;
    }
    if (this.viewOptions_.sortDirection) {
      this.fileListSortDirection_ = this.viewOptions_.sortDirection;
    }
    this.directoryModel_.getFileList().sort(
        this.fileListSortField_!, this.fileListSortDirection_!);
    if (this.viewOptions_.isAllAndroidFoldersVisible) {
      this.directoryModel_.getFileFilter().setAllAndroidFoldersVisible(true);
    }
    if (this.viewOptions_.columnConfig) {
      (table.columnModel as FileTableColumnModel)
          .restoreColumnConfig(this.viewOptions_.columnConfig);
      // The stored config might not match the current table width, do a
      // normalization here after restoration.
      table.columnModel.normalizeWidths(table.clientWidth);
    }
  }

  /**
   * Saves current view option.
   */
  async saveViewOptions() {
    const prefs = {
      sortField: this.fileListSortField_,
      sortDirection: this.fileListSortDirection_,
      columnConfig: {},
      listType: this.ui_?.listContainer.currentListType,
      isAllAndroidFoldersVisible:
          this.directoryModel_?.getFileFilter().isAllAndroidFoldersVisible(),
    };
    assert(this.ui_);
    const cm = this.ui_.listContainer.table.columnModel as FileTableColumnModel;
    prefs.columnConfig = cm.exportColumnConfig();
    // Save the global default.
    const items: Record<string, string> = {};
    items[this.viewOptionStorageKey_] = JSON.stringify(prefs);
    storage.local.setAsync(items);

    // Save the window-specific preference.
    if (window.appState) {
      window.appState.viewOptions = prefs;
      saveAppState();
    }
  }

  private async onFileListSorted_() {
    assert(this.directoryModel_);
    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirectory) {
      return;
    }

    // Update preferred sort field and direction only when the current directory
    // is not Recent folder.
    if (!isRecentRoot(currentDirectory)) {
      const currentSortStatus = this.directoryModel_.getFileList().sortStatus;
      this.fileListSortField_ = currentSortStatus.field;
      this.fileListSortDirection_ = currentSortStatus.direction;
    }
    this.saveViewOptions();
  }

  private async onFileFilterChanged_() {
    assert(this.directoryModel_);
    const isAllAndroidFoldersVisible =
        this.directoryModel_.getFileFilter().isAllAndroidFoldersVisible();
    if (this.viewOptions_.isAllAndroidFoldersVisible !==
        isAllAndroidFoldersVisible) {
      this.viewOptions_.isAllAndroidFoldersVisible = isAllAndroidFoldersVisible;
      this.saveViewOptions();
    }
  }

  private onDirectoryChanged_(event: DirectoryChangeEvent) {
    if (!event.detail.newDirEntry) {
      return;
    }

    assert(this.directoryModel_);
    assert(this.ui_);

    // Sort the file list by:
    // 1) 'date-mofidied' and 'desc' order on Recent folder.
    // 2) preferred field and direction on other folders.
    const isOnRecent = isRecentRoot(event.detail.newDirEntry);
    const fileListModel = this.directoryModel_.getFileList();
    this.ui_.listContainer.isOnRecent = isOnRecent;
    const isOnRecentBefore = event.detail.previousDirEntry &&
        isRecentRoot(event.detail.previousDirEntry);
    if (isOnRecent !== isOnRecentBefore) {
      if (isOnRecent) {
        fileListModel.groupByField = GROUP_BY_FIELD_MODIFICATION_TIME;
        fileListModel.sort(DEFAULT_SORT_FIELD, DEFAULT_SORT_DIRECTION);
      } else {
        const isGridView =
            this.ui_?.listContainer.currentListType === ListType.THUMBNAIL;
        fileListModel.groupByField =
            isGridView ? GROUP_BY_FIELD_DIRECTORY : null;
        fileListModel.sort(
            this.fileListSortField_!, this.fileListSortDirection_!);
      }
    }

    updateAppState(
        this.directoryModel_.getCurrentDirEntry() ?
            this.directoryModel_.getCurrentDirEntry()!.toURL() :
            '',
        /*selectionURL=*/ '');
  }
}

/**
 * Default sort field of the file list.
 */
const DEFAULT_SORT_FIELD = 'modificationTime';

/**
 * Default sort direction of the file list.
 */
const DEFAULT_SORT_DIRECTION = 'desc';
