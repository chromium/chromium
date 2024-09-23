// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {UniversalDirectory} from '../../common/js/files_app_entry_types.js';
import {recordEnum} from '../../common/js/metrics.js';
import {RootTypesForUMA} from '../../common/js/volume_manager_types.js';

/**
 * Records a UMA when a new directory is navigated to.
 */
export class NavigationUma {
  constructor(private volumeManager_: VolumeManager) {}

  /**
   * Records a UMA that captures the root type of the new current directory.
   *
   * @param entry the new directory
   */
  onDirectoryChanged(entry: UniversalDirectory|undefined) {
    // TOOD(b/327533814): Refactor to work with FileData.
    if (!entry) {
      return;
    }

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (locationInfo) {
      recordEnum(
          'ChangeDirectory.RootType', locationInfo.rootType, RootTypesForUMA);
    }
  }
}
