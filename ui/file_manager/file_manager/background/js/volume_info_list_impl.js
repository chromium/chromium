// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';

/**
 * The container of the VolumeInfo for each mounted volume.
 * @final
 * @implements {VolumeInfoList}
 */
export class VolumeInfoListImpl {
  constructor() {
    /**
     * Holds VolumeInfo instances.
     * @private @const {ArrayDataModel}
     */
    this.model_ = new ArrayDataModel([]);
    Object.freeze(this);
  }

  get length() {
    return this.model_.length;
  }

  addEventListener(type, handler) {
    this.model_.addEventListener(type, handler);
  }

  removeEventListener(type, handler) {
    this.model_.removeEventListener(type, handler);
  }

  add(volumeInfo) {
    const index = this.findIndex(volumeInfo.volumeId);
    if (index !== -1) {
      this.model_.splice(index, 1, volumeInfo);
    } else {
      this.model_.push(volumeInfo);
    }
  }

  remove(volumeId) {
    const index = this.findIndex(volumeId);
    if (index !== -1) {
      this.model_.splice(index, 1);
    }
  }

  item(index) {
    return /** @type {!VolumeInfo} */ (this.model_.item(index));
  }

  /**
   * Obtains an index from the volume ID.
   * @param {string} volumeId Volume ID.
   * @return {number} Index of the volume.
   */
  findIndex(volumeId) {
    for (let i = 0; i < this.model_.length; i++) {
      if (this.model_.item(i).volumeId === volumeId) {
        return i;
      }
    }
    return -1;
  }
}
