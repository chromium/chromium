// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The container of the VolumeInfo for each mounted volume.
 * Disable type checking for closure, as it is done by the typescript compiler.
 * @suppress {checkTypes}
 */

import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';

// To avoid the import being elided, closure requires this name here because of
// the @implements.
export const _unused = VolumeInfoList;

/**
 * @implements {VolumeInfoList}
 */
export class VolumeInfoListImpl implements VolumeInfoList {
  /**
   * Holds VolumeInfo instances.
   */
  private model_ = new ArrayDataModel([]);
  constructor() {
    Object.freeze(this);
  }

  get length(): number {
    return this.model_.length;
  }

  addEventListener(
      type: string, handler: EventListenerOrEventListenerObject|null) {
    this.model_.addEventListener(type, handler);
  }

  removeEventListener(
      type: string, handler: EventListenerOrEventListenerObject|null) {
    this.model_.removeEventListener(type, handler);
  }

  add(volumeInfo: VolumeInfo) {
    const index = this.findIndex(volumeInfo.volumeId);
    if (index !== -1) {
      this.model_.splice(index, 1, volumeInfo);
    } else {
      this.model_.push(volumeInfo);
    }
  }

  remove(volumeId: string) {
    const index = this.findIndex(volumeId);
    if (index !== -1) {
      // @ts-ignore: TS doesn't recognize Closure {...*} type.
      this.model_.splice(index, 1);
    }
  }

  item(index: number): VolumeInfo {
    return (this.model_.item(index)) as VolumeInfo;
  }

  /**
   * Obtains an index from the volume ID.
   */
  findIndex(volumeId: string): number {
    for (let i = 0; i < this.model_.length; i++) {
      if (this.model_.item(i).volumeId === volumeId) {
        return i;
      }
    }
    return -1;
  }
}
