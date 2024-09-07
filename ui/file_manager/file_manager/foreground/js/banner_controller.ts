// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {Crostini} from '../../background/js/crostini.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getDriveQuotaMetadata, getSizeStats} from '../../common/js/api.js';
import {RateLimiter} from '../../common/js/async_util.js';
import {getTeamDriveName, isFakeEntry} from '../../common/js/entry_utils.js';
import type {FakeEntry, FilesAppDirEntry} from '../../common/js/files_app_entry_types.js';
import {isGoogleOneOfferFilesBannerEligibleAndEnabled} from '../../common/js/flags.js';
import {storage} from '../../common/js/storage.js';
import {isNullOrUndefined} from '../../common/js/util.js';
import type {RootType} from '../../common/js/volume_manager_types.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';
import {DialogType, type State} from '../../state/state.js';
import {getStore, type Store} from '../../state/store.js';

import {DEFAULT_CROSTINI_VM, PLUGIN_VM} from './constants.js';
import type {DirectoryModel} from './directory_model.js';
import {TAG_NAME as DlpRestrictedBannerName} from './ui/banners/dlp_restricted_banner.js';
import {TAG_NAME as DriveBulkPinningBannerTagName} from './ui/banners/drive_bulk_pinning_banner.js';
import {TAG_NAME as DriveLowIndividualSpaceBanner} from './ui/banners/drive_low_individual_space_banner.js';
import {TAG_NAME as DriveLowSharedDriveSpaceBanner} from './ui/banners/drive_low_shared_drive_space_banner.js';
import {TAG_NAME as DriveOfflinePinningBannerTagName} from './ui/banners/drive_offline_pinning_banner.js';
import {TAG_NAME as DriveOutOfIndividualSpaceBanner} from './ui/banners/drive_out_of_individual_space_banner.js';
import {TAG_NAME as DriveOutOfOrganizationSpaceBanner} from './ui/banners/drive_out_of_organization_space_banner.js';
import {TAG_NAME as DriveOutOfSharedDriveSpaceBanner} from './ui/banners/drive_out_of_shared_drive_space_banner.js';
import {TAG_NAME as DriveWelcomeBannerTagName} from './ui/banners/drive_welcome_banner.js';
import {TAG_NAME as FilesMigratingToCloudBannerTagName} from './ui/banners/files_migrating_to_cloud_banner.js';
import {TAG_NAME as GoogleOneOfferBannerTagName} from './ui/banners/google_one_offer_banner.js';
import {TAG_NAME as HoldingSpaceWelcomeBannerTagName} from './ui/banners/holding_space_welcome_banner.js';
import {TAG_NAME as InvalidUsbFileSystemBannerTagName} from './ui/banners/invalid_usb_filesystem_banner.js';
import {TAG_NAME as LocalDiskLowSpaceBannerTagName} from './ui/banners/local_disk_low_space_banner.js';
import {TAG_NAME as PhotosWelcomeBannerTagName} from './ui/banners/photos_welcome_banner.js';
import {TAG_NAME as SharedWithCrostiniPluginVmBanner} from './ui/banners/shared_with_crostini_pluginvm_banner.js';
import {TAG_NAME as TrashBannerTagName} from './ui/banners/trash_banner.js';
import type {Banner} from './ui/banners/types.js';
import {type AllowedVolumeOrType, BANNER_INFINITE_TIME, BannerEvent, type MinDiskThreshold} from './ui/banners/types.js';

/**
 * Local storage key suffix for how many times a banner was shown.
 */
const VIEW_COUNTER_SUFFIX = '_VIEW_COUNTER';

/**
 * Local storage key suffix for the last Date a banner was dismissed.
 */
const LAST_DISMISSED_SUFFIX = '_LAST_DISMISSED';

/**
 * Local storage key suffix that stores the total number of seconds a banner has
 * been visible for.
 */
const MS_DISPLAYED_SUFFIX = '_SECONDS_DISPLAYED';

/**
 * Duration between calls to keep the current banners time limit in sync.
 */
const DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS = 10000;

