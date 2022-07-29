// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {str, strf, util} from '../../../common/js/util.js';

/**
 * @typedef {{totalSize: number, usedSize: number, warningMessage: string}}
 */
export let SpaceInfo;

export class GearMenu {
  /**
   * @param {!HTMLElement} element
   */
  constructor(element) {
    /**
     * @type {!HTMLMenuItemElement}
     * @const
     */
    this.syncButton = /** @type {!HTMLMenuItemElement} */
        (util.queryRequiredElement('#gear-menu-drive-sync-settings', element));

    /**
     * @type {!HTMLElement}
     * @const
     */
    this.volumeSpaceInfo =
        util.queryRequiredElement('#volume-space-info', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoSeparator_ =
        util.queryRequiredElement('#volume-space-info-separator', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoLabel_ =
        util.queryRequiredElement('#volume-space-info-label', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInnerBar_ =
        util.queryRequiredElement('#volume-space-info-bar', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceOuterBar_ =
        assertInstanceof(this.volumeSpaceInnerBar_.parentElement, HTMLElement);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceWarning_ =
        util.queryRequiredElement('#volume-space-info-warning', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.providersMenuItem_ =
        util.queryRequiredElement('#gear-menu-providers', element);

    /**
     * Promise to be resolved with volume space info.
     * @type {Promise<SpaceInfo|undefined>}
     * @private
     */
    this.spaceInfoPromise_ = null;

    // Initialize attributes.
    this.syncButton.checkable = true;
  }

  /**
   * @param {!boolean} shouldHide Whether the providers gear menu item should be
   *     hidden or not.
   */
  updateShowProviders(shouldHide) {
    this.providersMenuItem_.hidden = shouldHide;
  }

  /**
   * @param {Promise<SpaceInfo|undefined>} spaceInfoPromise Promise to be
   *     fulfilled with space info.
   * @param {boolean} showLoadingCaption Whether to show the loading caption or
   *     not.
   */
  setSpaceInfo(spaceInfoPromise, showLoadingCaption) {
    this.spaceInfoPromise_ = spaceInfoPromise;

    if (!spaceInfoPromise || loadTimeData.getBoolean('HIDE_SPACE_INFO')) {
      this.hideVolumeSpaceInfo_();
      return;
    }

    this.showVolumeSpaceInfo_();
    this.volumeSpaceInnerBar_.setAttribute('pending', '');
    if (showLoadingCaption) {
      this.volumeSpaceInfoLabel_.innerText = str('WAITING_FOR_SPACE_INFO');
      this.volumeSpaceInnerBar_.style.width = '100%';
    }

    spaceInfoPromise.then(
        spaceInfo => {
          if (this.spaceInfoPromise_ != spaceInfoPromise) {
            return;
          }

          this.volumeSpaceInnerBar_.removeAttribute('pending');
          this.volumeSpaceOuterBar_.hidden = true;
          this.volumeSpaceWarning_.hidden = true;

          if (!spaceInfo) {
            this.volumeSpaceInfoLabel_.textContent = str('FAILED_SPACE_INFO');
            return;
          }

          if (spaceInfo.totalSize >= 0) {
            const usedSpace = spaceInfo.usedSize;
            this.volumeSpaceInnerBar_.style.width =
                Math.min(100, 100 * usedSpace / spaceInfo.totalSize) + '%';

            this.volumeSpaceOuterBar_.hidden = false;

            this.volumeSpaceInfoLabel_.textContent = strf(
                'SPACE_AVAILABLE',
                util.bytesToString(
                    Math.max(0, spaceInfo.totalSize - spaceInfo.usedSize)));
          } else {
            // User has unlimited individual storage.
            this.volumeSpaceInfoLabel_.textContent =
                strf('SPACE_USED', util.bytesToString(spaceInfo.usedSize));
          }

          if (spaceInfo.warningMessage) {
            this.volumeSpaceWarning_.hidden = false;
            this.volumeSpaceWarning_.textContent =
                '*' + spaceInfo.warningMessage;
          }
        },
        error => {
          console.warn('Failed get space info', error);
        });
  }

  hideVolumeSpaceInfo_() {
    this.volumeSpaceInfo.hidden = true;
    this.volumeSpaceInfoSeparator_.hidden = true;
  }

  showVolumeSpaceInfo_() {
    this.volumeSpaceInfo.hidden = false;
    this.volumeSpaceInfoSeparator_.hidden = false;
  }
}
