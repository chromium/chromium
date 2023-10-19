// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The container of the VolumeInfo for each mounted volume.
 * @interface
 */
export class VolumeInfoList {
  constructor() {
    /** @const @type {number} */
    this.length = 0;
  }

  /**
   * Adds the event listener to listen the change of volume info.
   * @param {string} _type The name of the event.
   * @param {function(Event):void} _handler The handler for the event.
   */
  addEventListener(_type, _handler) {}

  /**
   * Removes the event listener.
   * @param {string} _type The name of the event.
   * @param {function(Event):void} _handler The handler to be removed.
   */
  removeEventListener(_type, _handler) {}

  /**
   * Adds the volumeInfo to the appropriate position. If there already exists,
   * just replaces it.
   * @param {import("./volume_info.js").VolumeInfo} _volumeInfo The information
   *     of the new volume.
   */
  add(_volumeInfo) {}

  /**
   * Removes the VolumeInfo having the given ID.
   * @param {string} _volumeId ID of the volume.
   */
  remove(_volumeId) {}

  /**
   * @param {number} _index The index of the volume in the list.
   * @return {!import('./volume_info.js').VolumeInfo} The VolumeInfo instance.
   */
  item(_index) {
    return /** @type {import("./volume_info.js").VolumeInfo} */ ({});
  }
}
