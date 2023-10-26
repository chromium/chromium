// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRecentRootType} from '../../common/js/entry_utils.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import type {VolumeInfo} from '../../externs/volume_info.js';

// To avoid the import being elided, closure requires this name here because of
// the @implements.
export const _unused = EntryLocation;

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 * @implements {EntryLocation}
 */
export class EntryLocationImpl implements EntryLocation {
  isSpecialSearchRoot: boolean;
  isDriveBased: boolean;
  hasFixedLabel: boolean;

  constructor(
      public volumeInfo: VolumeInfo|null,
      public rootType: VolumeManagerCommon.RootType,
      public isRootEntry: boolean, public isReadOnly: boolean) {
    this.isSpecialSearchRoot =
        this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
        isRecentRootType(this.rootType);


    this.isDriveBased = this.rootType === VolumeManagerCommon.RootType.DRIVE ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
        this.rootType ===
            VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT ||
        this.rootType === VolumeManagerCommon.RootType.SHARED_DRIVE ||
        this.rootType === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
        this.rootType === VolumeManagerCommon.RootType.COMPUTER;

    this.hasFixedLabel = this.isRootEntry &&
        (rootType !== VolumeManagerCommon.RootType.SHARED_DRIVE &&
         rootType !== VolumeManagerCommon.RootType.COMPUTER &&
         rootType !== VolumeManagerCommon.RootType.REMOVABLE &&
         rootType !== VolumeManagerCommon.RootType.TRASH &&
         rootType !== VolumeManagerCommon.RootType.GUEST_OS);
    Object.freeze(this);
  }
}
