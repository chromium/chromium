// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {VolumeManager} from '../../externs/volume_manager.js';

import {NavigationModelItem, NavigationModelVolumeItem} from './navigation_list_model.js';

/**
 * Container for a NavigationModelVolumeItem, allowing it to be reused for a
 * given volumeInfo.
 */
class MockNavigationListItem {
  // @ts-ignore: error TS7006: Parameter 'volumeInfo' implicitly has an 'any'
  // type.
  constructor(volumeInfo) {
    this.volumeInfo_ = volumeInfo;
    this.item_ = new NavigationModelVolumeItem(volumeInfo.label, volumeInfo);
  }

  get volumeInfo() {
    return this.volumeInfo_;
  }

  get item() {
    return this.item_;
  }
}

/**
 * Mock class for NavigationListModel.
 * Current implementation of mock class cannot handle shortcut list.
 */
export class MockNavigationListModel extends EventTarget {
  /**
   * @param {VolumeManager} volumeManager A volume manager.
   */
  constructor(volumeManager) {
    super();
    this.volumeManager_ = volumeManager;
    // @ts-ignore: error TS7008: Member 'entries_' implicitly has an 'any[]'
    // type.
    this.entries_ = [];
  }

  /**
   * Returns the item at the given index.
   * @param {number} index The index of the item to get.
   * @return {NavigationModelItem} The item at the given index.
   */
  item(index) {
    // DirectoryTree expects these NavigationModelVolumeItem objects to be long
    // lived. So cache them and only change if the underlying volumeInfoList has
    // changed.
    let item = this.entries_[index];
    const volumeInfo = this.volumeManager_.volumeInfoList.item(index);
    if (!item || item.volumeInfo !== volumeInfo) {
      item = new MockNavigationListItem(volumeInfo);
      this.entries_[index] = item;
    }
    return item.item;
  }

  /**
   * Returns the number of items in the model.
   * @return {number} The length of the model.
   * @private
   */
  length_() {
    return this.volumeManager_.volumeInfoList.length;
  }

  get length() {
    return this.length_();
  }
}
