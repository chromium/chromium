// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Key in localStorage to keep number of times the Drive Welcome
 * banner has shown.
 */
const WELCOME_HEADER_COUNTER_KEY = 'driveWelcomeHeaderCounter';

// If the warning was dismissed before, this key stores the quota value
// (as of the moment of dismissal).
// If the warning was never dismissed or was reset this key stores 0.
const DRIVE_WARNING_DISMISSED_KEY = 'driveSpaceWarningDismissed';

/**
 * If the warning was dismissed before, this key stores the moment of dismissal
 * in milliseconds since the UNIX epoch (i.e. the value of Date.now()).
 * @const {string}
 */
const DOWNLOADS_WARNING_DISMISSED_KEY = 'downloadsSpaceWarningDismissed';

/**
 * Key in localStorage to store the number of times the holding space welcome
 * banner has shown. Note that if the user explicitly dismisses the banner then
 * the value at this key will be `HOLDING_SPACE_WELCOME_BANNER_COUNTER_LIMIT`.
 * @type {string}
 */
const HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY =
    'holdingSpaceWelcomeBannerCounter';

/**
 * Key in localStorage to store the number of sessions the Offline Info banner
 * message has shown in.
 */
const OFFLINE_INFO_BANNER_COUNTER_KEY = 'driveOfflineInfoBannerCounter';

/**
 * Maximum times the holding space welcome banner could have shown.
 * @type {number}
 */
const HOLDING_SPACE_WELCOME_BANNER_COUNTER_LIMIT = 3;

/**
 * Maximum times Drive Welcome banner could have shown.
 */
const WELCOME_HEADER_COUNTER_LIMIT = 25;

/**
 * If the remaining space in Drive is less than 10%, we'll show a warning.
 * @const {number}
 */
const DRIVE_SPACE_WARNING_THRESHOLD_RATIO = 0.1;

/**
 * If the remaining space in Downloads is less than 1GB, we'll show a warning.
 * This value is in bytes.
 * @const {number}
 */
const DOWNLOADS_SPACE_WARNING_THRESHOLD_SIZE = 1 * 1024 * 1024 * 1024;

/**
 * After the warning banner is dismissed, we won't show it for 36 hours.
 * This value is in milliseconds.
 * @const {number}
 */
const DOWNLOADS_SPACE_WARNING_DISMISS_DURATION = 36 * 60 * 60 * 1000;

/**
 * Maximum sessions the Offline Info banner should be shown.
 */
const OFFLINE_INFO_BANNER_COUNTER_LIMIT = 3;

/**
 * Responsible for showing following banners in the file list.
 *  - WelcomeBanner
 *  - AuthFailBanner
 */
