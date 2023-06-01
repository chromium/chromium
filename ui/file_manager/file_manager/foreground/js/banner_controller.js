// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {getDriveQuotaMetadata, getSizeStats} from '../../common/js/api.js';
import {RateLimiter} from '../../common/js/async_util.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {storage} from '../../common/js/storage.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {Banner} from '../../externs/banner.js';
import {FakeEntry, FilesAppDirEntry} from '../../externs/files_app_entry_interfaces.js';
import {State} from '../../externs/ts/state.js';
import {Store} from '../../externs/ts/store.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {getStore} from '../../state/store.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {TAG_NAME as DlpRestrictedBannerName} from './ui/banners/dlp_restricted_banner.js';
import {TAG_NAME as DriveBulkPinningBannerTagName} from './ui/banners/drive_bulk_pinning_banner.js';
import {TAG_NAME as DriveLowIndividualSpaceBanner} from './ui/banners/drive_low_individual_space_banner.js';
import {TAG_NAME as DriveLowSharedDriveSpaceBanner} from './ui/banners/drive_low_shared_drive_space_banner.js';
import {TAG_NAME as DriveOfflinePinningBannerTagName} from './ui/banners/drive_offline_pinning_banner.js';
import {TAG_NAME as DriveOutOfIndividualSpaceBanner} from './ui/banners/drive_out_of_individual_space_banner.js';
import {TAG_NAME as DriveOutOfOrganizationSpaceBanner} from './ui/banners/drive_out_of_organization_space_banner.js';
import {TAG_NAME as DriveOutOfSharedDriveSpaceBanner} from './ui/banners/drive_out_of_shared_drive_space_banner.js';
import {TAG_NAME as DriveWelcomeBannerTagName} from './ui/banners/drive_welcome_banner.js';
import {TAG_NAME as GoogleOneOfferBannerTagName} from './ui/banners/google_one_offer_banner.js';
import {TAG_NAME as HoldingSpaceWelcomeBannerTagName} from './ui/banners/holding_space_welcome_banner.js';
import {TAG_NAME as InvalidUSBFileSystemBanner} from './ui/banners/invalid_usb_filesystem_banner.js';
import {TAG_NAME as LocalDiskLowSpaceBannerTagName} from './ui/banners/local_disk_low_space_banner.js';
import {TAG_NAME as PhotosWelcomeBannerTagName} from './ui/banners/photos_welcome_banner.js';
import {TAG_NAME as SharedWithCrostiniPluginVmBanner} from './ui/banners/shared_with_crostini_pluginvm_banner.js';
import {TAG_NAME as TrashBannerTagName} from './ui/banners/trash_banner.js';

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
 * Local storage key suffix that stores the total number of seconds a banner has
 * been visible for.
 * @type {string}
 */
const MS_DISPLAYED_SUFFIX = '_SECONDS_DISPLAYED';

/**
 * Duration between calls to keep the current banners time limit in sync.
 * @type {number}
 */
const DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS = 10000;

/**
 * Local storage key suffix for a banner that has been dismissed forever.
 * @type {string}
 */
const DISMISSED_FOREVER_SUFFIX = '_DISMISSED_FOREVER';

/**
 * The HTML attribute to force show a banner, if applied, the banner will always
 * show.
 * @private {string}
 */
const _BANNER_FORCE_SHOW_ATTRIBUTE = 'force-show-for-testing';

/**
 * Allowed duration between onDirectorySizeChanged events in milliseconds.
 * @type {number}
 */
