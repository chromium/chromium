// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The container of the VolumeInfo for each mounted volume.
 * @interface
 */
class VolumeInfoList {
  constructor() {
    /** @const {number} */
    this.length;
  }

  /**
   * Adds the event listener to listen the change of volume info.
   * @param {string} type The name of the event.
   * @param {function(Event)} handler The handler for the event.
   */
  addEventListener(type, handler) {}

  /**
   * Removes the event listener.
   * @param {string} type The name of the event.
   * @param {function(Event)} handler The handler to be removed.
   */
  removeEventListener(type, handler) {}

  /**
   * Adds the volumeInfo to the appropriate position. If there already exists,
   * just replaces it.
   * @param {VolumeInfo} volumeInfo The information of the new volume.
   */
  add(volumeInfo) {}

  /**
   * Removes the VolumeInfo having the given ID.
   * @param {string} volumeId ID of the volume.
   */
  remove(volumeId) {}

  /**
   * @param {number} index The index of the volume in the list.
   * @return {!VolumeInfo} The VolumeInfo instance.
   */
  item(index) {}
}