/**
 * Local storage key suffix for a banner that has been dismissed forever.
 */
const DISMISSED_FOREVER_SUFFIX = '_DISMISSED_FOREVER';

/**
 * The HTML attribute to force show a banner, if applied, the banner will always
 * show.
 */
const _BANNER_FORCE_SHOW_ATTRIBUTE = 'force-show-for-testing';

/**
 * Allowed duration between onDirectorySizeChanged events in milliseconds.
 */
const MIN_INTERVAL_BETWEEN_DIRECTORY_SIZE_CHANGED_EVENTS = 5000;

/**
 * Type defining the object that stores the volume size stats for various volume
 * types that are tracked by banners.
 * The key of this is the volume ID.
 */
interface VolumeSizeStats {
  [key: string]: chrome.fileManagerPrivate.MountPointSizeStats|undefined;
}

/**
 * A custom filter context that is set when the existing filters are not
 * powerful enough and more custom behaviour must be used to define when a
 * banner should be shown or not.
 */
interface CustomFilter {
  shouldShow: () => boolean | undefined;
  context: () => void;
}

/**
 * The central component to the Banners Framework. The controller maintains the
 * core logic that dictates which banner should be shown as well as what events
 * require a reconciliation of the banners to ensure the right banner is shown
 * at the right time.
 */
export class BannerController extends EventTarget {
  /**
   * Warning banners ordered by priority. Index 0 is the highest priority.
   */
  private warningBanners_: Banner[] = [];

  /**
   * Educational banners ordered by priority. Index 0 is the highest
   * priority.
   */
  private educationalBanners_: Banner[] = [];

  /**
   * State banners ordered by priority. Index 0 is the highest priority.
   */
  private stateBanners_: Banner[] = [];

  /**
   * Keep track of banners that subscribe to volume changes.
   */
  private volumeSizeObservers_: {[key: string]: Banner[]} = {};

  /**
   * Stores the state of each banner, such as view count or last dismissed
   * time. This is kept in sync with local storage.
   */
  private localStorageCache_: {[key: string]: number} = {};

  /**
   * Maintains the state of the current volume that has been navigated. This
   * is updated by the directory-changed event.
   */
  private currentVolume_: VolumeInfo|null = null;

  /**
   * Maintains the currently navigated root type. This is updated by the
   * directory-changed event.
   */
  private currentRootType_: RootType|null = null;

  /**
   * Maintains the currently navigated shared drive if any. This is updated
   * when a reconcile event is called.
   */
  private currentSharedDrive_ = '';

  /**
   * Maintains the currently navigated directory entry. This is updated when
   * a reconcile event is called.
   */
  private currentEntry_: DirectoryEntry|FakeEntry|FilesAppDirEntry|undefined =
      undefined;

  /**
   * Maintains a cache of the current size for all observed volumes. If a
   * banner requests to observe a volumeType on initialization, the volume
   * size is cached here, keyed by volumeId.
   */
  private volumeSizeStats_: VolumeSizeStats = {};

  /**
   * Maintains a cache of the user's Google Drive quota and associated
   * metadata.
   */
  private driveQuotaMetadata_?: chrome.fileManagerPrivate.DriveQuotaMetadata;

  /**
   * The container where all the banners will be appended to.
   */
  private container_: HTMLDivElement =
      document.querySelector<HTMLDivElement>('#banners')!;

  /**
   * Whether banners should be loaded or not during for unit tests.
   */
  private disableBannerLoading_ = false;

  /**
   * Whether banners should be completely disabled, useful to remove banners
   * during integration tests or tast tests.
   */
  private disableBanners_ = false;

  /**
   * A single banner to isolate and test it's functionality. Denoted by it's
   * tagName (in uppercase).
   */
  private isolatedBannerForTesting_: string|null = null;

  /**
   * setInterval handle that keeps track of the total time a banner has
   * been shown for.
   */
  private timeLimitInterval_?: number = undefined;

  /**
   * Last time that the setInterval was invoked.
   */
  private timeLimitIntervalLastInvokedMs_: number|null = null;

