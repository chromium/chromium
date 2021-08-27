// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {xfm} from '../../common/js/xfm.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {TAG_NAME as DriveWelcomeBannerTagName} from './ui/banners/drive_welcome_banner.js';
import {TAG_NAME as LocalDiskLowSpaceBannerTagName} from './ui/banners/local_disk_low_space_banner.js';

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
 * The HTML attribute to force show a banner, if applied, the banner will always
 * show.
 * @private {string}
 */
const _BANNER_FORCE_SHOW_ATTRIBUTE = 'force-show-for-testing';

/**
 * The central component to the Banners Framework. The controller maintains the
 * core logic that dictates which banner should be shown as well as what events
 * require a reconciliation of the banners to ensure the right banner is shown
 * at the right time.
 */
export class BannerController extends EventTarget {
  /**
   * @param {!DirectoryModel} directoryModel
   * @param {!VolumeManager} volumeManager
   */
  constructor(directoryModel, volumeManager) {
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
     * Keep track of banners that subscribe to volume changes.
     * @private {!Object<!VolumeManagerCommon.VolumeType, !Array<!Banner>>}
     */
    this.volumeSizeObservers_ = {};

    /**
     * Stores the state of each banner, such as view count or last dismissed
     * time. This is kept in sync with local storage.
     * @private {!Object<string, number>}
     */
    this.localStorageCache_ = {};

    /**
     * Maintains the state of the current volume that has been navigated. This
     * is updated by the directory-changed event.
     * @private {?VolumeInfo}
     */
    this.currentVolume_ = null;

    /**
     * Maintains a cache of the current size for all observed volumes. If a
     * banner requests to observe a volumeType on initialization, the volume
     * size is cached here, keyed by volumeId.
     * @private {!Object<string,
     *     (!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
     */
    this.volumeSizeStats_ = {};

    /**
     * The directory model is maintained to get the current volume and also to
     * listen to the directory-changed event.
     * @private {!DirectoryModel}
     */
    this.directoryModel_ = directoryModel;

    /**
     * The volume manager used to extract volumes from events when the
     * underlying volume size has changed.
     * @private {!VolumeManager}
     */
    this.volumeManager_ = volumeManager;

    /**
     * The container where all the banners will be appended to.
     * @private {?Element}
     */
    this.container_ = document.querySelector('#banners');

    /**
     * Whether banners should be loaded or not during for unit tests.
     * @private {boolean}
     */
    this.disableBanners_ = false;

    /**
     * Bind the onDirectorySizeChanged_ method to this instance once.
     * @private {!function(!chrome.fileManagerPrivate.FileWatchEvent)}
     */
    this.onDirectorySizeChangedBound_ = async event =>
        this.onDirectorySizeChanged_(event);

    xfm.storage.onChanged.addListener(
        (changes, areaName) => this.onStorageChanged_(changes, areaName));
    this.directoryModel_.addEventListener(
        'directory-changed', event => this.onDirectoryChanged_(event));
  }

  /**
   * Ensure all banners are in priority order and any existing local storage
   * values are retrieved.
   * @return {Promise<void>}
   */
  async initialize() {
    if (!this.disableBanners_) {
      // Banners are initialized in their priority order. The order of the array
      // denotes the priority of the banner, 0th index is highest priority.
      this.setWarningBannersInOrder([LocalDiskLowSpaceBannerTagName]);
      this.setEducationalBannersInOrder([DriveWelcomeBannerTagName]);
    }

    for (const banner of this.warningBanners_) {
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${LAST_DISMISSED_SUFFIX}`] = 0;

      this.maybeAddVolumeSizeObserver_(banner);
    }

    for (const banner of this.educationalBanners_) {
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;

      this.maybeAddVolumeSizeObserver_(banner);
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
    if (banner.hasAttribute(_BANNER_FORCE_SHOW_ATTRIBUTE)) {
      return true;
    }

    // Check if the banner should be shown on this particular volume type.
    const allowedVolumeTypes = banner.allowedVolumeTypes();
    if (!isAllowedVolume(this.currentVolume_, allowedVolumeTypes)) {
      return false;
    }

    // Check if the banner has exceeded the maximum number of times it can be
    // shown over multiple Files app sessions.
    const showLimit = banner.showLimit();
    if (showLimit) {
      const timesShown =
          this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`];
      if (timesShown >= showLimit && !banner.isConnected) {
        return false;
      }
    }