class Banners extends cr.EventTarget {
  /**
   * @param {DirectoryModel} directoryModel The model.
   * @param {!VolumeManager} volumeManager The manager.
   * @param {Document} document HTML document.
   * @param {boolean} showWelcome True if the welcome banner can be shown.
   */
  constructor(directoryModel, volumeManager, document, showWelcome) {
    super();

    this.directoryModel_ = directoryModel;
    this.volumeManager_ = volumeManager;
    this.document_ = assert(document);
    this.showWelcome_ = showWelcome;

    /** @private {boolean} */
    this.previousDirWasOnDrive_ = false;

    this.privateOnDirectoryChangedBound_ =
        this.privateOnDirectoryChanged_.bind(this);

    const handler = () => {
      this.maybeShowDriveBanners_();
      this.maybeShowHoldingSpaceWelcomeBanner_();
    };

    this.directoryModel_.addEventListener('scan-completed', handler);
    this.directoryModel_.addEventListener('rescan-completed', handler);
    this.directoryModel_.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));

    this.unmountedPanel_ = this.document_.querySelector('#unmounted-panel');
    this.volumeManager_.volumeInfoList.addEventListener(
        'splice', this.onVolumeInfoListSplice_.bind(this));
    this.volumeManager_.addEventListener(
        'drive-connection-changed', this.onDriveConnectionChanged_.bind(this));

    chrome.storage.onChanged.addListener(this.onStorageChange_.bind(this));

    /** @private {number} */
    this.holdingSpaceWelcomeBannerCounter_ =
        HOLDING_SPACE_WELCOME_BANNER_COUNTER_LIMIT;

    this.welcomeHeaderCounter_ = WELCOME_HEADER_COUNTER_LIMIT;
    this.warningDismissedCounter_ = 0;
    this.downloadsWarningDismissedTime_ = 0;

    /**
     * Number of sessions the offline info banner has been shown in already.
     * @private {!number}
     */
    this.offlineInfoBannerCounter_ = 0;

    /**
     * Whether or not the offline info banner has been shown this session.
     * @private {!boolean}
     */
    this.hasShownOfflineInfoBanner_ = false;

    this.ready_ = new Promise((resolve, reject) => {
      chrome.storage.local.get(
          [
            HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY,
            WELCOME_HEADER_COUNTER_KEY,
            DRIVE_WARNING_DISMISSED_KEY,
            DOWNLOADS_WARNING_DISMISSED_KEY,
            OFFLINE_INFO_BANNER_COUNTER_KEY,
          ],
          values => {
            if (chrome.runtime.lastError) {
              reject(
                  'Failed to load banner data from chrome.storage: ' +
                  chrome.runtime.lastError.message);
              return;
            }
            this.holdingSpaceWelcomeBannerCounter_ =
                parseInt(
                    values[HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY], 10) ||
                0;
            this.welcomeHeaderCounter_ =
                parseInt(values[WELCOME_HEADER_COUNTER_KEY], 10) || 0;
            this.warningDismissedCounter_ =
                parseInt(values[DRIVE_WARNING_DISMISSED_KEY], 10) || 0;
            this.downloadsWarningDismissedTime_ =
                parseInt(values[DOWNLOADS_WARNING_DISMISSED_KEY], 10) || 0;
            this.offlineInfoBannerCounter_ =
                parseInt(values[OFFLINE_INFO_BANNER_COUNTER_KEY], 10) || 0;

            // If it's in test, override the counter to show the header by
            // force.
            if (chrome.test) {
              this.holdingSpaceWelcomeBannerCounter_ = 0;
              this.welcomeHeaderCounter_ = 0;
              this.warningDismissedCounter_ = 0;
              this.offlineInfoBannerCounter_ = 0;
            }
            resolve();
          });
    });

    // Authentication failed banner.
    this.authFailedBanner_ =
        this.document_.querySelector('#drive-auth-failed-warning');
    const authFailedText = this.authFailedBanner_.querySelector('.drive-text');
    authFailedText.innerHTML = util.htmlUnescape(str('DRIVE_NOT_REACHED'));
    authFailedText.querySelector('a').addEventListener('click', e => {
      chrome.fileManagerPrivate.logoutUserForReauthentication();
      e.preventDefault();
    });
    this.maybeShowAuthFailBanner_();

    /**
     * Banner informing user they can make elements available offline.
     * @private {!HTMLElement}
     * @const
     */
    this.offlineInfoBanner_ = queryRequiredElement('#offline-info-banner');
    util.setClampLine(
        queryRequiredElement('.body2-primary', this.offlineInfoBanner_), '2');
    queryRequiredElement('#offline-learn-more').addEventListener('click', e => {
      util.visitURL(str('GOOGLE_DRIVE_OFFLINE_HELP_URL'));
      this.setOfflineInfoBannerCounter_(OFFLINE_INFO_BANNER_COUNTER_LIMIT);
      this.offlineInfoBanner_.hidden = true;
      e.preventDefault();
    });

    /** @const @private {!HTMLElement} */
    this.holdingSpaceWelcomeBanner_ =
        queryRequiredElement('.holding-space-welcome', this.document_);
  }

  /**
   * @param {number} value How many times the holding space welcome banner
   * has shown.
   * @private
   */
  setHoldingSpaceWelcomeBannerCounter_(value) {
    const values = {};
    values[HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY] = value;
    chrome.storage.local.set(values);
  }

  /**
   * @param {number} value How many times the Drive Welcome header banner
   * has shown.
   * @private
   */
  setWelcomeHeaderCounter_(value) {
    const values = {};
    values[WELCOME_HEADER_COUNTER_KEY] = value;
    chrome.storage.local.set(values);
  }

  /**
   * @param {number} value How many times the low space warning has dismissed.
   * @private
   */
  setWarningDismissedCounter_(value) {
    const values = {};
    values[DRIVE_WARNING_DISMISSED_KEY] = value;
    chrome.storage.local.set(values);
  }

  /**
   * @param {number} value How many sessions the Offline Info banner has shown
   * in.
   * @private
   */
  setOfflineInfoBannerCounter_(value) {
    const values = {};
    values[OFFLINE_INFO_BANNER_COUNTER_KEY] = value;
    chrome.storage.local.set(values);
  }

  /**
   * chrome.storage.onChanged event handler.
   * @param {Object<Object>} changes Changes values.
   * @param {string} areaName "local" or "sync".
   * @private
   */
  onStorageChange_(changes, areaName) {
    if (areaName == 'local' &&
        HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY in changes) {
      this.holdingSpaceWelcomeBannerCounter_ =
          changes[HOLDING_SPACE_WELCOME_BANNER_COUNTER_KEY].newValue;
    }
    if (areaName == 'local' && WELCOME_HEADER_COUNTER_KEY in changes) {
      this.welcomeHeaderCounter_ = changes[WELCOME_HEADER_COUNTER_KEY].newValue;
    }
    if (areaName == 'local' && DRIVE_WARNING_DISMISSED_KEY in changes) {
      this.warningDismissedCounter_ =
          changes[DRIVE_WARNING_DISMISSED_KEY].newValue;
    }
    if (areaName == 'local' && DOWNLOADS_WARNING_DISMISSED_KEY in changes) {
      this.downloadsWarningDismissedTime_ =
          changes[DOWNLOADS_WARNING_DISMISSED_KEY].newValue;
    }
    if (areaName == 'local' && OFFLINE_INFO_BANNER_COUNTER_KEY in changes) {
      this.offlineInfoBannerCounter_ =
          changes[OFFLINE_INFO_BANNER_COUNTER_KEY].newValue;
    }
  }

  /**
   * Invoked when the drive connection status is change in the volume manager.
   * @private
   */
  onDriveConnectionChanged_() {
    this.maybeShowAuthFailBanner_();
  }

  /**
   * Shows the holding space welcome banner, creating the banner if necessary.
   * @private
   */
  prepareAndShowHoldingSpaceWelcomeBanner_() {
    assert(HoldingSpaceUtil.isFeatureEnabled());
    this.showHoldingSpaceWelcomeBanner_(true);

    // Do not recreate the banner.
    if (this.holdingSpaceWelcomeBanner_.firstElementChild) {
      return;
    }

    // Add banner styles to document head.
    if (!this.document_.head.querySelector(
            'link[holding-space-welcome-style]')) {
      const style = this.document_.createElement('link');
      style.rel = 'stylesheet';
      style.href = constants.HOLDING_SPACE_WELCOME_CSS;
      style.setAttribute('holding-space-welcome-style', '');
      this.document_.head.appendChild(style);
    }

    const wrapper = util.createChild(
        this.holdingSpaceWelcomeBanner_, 'holding-space-welcome-wrapper');
    util.createChild(wrapper, 'holding-space-welcome-icon');

    const message = util.createChild(wrapper, 'holding-space-welcome-message');
    util.setClampLine(message, '2');

    const title =
        util.createChild(message, 'holding-space-welcome-title headline2');
    title.textContent = str('HOLDING_SPACE_WELCOME_TITLE');

    // NOTE: Only one of either `text` or `textInTabletMode` will be displayed
    // at a time depending on whether or not tablet mode is enabled.
    const body = util.createChild(message, 'body2-primary');
    const text = util.createChild(body, 'holding-space-welcome-text');
    const textInTabletMode = util.createChild(
        body, 'holding-space-welcome-text tablet-mode-enabled');
    text.textContent = str('HOLDING_SPACE_WELCOME_TEXT');
    textInTabletMode.innerHTML = strf(
        'HOLDING_SPACE_WELCOME_TEXT_IN_TABLET_MODE',
        '<span class="icon">&nbsp;</span>');

    const buttonGroup = util.createChild(wrapper, 'button-group', 'div');
    const dismiss = util.createChild(buttonGroup, 'text-button', 'cr-button');
    dismiss.setAttribute('aria-label', str('HOLDING_SPACE_WELCOME_DISMISS'));
    dismiss.textContent = str('HOLDING_SPACE_WELCOME_DISMISS');
    dismiss.tabIndex = 0;

    dismiss.addEventListener(
        'click', this.closeHoldingSpaceWelcomeBanner_.bind(this));
  }

  /**
   * @param {string} type 'none'|'page'|'header'.
   * @param {string} messageId Resource ID of the message.
   * @private
   */
  prepareAndShowWelcomeBanner_(type, messageId) {
    if (!this.showWelcome_) {
      return;
    }

    this.showWelcomeBanner_(type);

    const container =
        queryRequiredElement('.drive-welcome.' + type, this.document_);
    if (container.firstElementChild) {
      return;
    }  // Do not re-create.

    if (!this.document_.querySelector('link[drive-welcome-style]')) {
      const style = this.document_.createElement('link');
      style.rel = 'stylesheet';
      style.href = constants.DRIVE_WELCOME_CSS;
      style.setAttribute('drive-welcome-style', '');
      this.document_.head.appendChild(style);
    }

    const wrapper = util.createChild(container, 'drive-welcome-wrapper');
    util.createChild(wrapper, 'drive-welcome-icon');

    let close, links;
    if (util.isFilesNg()) {
      const message = util.createChild(wrapper, 'drive-welcome-message');
      util.setClampLine(message, '2');

      const title = util.createChild(message, 'drive-welcome-title headline2');
      title.textContent = str('DRIVE_WELCOME_TITLE');

      const body = util.createChild(message, 'body2-primary');

      const text = util.createChild(body, 'drive-welcome-text');
      text.innerHTML = str(messageId);

      links = util.createChild(body, 'drive-welcome-links');

      // Hide link if it's trimmed by line-clamp so it does not get focus
      // and break ellipsis render.
      this.hideOverflowedElement(links, body);

      const buttonGroup = util.createChild(wrapper, 'button-group', 'div');

      close = util.createChild(
          buttonGroup, 'banner-close text-button', 'cr-button');
      close.innerHTML = str('DRIVE_WELCOME_DISMISS');
    } else {
      close = util.createChild(wrapper, 'banner-close', 'button');

      const message = util.createChild(wrapper, 'drive-welcome-message');

      const title = util.createChild(message, 'drive-welcome-title');
      title.textContent = str('DRIVE_WELCOME_TITLE');

      const text = util.createChild(message, 'drive-welcome-text');
      text.innerHTML = str(messageId);

      links = util.createChild(message, 'drive-welcome-links');
    }

    close.setAttribute('aria-label', str('DRIVE_WELCOME_DISMISS'));
    close.id = 'welcome-dismiss';
    close.tabIndex = 0;
    close.addEventListener('click', this.closeWelcomeBanner_.bind(this));

    const more = util.createChild(links, 'plain-link', 'a');
    more.textContent = str('DRIVE_LEARN_MORE');
    more.href = str('GOOGLE_DRIVE_OVERVIEW_URL');
    more.tabIndex = 0;
    more.id = 'drive-welcome-link';
    more.rel = 'opener';
    more.target = '_blank';

    this.previousDirWasOnDrive_ = false;
  }

  /**
   * Show or hide the "Low Google Drive space" warning.
   * @param {boolean} show True if the box need to be shown.
   * @param {Object=} opt_sizeStats Size statistics. Should be defined when
   *     showing the warning.
   * @private
   */
  showLowDriveSpaceWarning_(show, opt_sizeStats) {
    const box = this.document_.querySelector('#volume-space-warning');

    // Avoid showing two banners.
    // TODO(kaznacheev): Unify the low space warning and the promo header.
    if (show) {
      this.cleanupWelcomeBanner_();
    }

    if (box.hidden == !show) {
      return;
    }

    if (this.warningDismissedCounter_) {
      if (opt_sizeStats &&
          // Quota had not changed
          this.warningDismissedCounter_ == opt_sizeStats.totalSize &&
          opt_sizeStats.remainingSize / opt_sizeStats.totalSize < 0.15) {
        // Since the last dismissal decision the quota has not changed AND
        // the user did not free up significant space. Obey the dismissal.
        show = false;
      } else {
        // Forget the dismissal. Warning will be shown again.
        this.setWarningDismissedCounter_(0);
      }
    }

    box.textContent = '';
    if (show && opt_sizeStats) {
      if (util.isFilesNg()) {
        const icon = this.document_.createElement('div');
        icon.className = 'drive-icon';
        box.appendChild(icon);

        const text = this.document_.createElement('div');
        text.className = 'body2-primary';
        text.textContent = strf(
            'DRIVE_SPACE_AVAILABLE_LONG',
            util.bytesToString(opt_sizeStats.remainingSize));
        util.setClampLine(text, '2');
        box.appendChild(text);

        const buttonGroup = this.document_.createElement('div');
        buttonGroup.className = 'button-group';
        box.appendChild(buttonGroup);

        const close = this.document_.createElement('cr-button');
        close.setAttribute('aria-label', str('DRIVE_WELCOME_DISMISS'));
        close.id = 'drive-space-warning-dismiss';
        close.innerHTML = str('DRIVE_WELCOME_DISMISS');
        close.className = 'banner-close text-button';
        buttonGroup.appendChild(close);

        const link = this.document_.createElement('a');
        link.href = str('GOOGLE_DRIVE_BUY_STORAGE_URL');
        link.rel = 'opener';
        link.target = '_blank';
        const buyMore = this.document_.createElement('cr-button');
        buyMore.className = 'banner-button text-button';
        buyMore.textContent = str('DRIVE_BUY_MORE_SPACE_LINK');
        link.appendChild(buyMore);
        buttonGroup.appendChild(link);

        const totalSize = opt_sizeStats.totalSize;
        close.addEventListener('click', () => {
          const values = {};
          values[DRIVE_WARNING_DISMISSED_KEY] = totalSize;
          chrome.storage.local.set(values);
          box.hidden = true;
          this.requestRelayout_(100);
        });
      } else {
        const icon = this.document_.createElement('div');
        icon.className = 'drive-icon';
        box.appendChild(icon);

        const text = this.document_.createElement('div');
        text.className = 'drive-text';
        text.textContent = strf(
            'DRIVE_SPACE_AVAILABLE_LONG',
            util.bytesToString(opt_sizeStats.remainingSize));
        box.appendChild(text);

        const link = this.document_.createElement('a');
        link.href = str('GOOGLE_DRIVE_BUY_STORAGE_URL');
        link.rel = 'opener';
        link.target = '_blank';
        const button = this.document_.createElement('button');
        button.className = 'imitate-paper-button';
        button.textContent = str('DRIVE_BUY_MORE_SPACE_LINK');
        link.appendChild(button);
        box.appendChild(link);

        const close = this.document_.createElement('button');
        close.setAttribute('aria-label', str('DRIVE_WELCOME_DISMISS'));
        close.id = 'drive-space-warning-dismiss';
        close.className = 'banner-close';
        box.appendChild(close);
        const totalSize = opt_sizeStats.totalSize;
        close.addEventListener('click', () => {
          const values = {};
          values[DRIVE_WARNING_DISMISSED_KEY] = totalSize;
          chrome.storage.local.set(values);
          box.hidden = true;
          this.requestRelayout_(100);
        });
      }
    }

    if (box.hidden != !show) {
      box.hidden = !show;
      this.requestRelayout_(100);
    }
  }

  /**
   * Closes the holding space welcome banner.
   * @private
   */
  closeHoldingSpaceWelcomeBanner_() {
    assert(HoldingSpaceUtil.isFeatureEnabled());
    this.cleanupHoldingSpaceWelcomeBanner_();

    // Stop showing the welcome banner.
    this.setHoldingSpaceWelcomeBannerCounter_(
        HOLDING_SPACE_WELCOME_BANNER_COUNTER_LIMIT);
  }

  /**
   * Closes the Drive Welcome banner.
   * @private
   */
  closeWelcomeBanner_() {
    this.cleanupWelcomeBanner_();
    // Stop showing the welcome banner.
    this.setWelcomeHeaderCounter_(WELCOME_HEADER_COUNTER_LIMIT);
  }

  /**
   * Shows or hides the welcome banner for drive.
   * @private
   */
  maybeShowDriveBanners_() {
    this.ready_.then(() => {
      if (!this.isOnCurrentProfileDrive()) {
        // We are not on the drive file system. Do not show (close) the drive
        // banners.
        this.cleanupWelcomeBanner_();
        this.previousDirWasOnDrive_ = false;
        this.offlineInfoBanner_.hidden = true;
        return;
      }

      const driveVolume = this.volumeManager_.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DRIVE);
      if (!driveVolume || driveVolume.error) {
        // Drive is not mounted, so do nothing.
        return;
      }

      if (this.welcomeHeaderCounter_ < WELCOME_HEADER_COUNTER_LIMIT) {
        this.maybeShowWelcomeBanner_();
      }

      if (util.isFilesNg() && util.isDriveDssPinEnabled() &&
          (this.offlineInfoBannerCounter_ < OFFLINE_INFO_BANNER_COUNTER_LIMIT ||
           this.hasShownOfflineInfoBanner_)) {
        this.offlineInfoBanner_.hidden = false;
        if (!this.hasShownOfflineInfoBanner_) {
          this.hasShownOfflineInfoBanner_ = true;
          this.setOfflineInfoBannerCounter_(this.offlineInfoBannerCounter_ + 1);
        }
      }
    });
  }

  /**
   * Shows or hides the welcome banner for holding space.
   * @return {Promise<void>}
   * @private
   */
  async maybeShowHoldingSpaceWelcomeBanner_() {
    if (!HoldingSpaceUtil.isFeatureEnabled()) {
      return;
    }

    await this.ready_;

    // The holding space feature is only allowed for specific volume types so
    // its banner should only be shown for those volumes. Note that the holding
    // space banner is explicitly disallowed from showing in `DRIVE` to prevent
    // the possibility of it being shown alongside the Drive banner.
    const allowedVolumeTypes = HoldingSpaceUtil.getAllowedVolumeTypes();
    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (!currentVolumeInfo ||
        !allowedVolumeTypes.includes(currentVolumeInfo.volumeType) ||
        currentVolumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      return;
    }

    // The holding space banner should not be shown after having been shown
    // enough times to reach the defined limit. Note that if the user explicitly
    // dismisses the banner the counter will be set to the limit to prevent any
    // additional showings.
    if (this.holdingSpaceWelcomeBannerCounter_ >=
        HOLDING_SPACE_WELCOME_BANNER_COUNTER_LIMIT) {
      return;
    }

    // If the holding space banner is already showing, don't increment the count
    // of how many times it has been shown since this is likely only occurring
    // due to directory change or some other event in which the banner never
    // disappeared from the user's view.
    if (!this.holdingSpaceWelcomeBanner_.hasAttribute('hidden')) {
      return;
    }

    this.setHoldingSpaceWelcomeBannerCounter_(
        this.holdingSpaceWelcomeBannerCounter_ + 1);
    this.prepareAndShowHoldingSpaceWelcomeBanner_();
  }

  /**
   * Decides which banner should be shown, and show it. This method is designed
   * to be called only from maybeShowDriveBanners_.
   * @private
   */
  maybeShowWelcomeBanner_() {
    this.ready_.then(() => {
      if (util.isFilesNg()) {
        // We do not want to increment the counter when the user navigates
        // between different directories on Drive, but we increment the counter
        // once anyway to prevent the full page banner from showing.
        if (!this.previousDirWasOnDrive_ || this.welcomeHeaderCounter_ == 0) {
          this.setWelcomeHeaderCounter_(this.welcomeHeaderCounter_ + 1);
          this.prepareAndShowWelcomeBanner_(
              'header', 'DRIVE_WELCOME_TEXT_SHORT_FILESNG');
        }
      } else if (
          this.directoryModel_.getFileList().length == 0 &&
          this.welcomeHeaderCounter_ == 0) {
        // Only show the full page banner if the header banner was never shown.
        // Do not increment the counter.
        // The timeout below is required because sometimes another
        // 'rescan-completed' event arrives shortly with non-empty file list.
        setTimeout(() => {
          if (this.isOnCurrentProfileDrive() &&
              this.welcomeHeaderCounter_ == 0) {
            this.prepareAndShowWelcomeBanner_(
                'page', 'DRIVE_WELCOME_TEXT_LONG');
          }
        }, 2000);
      } else {
        // We do not want to increment the counter when the user navigates
        // between different directories on Drive, but we increment the counter
        // once anyway to prevent the full page banner from showing.
        if (!this.previousDirWasOnDrive_ || this.welcomeHeaderCounter_ == 0) {
          this.setWelcomeHeaderCounter_(this.welcomeHeaderCounter_ + 1);
          this.prepareAndShowWelcomeBanner_(
              'header', 'DRIVE_WELCOME_TEXT_SHORT');
        }
      }
      this.previousDirWasOnDrive_ = true;
    });
  }

  /**
   * @return {boolean} True if current directory is on Drive root of current
   * profile.
   */
  isOnCurrentProfileDrive() {
    const entry = this.directoryModel_.getCurrentDirEntry();
    if (!entry || util.isFakeEntry(entry)) {
      return false;
    }
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (!locationInfo) {
      return false;
    }
    return locationInfo.rootType === VolumeManagerCommon.RootType.DRIVE &&
        locationInfo.volumeInfo.profile.isCurrentProfile;
  }

  /**
   * Shows (or hides) the holding space welcome banner.
   * @param {boolean} show
   * @private
   */
  showHoldingSpaceWelcomeBanner_(show) {
    assert(HoldingSpaceUtil.isFeatureEnabled());

    const /** boolean */ hidden = !show;
    if (this.holdingSpaceWelcomeBanner_.hasAttribute('hidden') == hidden) {
      return;
    }

    if (hidden) {
      this.holdingSpaceWelcomeBanner_.setAttribute('hidden', '');
    } else {
      this.holdingSpaceWelcomeBanner_.removeAttribute('hidden');
      HoldingSpaceUtil.maybeStoreTimeOfFirstWelcomeBannerShow();
    }

    this.requestRelayout_(200);  // Resize only after the animation is done.
  }

  /**
   * Shows the Drive Welcome banner.
   * @param {string} type 'page'|'head'|'none'.
   * @private
   */
  showWelcomeBanner_(type) {
    const container = this.document_.querySelector('.dialog-container');
    if (container.getAttribute('drive-welcome') != type) {
      container.setAttribute('drive-welcome', type);
      this.requestRelayout_(200);  // Resize only after the animation is done.
    }
  }

  /**
   * Update the UI when the current directory changes.
   *
   * @param {Event} event The directory-changed event.
   * @private
   */
  onDirectoryChanged_(event) {
    const rootVolume = this.volumeManager_.getVolumeInfo(event.newDirEntry);
    if (!rootVolume) {
      return;
    }
    const previousRootVolume = event.previousDirEntry ?
        this.volumeManager_.getVolumeInfo(event.previousDirEntry) :
        null;

    // Show (or hide) the low space warning.
    this.maybeShowLowSpaceWarning_(rootVolume);

    // Add or remove listener to show low space warning, if necessary.
    const isLowSpaceWarningTarget = this.isLowSpaceWarningTarget_(rootVolume);
    if (isLowSpaceWarningTarget !==
        this.isLowSpaceWarningTarget_(previousRootVolume)) {
      if (isLowSpaceWarningTarget) {
        chrome.fileManagerPrivate.onDirectoryChanged.addListener(
            this.privateOnDirectoryChangedBound_);
      } else {
        chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
            this.privateOnDirectoryChangedBound_);
      }
    }

    if (!this.isOnCurrentProfileDrive()) {
      this.cleanupWelcomeBanner_();
      this.authFailedBanner_.hidden = true;
    }

    this.updateDriveUnmountedPanel_();
    if (this.isOnCurrentProfileDrive()) {
      this.unmountedPanel_.classList.remove('retry-enabled');
      this.maybeShowAuthFailBanner_();
    }
  }

  /**
   * @param {VolumeInfo} volumeInfo Volume info to be checked.
   * @return {boolean} true if the file system specified by |root| is a target
   *     to show low space warning. Otherwise false.
   * @private
   */
  isLowSpaceWarningTarget_(volumeInfo) {
    if (!volumeInfo) {
      return false;
    }
    return volumeInfo.profile.isCurrentProfile &&
        (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS ||
         volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE);
  }

  /**
   * Callback which is invoked when the file system has been changed.
   * @param {Object} event chrome.fileManagerPrivate.onDirectoryChanged event.
   * @private
   */
  privateOnDirectoryChanged_(event) {
    const currentDirEntry = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirEntry) {
      return;
    }
    const currentVolume = this.volumeManager_.getVolumeInfo(currentDirEntry);
    if (!currentVolume) {
      return;
    }
    const eventVolume = this.volumeManager_.getVolumeInfo(event.entry);
    if (currentVolume === eventVolume) {
      // The file system we are currently on is changed.
      // So, check the free space.
      this.maybeShowLowSpaceWarning_(currentVolume);
    }
  }

  /**
   * Shows or hides the low space warning.
   * @param {VolumeInfo} volume Type of volume, which we are interested in.
   * @private
   */
  maybeShowLowSpaceWarning_(volume) {
    // Never show low space warning banners in a test as it will cause flakes.
    // TODO(crbug.com/1146265): Somehow figure out a way to test these banners.
    if (window.IN_TEST) {
      return;
    }

    // TODO(kaznacheev): Unify the two low space warning.
    switch (volume.volumeType) {
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
        this.showLowDriveSpaceWarning_(false);
        break;
      case VolumeManagerCommon.VolumeType.DRIVE:
        this.showLowDownloadsSpaceWarning_(false);
        break;
      default:
        // If the current file system is neither the DOWNLOAD nor the DRIVE,
        // just hide the warning.
        this.showLowDownloadsSpaceWarning_(false);
        this.showLowDriveSpaceWarning_(false);
        return;
    }

    // If not mounted correctly, then do not continue.
    if (!volume.fileSystem) {
      return;
    }

    chrome.fileManagerPrivate.getSizeStats(volume.volumeId, sizeStats => {
      const currentVolume = this.volumeManager_.getVolumeInfo(
          assert(this.directoryModel_.getCurrentDirEntry()));
      if (volume !== currentVolume) {
        // This happens when the current directory is moved during requesting
        // the file system size. Just ignore it.
        return;
      }
      // sizeStats is undefined, if some error occurs.
      if (!sizeStats || sizeStats.totalSize == 0) {
        return;
      }

      if (volume.volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS) {
        // Show the warning banner when the available space is less than 1GB.
        this.showLowDownloadsSpaceWarning_(
            sizeStats.remainingSize < DOWNLOADS_SPACE_WARNING_THRESHOLD_SIZE);
      } else {
        // Show the warning banner when the available space ls less than 10%.
        const remainingRatio = sizeStats.remainingSize / sizeStats.totalSize;
        this.showLowDriveSpaceWarning_(
            remainingRatio < DRIVE_SPACE_WARNING_THRESHOLD_RATIO, sizeStats);
      }
    });
  }

  /**
   * Removes the holding space welcome banner.
   * @private
   */
  cleanupHoldingSpaceWelcomeBanner_() {
    assert(HoldingSpaceUtil.isFeatureEnabled());
    this.showHoldingSpaceWelcomeBanner_(false);
  }

  /**
   * removes the Drive Welcome banner.
   * @private
   */
  cleanupWelcomeBanner_() {
    this.showWelcomeBanner_('none');
  }

  /**
   * Notifies the file manager what layout must be recalculated.
   * @param {number} delay In milliseconds.
   * @private
   */
  requestRelayout_(delay) {
    const self = this;
    setTimeout(() => {
      cr.dispatchSimpleEvent(self, 'relayout');
    }, delay);
  }

  /**
   * Show or hide the "Low disk space" warning.
   * @param {boolean} show True if the box need to be shown.
   * @private
   */
  showLowDownloadsSpaceWarning_(show) {
    const box = this.document_.querySelector('.downloads-warning');

    if (box.hidden == !show) {
      return;
    }

    if (this.downloadsWarningDismissedTime_) {
      if (Date.now() - this.downloadsWarningDismissedTime_ <
          DOWNLOADS_SPACE_WARNING_DISMISS_DURATION) {
        show = false;
      }
    }

    box.textContent = '';
    if (show) {
      const icon = this.document_.createElement('div');
      icon.className = 'warning-icon';
      const message = this.document_.createElement('div');
      message.className = 'warning-message';
      if (util.isFilesNg()) {
        message.className += ' body2-primary';
        message.innerHTML =
            util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING_FILESNG'));
        util.setClampLine(message, '2');

        // Wrap a div around link.
        const link = message.querySelector('a');
        const linkWrapper = this.document_.createElement('div');
        linkWrapper.className = 'link-wrapper';
        message.appendChild(linkWrapper);
        linkWrapper.appendChild(link);

        // Hide the link if it's trimmed by line-clamp so it does not get focus
        // and break ellipsis render.
        this.hideOverflowedElement(linkWrapper, message);
      } else {
        message.innerHTML =
            util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING'));
      }
      box.appendChild(icon);
      box.appendChild(message);
      box.querySelector('a').addEventListener('click', e => {
        util.visitURL(str('DOWNLOADS_LOW_SPACE_WARNING_HELP_URL'));
        e.preventDefault();
      });

      const buttonGroup = this.document_.createElement('div');
      buttonGroup.className = 'button-group';
      box.appendChild(buttonGroup);

      const closeType = util.isFilesNg() ? 'cr-button' : 'button';
      const close = this.document_.createElement(closeType);
      close.className = 'banner-close';
      close.setAttribute('aria-label', str('DRIVE_WELCOME_DISMISS'));
      close.id = 'downloads-space-warning-dismiss';
      if (util.isFilesNg()) {
        close.innerHTML = str('DRIVE_WELCOME_DISMISS');
        close.className = 'banner-close text-button';
      }
      buttonGroup.appendChild(close);
      close.addEventListener('click', () => {
        const values = {};
        values[DOWNLOADS_WARNING_DISMISSED_KEY] = Date.now();
        chrome.storage.local.set(values);
        box.hidden = true;
        // We explicitly mark the banner-close element as hidden as due to the
        // use of position absolute in it's layout it does not get hidden by
        // hiding it's parent.
        close.hidden = true;
        this.requestRelayout_(100);
      });
    }

    box.hidden = !show;
    this.requestRelayout_(100);
  }

  /**
   * Creates contents for the DRIVE unmounted panel.
   * @private
   */
  ensureDriveUnmountedPanelInitialized_() {
    const panel = this.unmountedPanel_;
    if (panel.firstElementChild) {
      return;
    }

    /**
     * Creates an element using given parameters.
     * @param {!Element} parent Parent element of the new element.
     * @param {string} tag Tag of the new element.
     * @param {string} className Class name of the new element.
     * @param {string=} opt_textContent Text content of the new element.
     * @return {!Element} The newly created element.
     */
    const create = (parent, tag, className, opt_textContent) => {
      const div = panel.ownerDocument.createElement(tag);
      div.className = className;
      div.textContent = opt_textContent || '';
      parent.appendChild(div);
      return div;
    };

    create(panel, 'div', 'error', str('DRIVE_CANNOT_REACH'));

    const learnMore =
        create(panel, 'a', 'learn-more plain-link', str('DRIVE_LEARN_MORE'));
    learnMore.href = str('GOOGLE_DRIVE_ERROR_HELP_URL');
    learnMore.rel = 'opener';
    learnMore.target = '_blank';
  }

  /**
   * Called when volume info list is updated.
   * @param {Event} event Splice event data on volume info list.
   * @private
   */
  onVolumeInfoListSplice_(event) {
    const isDriveVolume = volumeInfo => {
      return volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
    };
    if (event.removed.some(isDriveVolume) || event.added.some(isDriveVolume)) {
      this.updateDriveUnmountedPanel_();
    }
  }

  /**
   * Shows the panel when current directory is DRIVE and it's unmounted.
   * Hides it otherwise. The panel shows an error message if it failed.
   * @private
   */
  updateDriveUnmountedPanel_() {
    const node = this.document_.body;
    if (this.isOnCurrentProfileDrive()) {
      const driveVolume = this.volumeManager_.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DRIVE);
      if (driveVolume) {
        if (driveVolume.error) {
          this.ensureDriveUnmountedPanelInitialized_();
          this.unmountedPanel_.classList.add('retry-enabled');
          node.setAttribute('drive', 'error');
        } else {
          node.setAttribute('drive', 'mounted');
        }
      } else {
        this.unmountedPanel_.classList.remove('retry-enabled');
        node.setAttribute('drive', 'unmounted');
      }
    } else {
      node.removeAttribute('drive');
    }
  }

  /**
   * Updates the visibility of Drive Connection Warning banner, retrieving the
   * current connection information.
   * @private
   */
  maybeShowAuthFailBanner_() {
    const connection = this.volumeManager_.getDriveConnectionState();
    const showDriveNotReachedMessage = this.isOnCurrentProfileDrive() &&
        connection.type ==
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
        connection.reason ==
            chrome.fileManagerPrivate.DriveOfflineReason.NOT_READY;
    this.authFailedBanner_.hidden = !showDriveNotReachedMessage;
  }

  /**
   * Hides element if it has overflowed its container after resizing.
   *
   * @param {!Element} element The element to hide.
   * @param {!Element} container The container to observe overflow.
   */
  hideOverflowedElement(element, container) {
    const observer = new ResizeObserver(() => {
      if (util.hasOverflow(container)) {
        element.style.visibility = 'hidden';
      } else {
        element.style.visibility = 'visible';
      }
    });
    observer.observe(container);
  }
}
