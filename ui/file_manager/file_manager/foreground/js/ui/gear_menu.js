// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';

import {str, strf, util} from '../../../common/js/util.js';

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
        (queryRequiredElement('#gear-menu-drive-sync-settings', element));

    /**
     * @type {!HTMLElement}
     * @const
     */
    this.volumeSpaceInfo = queryRequiredElement('#volume-space-info', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoSeparator_ =
        queryRequiredElement('#volume-space-info-separator', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoLabel_ =
        queryRequiredElement('#volume-space-info-label', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInnerBar_ =
        queryRequiredElement('#volume-space-info-bar', element);

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
    this.providersMenuItem_ =
        queryRequiredElement('#gear-menu-providers', element);

    /**
     * Volume space info.
     * @type {Promise<chrome.fileManagerPrivate.MountPointSizeStats|undefined>}
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
   * @param {Promise<chrome.fileManagerPrivate.MountPointSizeStats|undefined>}
   * spaceInfoPromise Promise to be fulfilled with space info.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   */
  setSpaceInfo(spaceInfoPromise, showLoadingCaption) {
    this.spaceInfoPromise_ = spaceInfoPromise;

    if (!spaceInfoPromise || loadTimeData.getBoolean('HIDE_SPACE_INFO')) {
      this.volumeSpaceInfo.hidden = true;
      this.volumeSpaceInfoSeparator_.hidden = true;
      return;
    }

    this.volumeSpaceInfo.hidden = false;
    this.volumeSpaceInfoSeparator_.hidden = false;
    this.volumeSpaceInnerBar_.setAttribute('pending', '');
    if (showLoadingCaption) {
      this.volumeSpaceInfoLabel_.innerText = str('WAITING_FOR_SPACE_INFO');
      this.volumeSpaceInnerBar_.style.width = '100%';
    }

    spaceInfoPromise.then(spaceInfo => {
      if (this.spaceInfoPromise_ != spaceInfoPromise) {
        return;
      }
      this.volumeSpaceInnerBar_.removeAttribute('pending');
      this.volumeSpaceOuterBar_.hidden = true;
      if (spaceInfo) {
        const sizeStr = util.bytesToString(spaceInfo.remainingSize);
        this.volumeSpaceInfoLabel_.textContent =
            strf('SPACE_AVAILABLE', sizeStr);

        if (spaceInfo.totalSize > 0) {
          const usedSpace = spaceInfo.totalSize - spaceInfo.remainingSize;
          this.volumeSpaceInnerBar_.style.width =
              (100 * usedSpace / spaceInfo.totalSize) + '%';

          this.volumeSpaceOuterBar_.hidden = false;
        }
      } else {
        this.volumeSpaceInfoLabel_.textContent = str('FAILED_SPACE_INFO');
      }
    });
  }
}
