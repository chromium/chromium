// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {xfm} from '../../common/js/xfm.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {DirectoryModel} from './directory_model.js';

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
  /**
   * @param {!DirectoryModel} directoryModel
   */
  constructor(directoryModel) {
    super();

    /**
     * Warning banners ordered by priority. Index 0 is the highest priority.
     * @private {!Array<!Banner>}
     */
    this.warningBanners_ = [];
    /**
     * Educational banners ordered by priority. Index 0 is the highest
     * priority.
     * @private {!Array<!Banner>}
     */
    this.educationalBanners_ = [];

    /**
     * Stores the state of each banner, such as view count or last dismissed
     * time. This is kept in sync with local storage.
     * @private {!Object<string, number>}
     */
    this.localStorageCache_ = {};

    /**
     * Maintains the state of the current volume that has been navigated. This
     * is updated by the directory-changed event.
     * @type {?VolumeInfo}
     * @private
     */
    this.currentVolume_ = null;

    this.directoryModel_ = directoryModel;
    this.container_ = document.querySelector('#banners');

    xfm.storage.onChanged.addListener(this.onStorageChanged_.bind(this));
    this.directoryModel_.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
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
    let values = {};
    try {
      values = await xfm.storage.local.getAsync(cacheKeys);
    } catch (e) {
      console.warn(e.message);
    }
    for (const key of cacheKeys) {
      const storedValue = parseInt(values[key], 10);
      if (storedValue) {
        this.localStorageCache_[key] = storedValue;
      }
    }
  }

  /**
   * Loops through all the banners and checks whether they should be shown or
   * not. If shown, picks the highest priority banner.
   */
  async reconcile() {
    this.currentVolume_ = this.directoryModel_.getCurrentVolumeInfo();

    /** @type {?Banner} */
    let bannerToShow = null;

    // Identify if (given current conditions) any of the warning banners should
    // be shown or hidden.
    const orderedBanners =
        this.warningBanners_.concat(this.educationalBanners_);
    for (const banner of orderedBanners) {
      if (!this.shouldShowBanner_(banner)) {
        this.hideBannerIfShown_(banner);
        continue;
      }

      if (!bannerToShow) {
        bannerToShow = banner;
      }
    }

    if (bannerToShow) {
      await this.showBanner_(bannerToShow);
    }
  }

  /**
   * Checks if the banner should be visible.
   * @param {!Banner} banner The banner to check.
   * @return {boolean}
   * @private
   */
  shouldShowBanner_(banner) {
    const allowedVolumeTypes = banner.allowedVolumeTypes();
    if (!isAllowedVolume(this.currentVolume_, allowedVolumeTypes)) {
      return false;
    }

    const showLimit = banner.showLimit();
    const timesShown =
        this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`];
    if (showLimit && timesShown >= showLimit) {
      return false;
    }

    return true;
  }

  /**
   * Check if the banner exists (add to DOM if not) and ensure it's visible.
   * @param {!Banner} banner The banner to hide.
   * @private
   */
  async showBanner_(banner) {
    if (banner.parentElement !== this.container_) {
      this.container_.appendChild(/** @type {Node} */ (banner));

      // Views are set when the banner is first appended to the DOM. This
      // denotes a new app session.
      const localStorageKey = `${banner.tagName}_${VIEW_COUNTER_SUFFIX}`;
      await this.setLocalStorage_(
          localStorageKey, this.localStorageCache_[localStorageKey] + 1);
    }

    banner.setAttribute('hidden', false);
    banner.setAttribute('aria-hidden', false);

    banner.onShow();
  }

  /**
   * Hide the banner if it exists in the DOM.
   * @param {!Banner} banner The banner to hide.
   * @private
   */
  hideBannerIfShown_(banner) {
    if (banner.parentElement !== this.container_) {
      return;
    }

    banner.setAttribute('hidden', true);
    banner.setAttribute('aria-hidden', true);
  }

  /**
   * Creates all the warning banners with the supplied tagName's. This will
   * populate the |warningBanners_| array with HTMLElement's.
   * @param {!Array<string>} bannerTagNames The HTMLElement tagName's to create.
   */
  setWarningBannersInOrder(bannerTagNames) {
    for (const tagName of bannerTagNames) {
      this.warningBanners_.push(
          /** @type {!Banner} */ (document.createElement(tagName)));
    }
  }

  /**
   * Creates all the educational banners with the supplied tagName's. This will
   * populate the |educationalBanners_| array with HTMLElement's.
   * @param {!Array<string>} bannerTagNames The HTMLElement tagName's to create.
   */
  setEducationalBannersInOrder(bannerTagNames) {
    for (const tagName of bannerTagNames) {
      this.educationalBanners_.push(
          /** @type {!Banner} */ (document.createElement(tagName)));
    }
  }

  /**
   * Writes through the localStorage cache to localStorage to ensure values
   * are immediately available.
   * @param {string} key The key in localStorage to set.
   * @param {number} value The value to set the key to in localStorage.
   * @private
   */
  async setLocalStorage_(key, value) {
    if (!this.localStorageCache_.hasOwnProperty(key)) {
      console.error(`Key ${key} not found in localStorage cache`);
      return;
    }
    this.localStorageCache_[key] = value;
    try {
      await xfm.storage.local.setAsync({[key]: value});
    } catch (e) {
      console.warn(e.message);
    }
  }

  /**
   * Invoked when a directory has been changed, used to update the local cache
   * and reconcile the current banners being shown.
   * @param {Event} event The directory-changed event.
   * @private
   */
  async onDirectoryChanged_(event) {
    await this.reconcile();
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
 * @param {?VolumeInfo} currentVolume Volume that is currently navigated.
 * @param {!Array<!Banner.AllowedVolumeType>} allowedVolumeTypes Array of
 * allowed volumes
 * @return {boolean}
 */
export function isAllowedVolume(currentVolume, allowedVolumeTypes) {
  // Some entries return null despite being valid volume (e.g. the root entry
  // for a USB drive).
  if (!currentVolume) {
    return false;
  }
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

/**
 * Checks if the current sizeStats are below the threshold required to trigger
 * the banner to show.
 * @param {!Banner.DiskThresholdMinRatio|!Banner.DiskThresholdMinSize|undefined}
 *     threshold
 * @param {?chrome.fileManagerPrivate.MountPointSizeStats|undefined} sizeStats
 * @returns {boolean}
 */
export function isBelowThreshold(threshold, sizeStats) {
  if (!threshold || !sizeStats) {
    return false;
  }
  if (!sizeStats.remainingSize || !sizeStats.totalSize) {
    return false;
  }
  if (threshold.minSize < sizeStats.remainingSize) {
    return false;
  }
  const currentRatio = sizeStats.remainingSize / sizeStats.totalSize;
  if (threshold.minRatio < currentRatio) {
    return false;
  }
  return true;
}