  /**
   * An object keyed by a banners tagName (in upper case) that lists custom
   * filters for the specified banner. Used to house banner specific logic
   * that can decide whether to display a banner or not.
   */
  private customBannerFilters_: {[key: string]: CustomFilter[]} = {};

  /**
   * The instance of the store.
   */
  private store_: Store = getStore();

  /**
   * Cached value of `this.store_.currentDirectory.hasDisabledFiles`, to avoid
   * unnecessary reconciling.
   */
  private hasDlpDisabledFiles_ = false;

  /**
   * The volumeId that is pending a volume size update, updateVolumeSizeStats_
   * will remove the volumeId once updated. This is cleared when the debounced
   * version of updateVolumeSizeStats_ executes.
   */
  private pendingVolumeSizeUpdates_ = new Set<VolumeInfo>();

  /**
   * Bind the onDirectorySizeChanged_ method to this instance once.
   */
  private onDirectorySizeChangedBound_ =
      async (event: chrome.fileManagerPrivate.FileWatchEvent) =>
          this.onDirectorySizeChanged_(event);

  /**
   * Debounced version of updateVolumeSizeStats_ to stop overly aggressive
   * calls coming from onDirectoryChanged_.
   */
  private updateVolumeSizeStatsDebounced_ = new RateLimiter(
      async () => this.updateVolumeSizeStats_(),
      MIN_INTERVAL_BETWEEN_DIRECTORY_SIZE_CHANGED_EVENTS);

  /**
   * Whether the Drive bulk-pinning feature is available on this device.
   */
  private bulkPinningAvailable_ = false;

  /**
   * Whether the Drive bulk-pinning feature is currently enabled.
   */
  private bulkPinningEnabled_ = false;

  /**
   * SkyVault migration destination. If set, one of {Google Drive, OneDrive}.
   */
  private migrationDestination_: chrome.fileManagerPrivate.CloudProvider =
      chrome.fileManagerPrivate.CloudProvider.NOT_SPECIFIED;

  constructor(
      private directoryModel_: DirectoryModel,
      private volumeManager_: VolumeManager, private crostini_: Crostini,
      private dialogType_: DialogType) {
    super();

    // Ensure changes are received for store updates.
    this.store_.subscribe(this);

    // Only attach event listeners if the controller is enabled. Used to disable
    // all banners from being loaded.
    if (!this.disableBanners_) {
      storage.onChanged.addListener(this.onStorageChanged_.bind(this));
      this.directoryModel_.addEventListener(
          'directory-changed', (_event: Event) => this.onDirectoryChanged_());
    }

    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  private onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(pref => {
      if (this.bulkPinningAvailable_ !== pref.driveFsBulkPinningAvailable ||
          this.bulkPinningEnabled_ !== pref.driveFsBulkPinningEnabled ||
          this.migrationDestination_ !== pref.skyVaultMigrationDestination) {
        this.bulkPinningAvailable_ = pref.driveFsBulkPinningAvailable;
        this.bulkPinningEnabled_ = pref.driveFsBulkPinningEnabled;
        this.migrationDestination_ = pref.skyVaultMigrationDestination;
        this.reconcile();
      }
    });
  }

