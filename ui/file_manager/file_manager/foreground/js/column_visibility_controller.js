// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {VolumeManager} from '../../../externs/volume_manager.m.js';
// #import {DirectoryModel} from './directory_model.m.js';
// #import {FileManagerUI} from './ui/file_manager_ui.m.js';
// #import {VolumeManagerCommon} from '../../../base/js/volume_manager_types.m.js';
// #import {importer} from '../../common/js/importer_common.m.js';
// clang-format on

/**
 * A class that controls the visibility of the import status in the main table
 * UI.
 */
/* #export */ class ColumnVisibilityController {
  /**
   * @param {!FileManagerUI} ui
   * @param {!DirectoryModel} directoryModel
   * @param {!VolumeManager} volumeManager
   */
  constructor(ui, directoryModel, volumeManager) {
    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @private @const {!FileManagerUI} */
    this.ui_ = ui;

    // Register event listener.
    directoryModel.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onDirectoryChanged_(event) {
    // Enable the status column in import-eligible locations.
    //
    // TODO(kenobi): Once import status is exposed as part of the metadata
    // system, remove this and have the underlying UI determine its own status
    // using metadata.
    const isImportEligible =
        importer.isBeneathMediaDir(event.newDirEntry, this.volumeManager_) &&
        !!this.volumeManager_.getCurrentProfileVolumeInfo(
            VolumeManagerCommon.VolumeType.DRIVE);
    this.ui_.listContainer.table.setImportStatusVisible(isImportEligible);
    this.ui_.listContainer.grid.setImportStatusVisible(isImportEligible);
  }
}
