// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../common/js/array_data_model.js';

import type {VolumeInfo} from './volume_info.js';

/**
 * The container of the VolumeInfo for each mounted volume.
 */
export interface VolumeInfoList extends ArrayDataModel<VolumeInfo> {
  /**
   * Adds the volumeInfo to the appropriate position. If there already exists,
   * just replaces it.
   * @param volumeInfo The information of the new volume.
   */
  add(volumeInfo: VolumeInfo);

  /**
   * Removes the VolumeInfo having the given ID.
   * @param volumeId ID of the volume.
   */
  remove(volumeId: string);

  item(index: number): VolumeInfo;
}