  /**
   * Checks if the DlpRestrictedBanner should be shown/hidden based on the
   * latest state and reconciles banners if necessary.
   */
  onStateChanged(state: State) {
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
   */
  async initialize() {
    if (!this.disableBannerLoading_) {
      // Banners are initialized in their priority order. The order of the array
      // denotes the priority of the banner, 0th index is highest priority.
      this.setWarningBannersInOrder([
        FilesMigratingToCloudBannerTagName,
        LocalDiskLowSpaceBannerTagName,
        DriveOutOfOrganizationSpaceBanner,
        DriveOutOfSharedDriveSpaceBanner,
        DriveOutOfIndividualSpaceBanner,
        DriveLowIndividualSpaceBanner,
        DriveLowSharedDriveSpaceBanner,
      ]);

      const educationalBanners =
          isGoogleOneOfferFilesBannerEligibleAndEnabled() ?
          [GoogleOneOfferBannerTagName] :
          [DriveWelcomeBannerTagName];

      educationalBanners.push(DriveBulkPinningBannerTagName);
      educationalBanners.push(HoldingSpaceWelcomeBannerTagName);
      educationalBanners.push(DriveOfflinePinningBannerTagName);
      educationalBanners.push(PhotosWelcomeBannerTagName);
      this.setEducationalBannersInOrder(educationalBanners);

      this.setStateBannersInOrder([
        DlpRestrictedBannerName,
        InvalidUsbFileSystemBannerTagName,
        SharedWithCrostiniPluginVmBanner,
        TrashBannerTagName,
      ]);

      // Register custom filters that verify whether the currently navigated
      // path is shared with Crostini, PluginVM or both.
      this.registerCustomBannerFilter(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () =>
            isPathSharedWithVm(
                this.crostini_, this.currentEntry_, DEFAULT_CROSTINI_VM) &&
            isPathSharedWithVm(this.crostini_, this.currentEntry_, PLUGIN_VM),
        context: () => ({type: DEFAULT_CROSTINI_VM + PLUGIN_VM}),
      });
      this.registerCustomBannerFilter(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () => isPathSharedWithVm(
            this.crostini_, this.currentEntry_, DEFAULT_CROSTINI_VM),
        context: () => ({type: DEFAULT_CROSTINI_VM}),
      });
      this.registerCustomBannerFilter(SharedWithCrostiniPluginVmBanner, {
        shouldShow: () =>
            isPathSharedWithVm(this.crostini_, this.currentEntry_, PLUGIN_VM),
        context: () => ({type: PLUGIN_VM}),
      });

      this.registerCustomBannerFilter(DriveBulkPinningBannerTagName, {
        shouldShow: () =>
            this.bulkPinningAvailable_ && !this.bulkPinningEnabled_,
        context: () => ({}),
      });

      this.registerCustomBannerFilter(DriveOfflinePinningBannerTagName, {
        shouldShow: () => !this.bulkPinningAvailable_,
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
      this.registerCustomBannerFilter(DriveLowIndividualSpaceBanner, {
        shouldShow: notOutOfSpace,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter(DriveOutOfIndividualSpaceBanner, {
        shouldShow: outOfSpace,
        context: () => ({}),
      });

      this.registerCustomBannerFilter(DriveOutOfOrganizationSpaceBanner, {
        shouldShow: () => this.driveQuotaMetadata_ &&
            this.driveQuotaMetadata_.organizationLimitExceeded,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter(DriveLowSharedDriveSpaceBanner, {
        shouldShow: notOutOfSpace,
        context: () => this.driveQuotaMetadata_,
      });

      this.registerCustomBannerFilter(DriveOutOfSharedDriveSpaceBanner, {
        shouldShow: outOfSpace,
        context: () => ({}),
      });

      // Register a custom filter that checks if the removable device has an
      // error and show the invalid USB file system banner.
      this.registerCustomBannerFilter(InvalidUsbFileSystemBannerTagName, {
        shouldShow: () => !!(this.currentVolume_?.error),
        context: () => ({error: this.currentVolume_?.error}),
      });

      // Register a custom filter that checks if DLP restricted banner should
      // be shown.
      this.registerCustomBannerFilter(DlpRestrictedBannerName, {
        shouldShow: () =>
            (this.volumeManager_.hasDisabledVolumes() ||
             this.hasDlpDisabledFiles_),
        context: () => ({type: this.dialogType_}),
      });

      this.registerCustomBannerFilter(FilesMigratingToCloudBannerTagName, {
        shouldShow: () => this.migrationDestination_ !==
            chrome.fileManagerPrivate.CloudProvider.NOT_SPECIFIED,
        context: () => ({cloudProvider: this.migrationDestination_}),
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
    let values: {[key: string]: any} = {};
    try {
      values = await storage.local.getAsync(cacheKeys);
    } catch (e) {
      console.warn((e as Error).message);
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
      this.currentSharedDrive_ = getTeamDriveName(this.currentEntry_);
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
      if (this.currentVolume_) {
        this.pendingVolumeSizeUpdates_.add(this.currentVolume_);
      }
      this.updateVolumeSizeStatsDebounced_.runImmediately();

      // updateVolumeSizeStats will call reconcile at its end. Return here to
      // avoid calling showBanner_ twice for a banner.
      return;
    }

    let bannerToShow: Banner|null = null;

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
   */
  private shouldShowBanner_(banner: Banner) {
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
    const showLimit = banner.showLimit && banner.showLimit();
    if (showLimit) {
      const timesShown =
          this.localStorageCache_[`${banner.tagName}_${VIEW_COUNTER_SUFFIX}`];
      if (timesShown && (timesShown >= showLimit) && !banner.isConnected) {
        return false;
      }
    }

    // Check if the threshold has been breached for the banner to be shown.
    const diskThreshold = banner.diskThreshold && banner.diskThreshold();
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
        banner.hideAfterDismissedDurationSeconds &&
        (banner.hideAfterDismissedDurationSeconds() * 1000);
    const lastDismissedMilliseconds =
        this.localStorageCache_[`${banner.tagName}_${LAST_DISMISSED_SUFFIX}`];
    if (hideAfterDismissedDurationSeconds &&
        (lastDismissedMilliseconds &&
         ((Date.now() - lastDismissedMilliseconds) <
          hideAfterDismissedDurationSeconds))) {
      return false;
    }

    // Check if the banner has been shown for more than it's required limit.
    // Date.now returns in milliseconds so convert seconds into milliseconds.
    const timeLimitMs = banner.timeLimit && (banner.timeLimit() * 1000);
    const totalTimeShownMs =
        this.localStorageCache_[`${banner.tagName}_${MS_DISPLAYED_SUFFIX}`];
    if (timeLimitMs && (totalTimeShownMs && timeLimitMs < totalTimeShownMs)) {
      return false;
    }

    // See if the banner has any custom filters assigned, if the shouldShow
    // method returns true, the banner should be shown and the context is passed
    // to the banner in preparation.
    if (this.customBannerFilters_[banner.tagName]) {
      let shownFilter = false;
      for (const bannerFilter of this.customBannerFilters_[banner.tagName]!) {
        if (bannerFilter.shouldShow()) {
          if (banner.onFilteredContext) {
            banner.onFilteredContext(bannerFilter.context());
          }
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
   */
  private async showBanner_(banner: Banner) {
    if (!banner.isConnected) {
      this.container_.appendChild(banner);

      // Views are set when the banner is first appended to the DOM. This
      // denotes a new app session.
      if (banner.showLimit && banner.showLimit()) {
        const localStorageKey = `${banner.tagName}_${VIEW_COUNTER_SUFFIX}`;
        await this.setLocalStorage_(
            localStorageKey,
            (this.localStorageCache_[localStorageKey] || 0) + 1);
      }
    }

    // If the banner to be shown needs to checkpoint it's time shown, start
    // the checkpoint interval.
    this.resetTimeLimitInterval_();
    if (banner.timeLimit &&
        (banner.timeLimit() && banner.timeLimit() !== BANNER_INFINITE_TIME)) {
      this.timeLimitInterval_ = setInterval(
          () => this.updateTimeLimit(banner),
          DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS);
    }

    banner.removeAttribute('hidden');
    banner.setAttribute('aria-hidden', 'false');

    banner.onShow && banner.onShow();
  }

  /**
   * Hide the banner if it exists in the DOM.
   */
  private hideBannerIfShown_(banner: Banner) {
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
   */
  private maybeAddVolumeSizeObserver_(banner: Banner) {
    if (!banner.diskThreshold || !banner.diskThreshold()) {
      return;
    }

    const diskThreshold = banner.diskThreshold()!;
    if (!this.volumeSizeObservers_[diskThreshold.type]) {
      this.volumeSizeObservers_[diskThreshold.type] = [];
    }

    this.volumeSizeObservers_[diskThreshold.type]!.push(banner);
  }

  /**
   * Creates all the warning banners with the supplied tagName's. This will
   * populate the warningBanners_ array with HTMLElement's.
   */
  setWarningBannersInOrder(bannerTagNames: string[]) {
    for (const tagName of bannerTagNames) {
      const banner = document.createElement(tagName) as Banner;
      banner.toggleAttribute('hidden', true);
      banner.setAttribute('aria-hidden', 'true');
      banner.addEventListener(
          BannerEvent.BANNER_DISMISSED,
          event => this.onBannerDismissedClick_(event as BannerDismissedEvent));
      this.warningBanners_.push(banner);
    }
  }

  /**
   * Creates all the educational banners with the supplied tagName's. This will
   * populate the educationalBanners_ array with HTMLElement's.
   */
  setEducationalBannersInOrder(bannerTagNames: string[]) {
    for (const tagName of bannerTagNames) {
      const banner = document.createElement(tagName) as Banner;
      banner.toggleAttribute('hidden', true);
      banner.setAttribute('aria-hidden', 'true');
      banner.addEventListener(
          BannerEvent.BANNER_DISMISSED_FOREVER,
          event => this.onBannerDismissedClick_(event as BannerDismissedEvent));
      this.educationalBanners_.push(banner);
    }
  }

  /**
   * Creates all the state banners with the supplied tagName's. This will
   * populate the stateBanners_ array with HTMLElement's.
   */
  setStateBannersInOrder(bannerTagNames: string[]) {
    for (const tagName of bannerTagNames) {
      const banner = document.createElement(tagName) as Banner;
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
   */
  async isolateBannerForTesting(bannerTagName: string) {
    const tagName = bannerTagName.toUpperCase();
    this.isolatedBannerForTesting_ = tagName;
    await this.reconcile();
  }

  /**
   * Clears the time interval and resets the tracked interval and time in ms
   * back to null.
   * @private
   */
  private resetTimeLimitInterval_() {
    clearInterval(this.timeLimitInterval_);
    this.timeLimitInterval_ = undefined;
    this.timeLimitIntervalLastInvokedMs_ = null;
  }

  /**
   * Toggles force show a single banner. If multiple banners are force shown
   * the banner with the highest priority will still be the only one shown.
   */
  async toggleBannerForTesting(bannerTagName: string) {
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
   */
  private async onBannerDismissedClick_(event: BannerDismissedEvent) {
    if (!event.detail || !event.detail.banner) {
      console.warn('Banner dismiss event missing banner detail');
      return;
    }
    const banner = event.detail.banner;
    // If the banner has been dismissed forever (in the case of educational
    // banners) set the localStorage value to be 1.
    if (event.type === BannerEvent.BANNER_DISMISSED_FOREVER) {
      this.setLocalStorage_(`${banner.tagName}_${DISMISSED_FOREVER_SUFFIX}`, 1);
    } else if (event.type === BannerEvent.BANNER_DISMISSED) {
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
   */
  private async setLocalStorage_(key: string, value: number) {
    if (!this.localStorageCache_.hasOwnProperty(key)) {
      console.warn(`Key ${key} not found in localStorage cache`);
      return;
    }
    this.localStorageCache_[key] = value;
    try {
      await storage.local.setAsync({[key]: value});
    } catch (e) {
      console.warn((e as Error).message);
    }
  }

  /**
   * Registers a custom filter against the specified banner tagName.
   */
  registerCustomBannerFilter(bannerTagName: string, filter: CustomFilter) {
    // Canonical tagNames are retrieved from the DOM element which transforms
    // them into uppercase (they are supplied in lowercase, as required by the
    // customElement registry).
    const tagName = bannerTagName.toUpperCase();
    if (!this.customBannerFilters_[tagName]) {
      this.customBannerFilters_[tagName] = [];
    }
    this.customBannerFilters_[tagName]!.push(filter);
  }

  /**
   * Invoked when a directory has been changed, used to update the local cache
   * and reconcile the current banners being shown.
   */
  private async onDirectoryChanged_() {
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
   */
  private async onDirectorySizeChanged_(
      event: chrome.fileManagerPrivate.FileWatchEvent) {
    if (!event.entry) {
      return;
    }
    const eventVolumeInfo = this.volumeManager_.getVolumeInfo(event.entry);
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
   */
  async updateTimeLimit(banner: Banner) {
    const localStorageKey = `${banner.tagName}_${MS_DISPLAYED_SUFFIX}`;
    const currentDateNowMs = Date.now();
    const durationBannerHasBeenShownMs =
        (this.timeLimitIntervalLastInvokedMs_) ?
        (Date.now() - this.timeLimitIntervalLastInvokedMs_) :
        DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS;
    await this.setLocalStorage_(
        localStorageKey,
        durationBannerHasBeenShownMs +
            (this.localStorageCache_[localStorageKey] || 0));
    this.timeLimitIntervalLastInvokedMs_ = currentDateNowMs;

    // Hide the banner if it's reached the time limit.
    if (!this.shouldShowBanner_(banner)) {
      await this.reconcile();
    }
  }

  /**
   * Refresh the volume size stats for all volumeIds in
   * |pendingVolumeSizeUpdate_|.
   */
  private async updateVolumeSizeStats_() {
    if (this.pendingVolumeSizeUpdates_.size === 0) {
      return;
    }
    for (const {volumeType, volumeId} of this.pendingVolumeSizeUpdates_) {
      if (volumeType === VolumeType.DRIVE) {
        try {
          if (!this.currentEntry_ || isFakeEntry(this.currentEntry_)) {
            continue;
          }
          this.driveQuotaMetadata_ =
              await getDriveQuotaMetadata(this.currentEntry_);
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
   */
  private onStorageChanged_(changes: {[key: string]: any}, areaName: string) {
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
 */
export function isAllowedVolume(
    currentVolume: VolumeInfo|null, currentRootType: RootType|null,
    allowedVolumes: AllowedVolumeOrType[]) {
  let currentVolumeType = null;
  let currentVolumeId = null;
  if (currentVolume) {
    currentVolumeType = currentVolume.volumeType;
    currentVolumeId = currentVolume.volumeId;
  }
  for (let i = 0; i < allowedVolumes.length; i++) {
    const allowedVolume = allowedVolumes[i]!;
    if (!('root' in allowedVolume) && !('type' in allowedVolume)) {
      continue;
    }
    if (('type' in allowedVolume) && currentVolumeType !== allowedVolume.type) {
      continue;
    }
    if (('root' in allowedVolume) && currentRootType !== allowedVolume.root) {
      continue;
    }
    if (('id' in allowedVolume) && currentVolumeId !== allowedVolume.id) {
      continue;
    }
    return true;
  }
  return false;
}

/**
 * Checks if the current sizeStats are below the threshold required to trigger
 * the banner to show.
 */
export function isBelowThreshold(
    threshold: MinDiskThreshold,
    sizeStats?: chrome.fileManagerPrivate.MountPointSizeStats|null) {
  if (!threshold || !sizeStats) {
    return false;
  }
  if (isNullOrUndefined(sizeStats.remainingSize) ||
      isNullOrUndefined(sizeStats.totalSize)) {
    return false;
  }
  if (('minSize' in threshold) && threshold.minSize < sizeStats.remainingSize) {
    return false;
  }
  const currentRatio = sizeStats.remainingSize / sizeStats.totalSize;
  if (('minRatio' in threshold) && threshold.minRatio < currentRatio) {
    return false;
  }
  return true;
}

/**
 * Identifies if a supplied Entry is shared with a particularly VM. Returns a
 * curried function that takes the vm type.
 */
function isPathSharedWithVm(
    crostini: Crostini,
    entry: DirectoryEntry|FakeEntry|FilesAppDirEntry|undefined,
    vmType: string) {
  if (!crostini.isEnabled(vmType)) {
    return false;
  }
  if (!entry) {
    return false;
  }
  return crostini.isPathShared(vmType, entry as FileSystemEntry);
}