const MIN_INTERVAL_BETWEEN_DIRECTORY_SIZE_CHANGED_EVENTS = 5000;

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
   * @param {!Crostini} crostini
   * @param {!DialogType} dialogType
   */
  constructor(directoryModel, volumeManager, crostini, dialogType) {
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
     * State banners ordered by priority. Index 0 is the highest priority.
     * @private {!Array<!Banner>}
     */
    this.stateBanners_ = [];

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
     * Maintains the currently navigated root type. This is updated by the
     * directory-changed event.
     * @private {?VolumeManagerCommon.RootType}
     */
    this.currentRootType_ = null;

    /**
     * Maintains the currently navigated shared drive if any. This is updated
     * when a reconcile event is called.
     * @private {string}
     */
    this.currentSharedDrive_ = '';

    /**
     * Maintains the currently navigated directory entry. This is updated when
     * a reconcile event is called.
     * @private {?DirectoryEntry|?FakeEntry|?FilesAppDirEntry}
     */
    this.currentEntry_ = null;

    /**
     * Maintains a cache of the current size for all observed volumes. If a
     * banner requests to observe a volumeType on initialization, the volume
     * size is cached here, keyed by volumeId.
     * @private {!Object<string,
     *     (!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
     */
    this.volumeSizeStats_ = {};

    /**
     * Maintains a cache of the user's Google Drive quota and associated
     * metadata.
     * @private {?chrome.fileManagerPrivate.DriveQuotaMetadata|undefined}
     */
    this.driveQuotaMetadata_ = null;

    /**
     * The directory model is maintained to get the current volume and also to
     * listen to the directory-changed event.
     * @private {!DirectoryModel}
     */
    this.directoryModel_ = directoryModel;

    /**
     * The Crostini background object maintains a list of shared paths with
     * Crostini and PluginVM. This is used to show the Crostini / PluginVM
     * shared with state banners.
     * @private {!Crostini}
     */
    this.crostini_ = crostini;

    /**
     * The volume manager used to extract volumes from events when the
     * underlying volume size has changed.
     * @private {!VolumeManager}
     */
    this.volumeManager_ = volumeManager;

    /**
     * The dialog type, used to determine whether certain banners should be
     * shown or not.
     * @private {!DialogType}
     */
    this.dialogType_ = dialogType;

    /**
     * The container where all the banners will be appended to.
     * @private {?Element}
     */
    this.container_ = document.querySelector('#banners');

    /**
     * Whether banners should be loaded or not during for unit tests.
     * @private {boolean}
     */
    this.disableBannerLoading_ = false;

    /**
     * Whether banners should be completely disabled, useful to remove banners
     * during integration tests or tast tests.
     * @private {boolean}
     */
    this.disableBanners_ = false;

    /**
     * A single banner to isolate and test it's functionality. Denoted by it's
     * tagName (in uppercase).
     * @private {?string}
     */
    this.isolatedBannerForTesting_ = null;

    /**
     * setInterval handle that keeps track of the total time a banner has
     * been shown for.
     * @private {?number|undefined}
     */
    this.timeLimitInterval_ = null;

    /**
     * Last time that the setInterval was invoked.
     * @private {?number}
     */
    this.timeLimitIntervalLastInvokedMs_ = null;

    /**
     * An object keyed by a banners tagName (in upper case) that lists custom
     * filters for the specified banner. Used to house banner specific logic
     * that can decide whether to display a banner or not.
     * @private {!Object<string, !Array<Banner.CustomFilter>>}
     */
    this.customBannerFilters_ = {};

    /**
     * @private {!Store}
     */
    this.store_ = getStore();
    this.store_.subscribe(this);

    /**
     * Cached value of `this.store_.currentDirectory.hasDisabledFiles`, to avoid
     * unnecessary reconciling.
     * @private {boolean}
     */
    this.hasDlpDisabledFiles_ = false;

    /**
     * The volumeId that is pending a volume size update, updateVolumeSizeStats_
     * will remove the volumeId once updated. This is cleared when the debounced
     * version of updateVolumeSizeStats_ executes.
     * @private {!Set<VolumeInfo>}
     */
    this.pendingVolumeSizeUpdates_ = new Set();

    /**
     * Bind the onDirectorySizeChanged_ method to this instance once.
     * @private {!function(!chrome.fileManagerPrivate.FileWatchEvent)}
     */
    this.onDirectorySizeChangedBound_ = async event =>
        this.onDirectorySizeChanged_(event);

    /**
     * Debounced version of updateVolumeSizeStats_ to stop overly aggressive
     * calls coming from onDirectoryChanged_.
     * @private {RateLimiter}
     */
    this.updateVolumeSizeStatsDebounced_ = new RateLimiter(
        async () => this.updateVolumeSizeStats_(),
        MIN_INTERVAL_BETWEEN_DIRECTORY_SIZE_CHANGED_EVENTS);

    // Only attach event listeners if the controller is enabled. Used to disable
    // all banners from being loaded.
    if (!this.disableBanners_) {
      storage.onChanged.addListener(
          (changes, areaName) => this.onStorageChanged_(changes, areaName));
      this.directoryModel_.addEventListener(
          'directory-changed', event => this.onDirectoryChanged_(event));
    }

    /**
     * Whether the DriveBulkPinning preference is enabled.
     * @private {boolean}
     */
    this.isDriveBulkPinningPrefEnabled_ = false;

    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(pref => {
      if (this.isDriveBulkPinningPrefEnabled_ ===
          pref.driveFsBulkPinningEnabled) {
        // The driveFsBulkPinningEnabled preference did not change.
        return;
      }

      this.isDriveBulkPinningPrefEnabled_ = pref.driveFsBulkPinningEnabled;
      this.reconcile();
    });
  }

  /**
   * Checks if the DlpRestrictedBanner should be shown/hidden based on the
   * latest state and reconciles banners if necessary.
   * @param {!State} state latest state from the store.
   */
  onStateChanged(state) {
    if (this.dialogType_ !== DialogType.SELECT_OPEN_FILE &&
        this.dialogType_ !== DialogType.SELECT_OPEN_MULTI_FILE) {
      return;
    }
    const changedHasDlpDisabledFiles =
        !!state.currentDirectory?.hasDlpDisabledFiles;
    if (this.hasDlpDisabledFiles_ !== changedHasDlpDisabledFiles) {
      this.hasDlpDisabledFiles_ = changedHasDlpDisabledFiles;
      this.reconcile();
    }
  }

  /**
   * Ensure all banners are in priority order and any existing local storage
   * values are retrieved.
   * @return {Promise<void>}
   */
  async initialize() {
    if (!this.disableBannerLoading_) {
      // Banners are initialized in their priority order. The order of the array
      // denotes the priority of the banner, 0th index is highest priority.
      this.setWarningBannersInOrder([
        LocalDiskLowSpaceBannerTagName,
        DriveOutOfOrganizationSpaceBanner,
        DriveOutOfSharedDriveSpaceBanner,
        DriveOutOfIndividualSpaceBanner,
        DriveLowIndividualSpaceBanner,
        DriveLowSharedDriveSpaceBanner,
      ]);

      const educationalBanners =
          util.isGoogleOneOfferFilesBannerEligibleAndEnabled() ?
          [GoogleOneOfferBannerTagName] :
          [DriveWelcomeBannerTagName];
      if (util.isDriveFsBulkPinningEnabled()) {
        educationalBanners.push(DriveBulkPinningBannerTagName);
      }
      educationalBanners.push(HoldingSpaceWelcomeBannerTagName);
      if (!util.isDriveFsBulkPinningEnabled()) {
        educationalBanners.push(DriveOfflinePinningBannerTagName);
      }
      educationalBanners.push(PhotosWelcomeBannerTagName);
      this.setEducationalBannersInOrder(educationalBanners);

      this.setStateBannersInOrder([
        DlpRestrictedBannerName,
        InvalidUSBFileSystemBanner,
        SharedWithCrostiniPluginVmBanner,
        TrashBannerTagName,
      ]);

      // Register custom filters that verify whether the currently navigated
      // path is shared with Crostini, PluginVM or both.
      this.registerCustomBannerFilter_(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () =>
            isPathSharedWithVm(
                this.crostini_, this.currentEntry_,
                constants.DEFAULT_CROSTINI_VM) &&
            isPathSharedWithVm(
                this.crostini_, this.currentEntry_, constants.PLUGIN_VM),
        context: () =>
            ({type: constants.DEFAULT_CROSTINI_VM + constants.PLUGIN_VM}),
      });
      this.registerCustomBannerFilter_(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () => isPathSharedWithVm(
            this.crostini_, this.currentEntry_, constants.DEFAULT_CROSTINI_VM),
        context: () => ({type: constants.DEFAULT_CROSTINI_VM}),
      });
      this.registerCustomBannerFilter_(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () => isPathSharedWithVm(
            this.crostini_, this.currentEntry_, constants.PLUGIN_VM),
        context: () => ({type: constants.PLUGIN_VM}),
      });

      this.registerCustomBannerFilter_(DriveBulkPinningBannerTagName, {
        shouldShow: () => !this.isDriveBulkPinningPrefEnabled_,
        context: () => ({}),
      });

      // Register a custom filter that passes the current size stats down to the
      // the Drive banner only if the volume stats are available. The general
      // volume available handler will run before this ensuring the minimum
      // ratio has been met.
      const notOutOfSpace = () => this.driveQuotaMetadata_ &&
          this.driveQuotaMetadata_.usedBytes <
              this.driveQuotaMetadata_.totalBytes &&
          this.driveQuotaMetadata_.totalBytes >= 0;  // not unlimited
      const outOfSpace = () => this.driveQuotaMetadata_ &&
          this.driveQuotaMetadata_.usedBytes >=
              this.driveQuotaMetadata_.totalBytes &&
          this.driveQuotaMetadata_.totalBytes >= 0;  // not unlimited
      this.registerCustomBannerFilter_(DriveLowIndividualSpaceBanner, {
        shouldShow: notOutOfSpace,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter_(DriveOutOfIndividualSpaceBanner, {
        shouldShow: outOfSpace,
        context: () => ({}),
      });

      this.registerCustomBannerFilter_(DriveOutOfOrganizationSpaceBanner, {
        shouldShow: () => this.driveQuotaMetadata_ &&
            this.driveQuotaMetadata_.organizationLimitExceeded,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter_(DriveLowSharedDriveSpaceBanner, {
        shouldShow: notOutOfSpace,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter_(DriveOutOfSharedDriveSpaceBanner, {
        shouldShow: outOfSpace,
        context: () => ({}),
      });

      // Register a custom filter that checks if the removable device has an
      // error and show the invalid USB file system banner.
      this.registerCustomBannerFilter_(InvalidUSBFileSystemBanner, {
        shouldShow: () => !!(this.currentVolume_ && this.currentVolume_.error),
        context: () => ({error: this.currentVolume_.error}),
      });

      // Register a custom filter that checks if DLP restricted banner should
      // be shown.
      this.registerCustomBannerFilter_(DlpRestrictedBannerName, {
        shouldShow: () =>
            (this.volumeManager_.hasDisabledVolumes() ||
             this.hasDlpDisabledFiles_),
        context: () => ({type: this.dialogType_}),
      });
    }

    for (const banner of this.warningBanners_) {
      this.localStorageCache_[`${banner.tagName}_${LAST_DISMISSED_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${MS_DISPLAYED_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;

      this.maybeAddVolumeSizeObserver_(banner);
    }

    for (const banner of this.educationalBanners_) {
      this.localStorageCache_[`${banner.tagName}_${MS_DISPLAYED_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${DISMISSED_FOREVER_SUFFIX}`] =
          0;

      this.maybeAddVolumeSizeObserver_(banner);
    }

    for (const banner of this.stateBanners_) {
      this.localStorageCache_[`${banner.tagName}_${MS_DISPLAYED_SUFFIX}`] = 0;
      this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`] = 0;

      this.maybeAddVolumeSizeObserver_(banner);
    }

    const cacheKeys = Object.keys(this.localStorageCache_);
    let values = {};
    try {
      values = await storage.local.getAsync(cacheKeys);
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
    const previousVolume = this.currentVolume_;
    const previousSharedDrive = this.currentSharedDrive_;
    this.currentEntry_ = this.directoryModel_.getCurrentDirEntry();
    if (this.currentEntry_) {
      this.currentSharedDrive_ = util.getTeamDriveName(this.currentEntry_);
    }
    this.currentRootType_ = this.directoryModel_.getCurrentRootType();
    this.currentVolume_ = this.directoryModel_.getCurrentVolumeInfo();

    // When navigating to a different volume, refresh the volume size stats
    // when first navigating. A listener will keep this in sync.
    const volumeChanged = this.currentVolume_ &&
        previousVolume?.volumeId !== this.currentVolume_.volumeId &&
        this.volumeSizeObservers_[this.currentVolume_.volumeType];
    const sharedDriveChanged = this.currentSharedDrive_ !== previousSharedDrive;
    if (volumeChanged || sharedDriveChanged) {
      this.pendingVolumeSizeUpdates_.add(this.currentVolume_);
      this.updateVolumeSizeStatsDebounced_.runImmediately();

      // updateVolumeSizeStats will call reconcile at its end. Return here to
      // avoid calling showBanner_ twice for a banner.
      return;
    }

    /** @type {?Banner} */
    let bannerToShow = null;

    // Identify if (given current conditions) any of the banners should be shown
    // or hidden.
    const orderedBanners = this.warningBanners_.concat(
        this.educationalBanners_, this.stateBanners_);
    for (const banner of orderedBanners) {
      if (!this.shouldShowBanner_(banner)) {
        this.hideBannerIfShown_(banner);
        continue;
      }

      // If a higher priority banner has been chosen, hide any lower priority
      // banners that may already be showing.
      if (bannerToShow) {
        this.hideBannerIfShown_(banner);
        continue;
      }

      bannerToShow = banner;
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

    // If a banner has been isolated to be shown for testing, all other banners
    // should not show. The isolated baner should still ensure it should be
    // displayed.
    if (this.isolatedBannerForTesting_ &&
        this.isolatedBannerForTesting_ !== banner.tagName) {
      return false;
    }

    // Check if the banner should be shown on this particular volume type.
    const allowedVolumes = banner.allowedVolumes();
    if (!isAllowedVolume(
            this.currentVolume_, this.currentRootType_, allowedVolumes)) {
      return false;
    }

    // Check if the banner has been dismissed forever.
    if (this.localStorageCache_[`${banner.tagName}_${
            DISMISSED_FOREVER_SUFFIX}`] === 1) {
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

    // Check if the banner has been shown for more than it's required limit.
    // Date.now returns in milliseconds so convert seconds into milliseconds.
    const timeLimitMs = banner.timeLimit() * 1000;
    const totalTimeShownMs =
        this.localStorageCache_[`${banner.tagName}_${MS_DISPLAYED_SUFFIX}`];
    if (timeLimitMs && timeLimitMs < totalTimeShownMs) {
      return false;
    }

    // See if the banner has any custom filters assigned, if the shouldShow
    // method returns true, the banner should be shown and the context is passed
    // to the banner in preparation.
    if (this.customBannerFilters_[banner.tagName]) {
      let shownFilter = false;
      for (const bannerFilter of this.customBannerFilters_[banner.tagName]) {
        if (bannerFilter.shouldShow()) {
          banner.onFilteredContext(bannerFilter.context());
          shownFilter = true;
          break;
        }
      }
      if (!shownFilter) {
        return false;
      }
    }

    return true;
  }

  /**
   * Check if the banner exists (add to DOM if not) and ensure it's visible.
   * @param {!Banner} banner The banner to show.
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

    // If the banner to be shown needs to checkpoint it's time shown, start
    // the checkpoint interval.
    this.resetTimeLimitInterval_();
    if (banner.timeLimit() && banner.timeLimit() !== Banner.INIFINITE_TIME) {
      this.timeLimitInterval_ = setInterval(
          () => this.updateTimeLimit_(banner),
          DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS);
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
    if (!banner.isConnected) {
      return;
    }

    this.resetTimeLimitInterval_();
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
   * Creates all the state banners with the supplied tagName's. This will
   * populate the stateBanners_ array with HTMLElement's.
   * @param {!Array<string>} bannerTagNames The HTMLElement tagName's to create.
   */
  setStateBannersInOrder(bannerTagNames) {
    for (const tagName of bannerTagNames) {
      const banner = /** @type {!Banner} */ (document.createElement(tagName));
      banner.toggleAttribute('hidden', true);
      banner.setAttribute('aria-hidden', 'true');
      this.stateBanners_.push(banner);
    }
  }

  /**
   * Disable the banners entirely from executing
   */
  disableBannersForTesting() {
    this.disableBanners_ = true;
  }

  /**
   * Disable the banners from being loaded for testing. This is used to override
   * the loading of actual banners to load fake banners in unit tests.
   */
  disableBannerLoadingForTesting() {
    this.disableBannerLoading_ = true;
  }

  /**
   * Isolates a banner from the priority list for testing. Used to test
   * functionality of a specific banner in integration tests.
   * @param {string} bannerTagName Banner tagName to isolate.
   */
  async isolateBannerForTesting(bannerTagName) {
    const tagName = bannerTagName.toUpperCase();
    this.isolatedBannerForTesting_ = tagName;
    await this.reconcile();
  }

  /**
   * Clears the time interval and resets the tracked interval and time in ms
   * back to null.
   * @private
   */
  resetTimeLimitInterval_() {
    clearInterval(this.timeLimitInterval_);
    this.timeLimitInterval_ = null;
    this.timeLimitIntervalLastInvokedMs_ = null;
  }

  /**
   * Toggles force show a single banner. If multiple banners are force shown
   * the banner with the highest priority will still be the only one shown.
   * @param {string} bannerTagName The tagName of the banner to force show.
   */
  async toggleBannerForTesting(bannerTagName) {
    const orderedBanners = this.warningBanners_.concat(
        this.educationalBanners_, this.stateBanners_);
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
  async onBannerDismissedClick_(event) {
    if (!event.detail || !event.detail.banner) {
      console.warn('Banner dismiss event missing banner detail');
      return;
    }
    const banner = event.detail.banner;
    // If the banner has been dismissed forever (in the case of educational
    // banners) set the localStorage value to be 1.
    if (event.type === Banner.Event.BANNER_DISMISSED_FOREVER) {
      this.setLocalStorage_(`${banner.tagName}_${DISMISSED_FOREVER_SUFFIX}`, 1);
    } else if (event.type === Banner.Event.BANNER_DISMISSED) {
      // Reset the view counter so that after the dismiss duration elapses the
      // banner can be shown for the showLimit again.
      this.setLocalStorage_(`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`, 0);
      this.setLocalStorage_(
          `${banner.tagName}_${LAST_DISMISSED_SUFFIX}`, Date.now());
    }

    await this.reconcile();
  }

  /**
   * Writes through the localStorage cache to local storage to ensure values
   * are immediately available.
   * @param {string} key The key in local storage to set.
   * @param {number} value The value to set the key to in local storage.
   * @private
   */
  async setLocalStorage_(key, value) {
    if (!this.localStorageCache_.hasOwnProperty(key)) {
      console.warn(`Key ${key} not found in localStorage cache`);
      return;
    }
    this.localStorageCache_[key] = value;
    try {
      await storage.local.setAsync({[key]: value});
    } catch (e) {
      console.warn(e.message);
    }
  }

  /**
   * Registers a custom filter against the specified banner tagName.
   * @param {string} bannerTagName
   * @param {!Banner.CustomFilter} filter
   * @private
   */
  registerCustomBannerFilter_(bannerTagName, filter) {
    // Canonical tagNames are retrieved from the DOM element which transforms
    // them into uppercase (they are supplied in lowercase, as required by the
    // customElement registry).
    const tagName = bannerTagName.toUpperCase();
    if (!this.customBannerFilters_[tagName]) {
      this.customBannerFilters_[tagName] = [];
    }
    this.customBannerFilters_[tagName].push(filter);
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
    this.pendingVolumeSizeUpdates_.add(eventVolumeInfo);
    this.updateVolumeSizeStatsDebounced_.run();
  }

  /**
   * Updates the time limit for the bound banner. Ensures the time limit only
   * loses DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS granularity in the event
   * of a crash or the Files app window is closed.
   * @param {!Banner} banner The banner that requires it's time limit updated.
   * @private
   */
  async updateTimeLimit_(banner) {
    const localStorageKey = `${banner.tagName}_${MS_DISPLAYED_SUFFIX}`;
    const currentDateNowMs = Date.now();
    const durationBannerHasBeenShownMs =
        (this.timeLimitIntervalLastInvokedMs_) ?
        (Date.now() - this.timeLimitIntervalLastInvokedMs_) :
        DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS;
    await this.setLocalStorage_(
        localStorageKey,
        durationBannerHasBeenShownMs +
            this.localStorageCache_[localStorageKey]);
    this.timeLimitIntervalLastInvokedMs_ = currentDateNowMs;

    // Hide the banner if it's reached the time limit.
    if (!this.shouldShowBanner_(banner)) {
      await this.reconcile();
    }
  }

  /**
   * Refresh the volume size stats for all volumeIds in
   * |pendingVolumeSizeUpdate_|.
   * @private
   */
  async updateVolumeSizeStats_() {
    if (this.pendingVolumeSizeUpdates_.size === 0) {
      return;
    }
    for (const {volumeType, volumeId} of this.pendingVolumeSizeUpdates_) {
      if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
        try {
          this.driveQuotaMetadata_ =
              await getDriveQuotaMetadata(assert(this.currentEntry_));
          if (this.driveQuotaMetadata_) {
            this.volumeSizeStats_[volumeId] = {
              totalSize: this.driveQuotaMetadata_.totalBytes,
              remainingSize: this.driveQuotaMetadata_.totalBytes -
                  this.driveQuotaMetadata_.usedBytes,
            };
          }
        } catch (e) {
          console.warn('Error getting drive quota metadata', e);
        }
        continue;
      }

      try {
        const sizeStats = await getSizeStats(volumeId);
        if (!sizeStats || sizeStats.totalSize === 0) {
          continue;
        }
        this.volumeSizeStats_[volumeId] = sizeStats;
      } catch (e) {
        console.warn('Error getting size stats', e);
      }
    }
    this.pendingVolumeSizeUpdates_.clear();
    await this.reconcile();
  }

  /**
   * Listens for localStorage changes to ensure instance cache is in sync.
   * @param {!Object<string, !StorageChange>} changes Changes that occurred.
   * @param {string} areaName One of "sync"|"local"|"managed".
   * @private
   */
  onStorageChanged_(changes, areaName) {
    if (areaName !== 'local') {
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
 * @param {?VolumeManagerCommon.RootType} currentRootType Root type that is
 *    currently navigated.
 * @param {!Array<!Banner.AllowedVolume>} allowedVolumes Array of allowed
 *    volumes.
 * @return {boolean}
 */
export function isAllowedVolume(
    currentVolume, currentRootType, allowedVolumes) {
  let currentVolumeType = null;
  let currentVolumeId = null;
  if (currentVolume) {
    currentVolumeType = currentVolume.volumeType;
    currentVolumeId = currentVolume.volumeId;
  }
  for (let i = 0; i < allowedVolumes.length; i++) {
    const {type, id, root} = allowedVolumes[i];
    if (!type && !root) {
      continue;
    }
    if (type && currentVolumeType !== type) {
      continue;
    }
    if (root && currentRootType !== root) {
      continue;
    }
    if (id && currentVolumeId !== id) {
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
  if (util.isNullOrUndefined(sizeStats.remainingSize) ||
      util.isNullOrUndefined(sizeStats.totalSize)) {
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
 * Identifies if a supplied Entry is shared with a particularly VM. Returns a
 * curried function that takes the vm type.
 * @param {!Crostini} crostini
 * @param {?DirectoryEntry|?FakeEntry|?FilesAppDirEntry} entry
 * @param {string} vmType
 * @returns {boolean}
 */
function isPathSharedWithVm(crostini, entry, vmType) {
  if (!crostini.isEnabled(vmType)) {
    return false;
  }
  if (!entry) {
    return false;
  }
  return crostini.isPathShared(vmType, /** @type {!Entry} */ (entry));
}
