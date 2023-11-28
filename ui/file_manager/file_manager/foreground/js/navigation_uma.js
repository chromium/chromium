// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordEnum} from '../../common/js/metrics.js';
import {RootTypesForUMA} from '../../common/js/volume_manager_types.js';

/**
 * UMA exporter for navigation in the Files app.
 *
 */
export class NavigationUma {
  /**
   * @param {!import('../../externs/volume_manager.js').VolumeManager}
   *     volumeManager
   *
   */
  constructor(volumeManager) {
    /**
     * @type {!import('../../externs/volume_manager.js').VolumeManager}
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
      recordEnum(name, locationInfo.rootType, RootTypesForUMA);
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
