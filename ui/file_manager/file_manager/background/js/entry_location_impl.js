// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {VolumeInfo} from '../../externs/volume_info.js';

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 *
 * @implements {EntryLocation}
 */
export class EntryLocationImpl {
  /**
   * @param {VolumeInfo} volumeInfo Volume information.
   * @param {VolumeManagerCommon.RootType} rootType Root type.
   * @param {boolean} isRootEntry Whether the entry is root entry or not.
   * @param {boolean} isReadOnly Whether the entry is read only or not.
   */
  constructor(volumeInfo, rootType, isRootEntry, isReadOnly) {
    /** @override */
    this.volumeInfo = volumeInfo;

    /** @override */
    this.rootType = rootType;

    /** @override */
    this.isRootEntry = isRootEntry;

    /** @override */
    this.isSpecialSearchRoot =
        this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
        util.isRecentRootType(this.rootType);

    /** @override */
    this.isDriveBased = this.rootType === VolumeManagerCommon.RootType.DRIVE ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
        this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
        this.rootType ===
            VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT ||
        this.rootType === VolumeManagerCommon.RootType.SHARED_DRIVE ||
        this.rootType === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
        this.rootType === VolumeManagerCommon.RootType.COMPUTER;

    /** @override */
    this.isReadOnly = isReadOnly;

    /** @type{boolean} */
    this.hasFixedLabel = this.isRootEntry &&
        (rootType !== VolumeManagerCommon.RootType.SHARED_DRIVE &&
         rootType !== VolumeManagerCommon.RootType.COMPUTER &&
         rootType !== VolumeManagerCommon.RootType.REMOVABLE &&
         rootType !== VolumeManagerCommon.RootType.TRASH &&
         rootType !== VolumeManagerCommon.RootType.GUEST_OS);
    Object.freeze(this);
  }
}
