// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The container of the VolumeInfo for each mounted volume.
 */

import {ArrayDataModel} from '../../common/js/array_data_model.js';

import type {VolumeInfo} from './volume_info.js';

/**
 * The container of the VolumeInfo for each mounted volume.
 */
export class VolumeInfoList extends ArrayDataModel<VolumeInfo> {
  constructor() {
    super([]);
  }

  /**
   * Adds the volumeInfo to the appropriate position. If there already exists,
   * just replaces it.
   */
  add(volumeInfo: VolumeInfo) {
    const index = this.findIndex(volumeInfo.volumeId);
    if (index !== -1) {
      super.splice(index, 1, volumeInfo);
    } else {
      super.push(volumeInfo);
    }
  }

  /**
   * Removes the VolumeInfo having the given ID.
   */
  remove(volumeId: string) {
    const index = this.findIndex(volumeId);
    if (index !== -1) {
      super.splice(index, 1);
    }
  }

  override item(index: number): VolumeInfo {
    return super.item(index)!;
  }

  /**
   * Obtains an index from the volume ID.
   */
  findIndex(volumeId: string): number {
    for (let i = 0; i < this.length; i++) {
      if (this.item(i).volumeId === volumeId) {
        return i;
      }
    }
    return -1;
  }
}