    // Check if the threshold has been breached for the banner to be shown.
    const diskThreshold = banner.diskThreshold();
    if (diskThreshold) {
      const currentVolumeSizeStats = this.currentVolume_ &&
          this.volumeSizeStats_[this.currentVolume_.volumeId];
      if (!isBelowThreshold(diskThreshold, currentVolumeSizeStats)) {
        return false;
      }
    }

    // Check if the banner has previously been dismissed and should not be shown
    // for a set duration. Date.now returns in milliseconds so convert seconds
    // into milliseconds.
    const hideAfterDismissedDurationSeconds =
        banner.hideAfterDismissedDurationSeconds() * 1000;
    const lastDismissedMilliseconds =
        this.localStorageCache_[`${banner.tagName}_${LAST_DISMISSED_SUFFIX}`];
    if (hideAfterDismissedDurationSeconds &&
        ((Date.now() - lastDismissedMilliseconds) <
         hideAfterDismissedDurationSeconds)) {
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
    if (!banner.isConnected) {
      this.container_.appendChild(/** @type {Node} */ (banner));

      // Views are set when the banner is first appended to the DOM. This
      // denotes a new app session.
      if (banner.showLimit()) {
        const localStorageKey = `${banner.tagName}_${VIEW_COUNTER_SUFFIX}`;
        await this.setLocalStorage_(
            localStorageKey, this.localStorageCache_[localStorageKey] + 1);
      }
    }

    banner.removeAttribute('hidden');
    banner.setAttribute('aria-hidden', 'false');

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

    banner.toggleAttribute('hidden', true);
    banner.setAttribute('aria-hidden', 'true');
  }

  /**
   * If the banner implements diskThreshold, add the banner to the observers of
   * volume size for the specified volumeType.
   * @param {!Banner} banner The banner to add.
   * @private
   */
  maybeAddVolumeSizeObserver_(banner) {
    if (!banner.diskThreshold()) {
      return;
    }

    const diskThreshold = banner.diskThreshold();
    if (!this.volumeSizeObservers_[diskThreshold.type]) {
      this.volumeSizeObservers_[diskThreshold.type] = [];
    }

    this.volumeSizeObservers_[diskThreshold.type].push(banner);
  }

  /**
   * Creates all the warning banners with the supplied tagName's. This will
   * populate the warningBanners_ array with HTMLElement's.
   * @param {!Array<string>} bannerTagNames The HTMLElement tagName's to create.
   */
  setWarningBannersInOrder(bannerTagNames) {
    for (const tagName of bannerTagNames) {
      const banner = /** @type {!Banner} */ (document.createElement(tagName));
      banner.toggleAttribute('hidden', true);
      banner.setAttribute('aria-hidden', 'true');
      banner.addEventListener(
          Banner.Event.BANNER_DISMISSED,
          event => this.onBannerDismissedClick_(event));
      this.warningBanners_.push(banner);
    }
  }

  /**
   * Creates all the educational banners with the supplied tagName's. This will
   * populate the educationalBanners_ array with HTMLElement's.
   * @param {!Array<string>} bannerTagNames The HTMLElement tagName's to create.
   */
  setEducationalBannersInOrder(bannerTagNames) {
    for (const tagName of bannerTagNames) {
      const banner = /** @type {!Banner} */ (document.createElement(tagName));
      banner.toggleAttribute('hidden', true);
      banner.setAttribute('aria-hidden', 'true');
      banner.addEventListener(
          Banner.Event.BANNER_DISMISSED_FOREVER,
          event => this.onBannerDismissedClick_(event));
      this.educationalBanners_.push(banner);
    }
  }

