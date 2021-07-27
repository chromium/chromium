// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {xfm} from '../../common/js/xfm.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

/**
 * Local storage key suffix for how many times a banner was shown.
 * @type {string}
 */
const VIEW_COUNTER_SUFFIX = '_VIEW_COUNTER';

/**
 * Local storage key suffix for the last Date a banner was dismissed.
 * @type {string}
 */
const LAST_DISMISSED_SUFFIX = '_LAST_DISMISSED';

/**
 * The central component to the Banners Framework. The controller maintains the
 * core logic that dictates which banner should be shown as well as what events
 * require a reconciliation of the banners to ensure the right banner is shown
 * at the right time.
 */
export class BannerController extends EventTarget {
  constructor() {
    super();

    /**
     * Warning banners ordered by priority. Index 0 is the highest priority.
     * @private {!Array<!Banner|!HTMLElement>}
     */
    this.warningBanners_ = [];
    /**
     * Educational banners ordered by priority. Index 0 is the highest
     * priority.
     * @private {!Array<!Banner|!HTMLElement>}
     */
    this.educationalBanners_ = [];

    /**
     * Stores the state of each banner, such as view count or last dismissed
     * time. This is kept in sync with local storage.
     * @private {!Object<string, number>}
     */
    this.localStorageCache_ = {};

    xfm.storage.onChanged.addListener(this.onStorageChanged_.bind(this));
  }

  /**
   * Ensure all banners are in priority order and any existing local storage
   * values are retrieved.
   * @return {Promise<void>}
   */
  async initialize() {
    // TODO(benreich): Initialize banners in their priority order when
    // implemented.

    for (const banner of this.warningBanners_) {
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${LAST_DISMISSED_SUFFIX}`] = 0;
    }

    for (const banner of this.educationalBanners_) {
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;
    }

    const cacheKeys = Object.keys(this.localStorageCache_);

    // TODO(crbug.com/1233372): Update xfm.storage.local.get method to async
    // friendly syntax once implemented.
    return new Promise((resolve, reject) => {
      xfm.storage.local.get(cacheKeys, values => {
        if (chrome.runtime.lastError) {
          reject(
              'Failed to load banner data from storage: ' +
              chrome.runtime.lastError.message);
          return;
        }
        for (const key of cacheKeys) {
          const storedValue = parseInt(values[key], 10);
          if (storedValue) {
            this.localStorageCache_[key] = storedValue;
          }
        }
        // TODO(benreich): Call the reconcile method here once the method has
        // been implemented.
        resolve();
      });
    });
  }

  /**
   * Listens for localStorage changes to ensure instance cache is in sync.
   * @param {!Object<string, !StorageChange>} changes Changes that occurred.
   * @param {string} areaName One of "sync"|"local"|"managed".
   * @private
   */
  onStorageChanged_(changes, areaName) {
    if (areaName != 'local') {
      return;
    }

    for (const key in changes) {
      if (this.localStorageCache_.hasOwnProperty(key)) {
        this.localStorageCache_[key] = changes[key].newValue;
      }
    }
  }
}

/**
 * Identifies if the current volume is in the list of allowed volume type
 * array for a specific banner.
 * @param {!VolumeInfo} currentVolume Volume that is currently navigated.
 * @param {!Array<!Banner.AllowedVolumeType>} allowedVolumeTypes Array of
 * allowed volumes
 * @return {boolean}
 */
export function isAllowedVolume(currentVolume, allowedVolumeTypes) {
  for (let i = 0; i < allowedVolumeTypes.length; i++) {
    const {type, id} = allowedVolumeTypes[i];
    if (currentVolume.volumeType !== type) {
      continue;
    }
    // Avoid verifying the volumeId against null in the case a Banner wants to
    // show for all volumeId's of a specific volumeType.
    if (id && currentVolume.volumeId !== id) {
      continue;
    }
    return true;
  }
  return false;
}
