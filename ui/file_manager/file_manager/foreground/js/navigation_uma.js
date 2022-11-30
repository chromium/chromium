// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {metrics} from '../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

/**
 * UMA exporter for navigation in the Files app.
 *
 */
export class NavigationUma {
  /**
   * @param {!VolumeManager} volumeManager
   *
   */
  constructor(volumeManager) {
    /**
     * @type {!VolumeManager}
     * @private
     */
    this.volumeManager_ = volumeManager;
  }

  /**
   * Exports file type metric with the given |name|.
   *
   * @param {!FileEntry} entry
   * @param {string} name The histogram name.
   *
   * @private
   */
  exportRootType_(entry, name) {
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (locationInfo) {
      metrics.recordEnum(
          name, locationInfo.rootType, VolumeManagerCommon.RootTypesForUMA);
    }
  }

  /**
   * Exports UMA based on the entry that has became new current directory.
   *
   * @param {!FileEntry} entry the new directory
   */
  onDirectoryChanged(entry) {
    this.exportRootType_(entry, 'ChangeDirectory.RootType');
  }
}
