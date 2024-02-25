// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRecentRootType} from '../../common/js/entry_utils.js';
import {RootType} from '../../common/js/volume_manager_types.js';

import type {VolumeInfo} from './volume_info.js';

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 */
export class EntryLocation {
  /**
   * Whether the location is under Google Drive or a special search root which
   * represents a special search from Google Drive.
   */
  isSpecialSearchRoot: boolean;

  /**
   * Whether the location is under Google Drive or a special search root which
   * represents a special search from Google Drive.
   */
  isDriveBased: boolean;

  /**
   * Whether the entry should be displayed with a fixed name instead of
   * individual entry's name. (e.g. "Downloads" is a fixed name)
   */
  hasFixedLabel: boolean;

  constructor(
      public volumeInfo: VolumeInfo|null, public rootType: RootType,
      public isRootEntry: boolean, public isReadOnly: boolean) {
    this.isSpecialSearchRoot = this.rootType === RootType.DRIVE_OFFLINE ||
        this.rootType === RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === RootType.DRIVE_RECENT ||
        isRecentRootType(this.rootType);


    this.isDriveBased = this.rootType === RootType.DRIVE ||
        this.rootType === RootType.DRIVE_SHARED_WITH_ME ||
        this.rootType === RootType.DRIVE_RECENT ||
        this.rootType === RootType.DRIVE_OFFLINE ||
        this.rootType === RootType.SHARED_DRIVES_GRAND_ROOT ||
        this.rootType === RootType.SHARED_DRIVE ||
        this.rootType === RootType.COMPUTERS_GRAND_ROOT ||
        this.rootType === RootType.COMPUTER;

    this.hasFixedLabel = this.isRootEntry &&
        (rootType !== RootType.SHARED_DRIVE && rootType !== RootType.COMPUTER &&
         rootType !== RootType.REMOVABLE && rootType !== RootType.TRASH &&
         rootType !== RootType.GUEST_OS);
    Object.freeze(this);
  }
}