  /**
   * Disable the banners from being loaded for testing. This is used to override
   * the loading of actual banners to load fake banners in unit tests.
   */
  disableBannersForTesting() {
    this.disableBanners_ = true;
  }

  /**
   * Toggles force show a single banner. If multiple banners are force shown
   * the banner with the highest priority will still be the only one shown.
   * @param {string} bannerTagName The tagName of the banner to force show.
   */
  async toggleBannerForTesting(bannerTagName) {
    const orderedBanners =
        this.warningBanners_.concat(this.educationalBanners_);
    for (const banner of orderedBanners) {
      if (banner.tagName === bannerTagName) {
        banner.toggleAttribute(_BANNER_FORCE_SHOW_ATTRIBUTE);
        await this.reconcile();
        return;
      }
    }
    console.warn(`${bannerTagName} not found in initialized banners`);
  }

  /**
   * Create an event handler bound to the specific banner that was created.
   * @param {!Event} event The banner-dismissed event.
   * @private
   */
  onBannerDismissedClick_(event) {
    if (!event.detail || !event.detail.banner) {
      console.warn('Banner dismiss event missing banner detail');
      return;
    }
    const banner = event.detail.banner;

    // If the banner has been dismissed forever (in the case of educational
    // banners) set the view counter to the max limit to ensure it is not
    // shown again.
    if (event.type === Banner.Event.BANNER_DISMISSED_FOREVER) {
      this.setLocalStorage_(
          `${banner.tagName}_${VIEW_COUNTER_SUFFIX}`, banner.showLimit());
      this.hideBannerIfShown_(banner);
      return;
    }

    // Reset the view counter so that after the dismiss duration elapses the
    // banner can be shown for the showLimit again.
    this.setLocalStorage_(`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`, 0);
    this.setLocalStorage_(
        `${banner.tagName}_${LAST_DISMISSED_SUFFIX}`, Date.now());
    this.hideBannerIfShown_(banner);
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
    const previousVolume = this.currentVolume_;
    await this.reconcile();

    // Don't change subscriptions if the volume hasn't changed.
    if (this.currentVolume_ === previousVolume) {
      return;
    }

    if (!this.currentVolume_ ||
        !this.volumeSizeObservers_[this.currentVolume_.volumeType]) {
      chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
          this.onDirectorySizeChangedBound_);
      return;
    }

    const isSubscribedByPreviousVolume =
        previousVolume && this.volumeSizeStats_[previousVolume.volumeType];
    if (!isSubscribedByPreviousVolume &&
        this.volumeSizeObservers_[this.currentVolume_.volumeType]) {
      chrome.fileManagerPrivate.onDirectoryChanged.addListener(
          this.onDirectorySizeChangedBound_);
    }
  }

  /**
   * When a directory changes, grab the current directory size. This is useful
   * if events are occurring on the current Files app directory (e.g. a copy
   * operation occurs and the disk size changes). Use this event to check if
   * the underlying disk space has changed.
   * @param {!chrome.fileManagerPrivate.FileWatchEvent} event
   * @private
   */
  async onDirectorySizeChanged_(event) {
    if (!event.entry) {
      return;
    }
    const eventVolumeInfo =
        this.volumeManager_.getVolumeInfo(/** @type{!Entry} */ (event.entry));
    if (!eventVolumeInfo || !eventVolumeInfo.volumeId) {
      return;
    }
    const sizeStats = await getSizeStats(eventVolumeInfo.volumeId);
    if (!sizeStats || sizeStats.totalSize === 0) {
      return;
    }
    this.volumeSizeStats_[eventVolumeInfo.volumeId] = sizeStats;
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

/**
 * Wrap the chrome.fileManagerPrivate.getSizeStats function in an async/await
 * compatible style.
 * @param {string} volumeId The volumeId to retrieve the size stats for.
 * @returns {!Promise<(!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
 */
async function getSizeStats(volumeId) {
  return new Promise((resolve) => {
    chrome.fileManagerPrivate.getSizeStats(volumeId, resolve);
  });
}
