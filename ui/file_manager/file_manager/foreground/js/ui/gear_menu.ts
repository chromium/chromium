// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';

import {queryRequiredElement} from '../../../common/js/dom_utils.js';
import {bytesToString, str, strf} from '../../../common/js/translations.js';

import type {MenuItem} from './menu_item.js';

/**
 * Selector used by tast tests to identify when the storage meter is empty.
 */
const tastEmptySpaceId = 'tast-storage-meter-empty';

export interface SpaceInfo {
  totalSize: number;
  usedSize: number;
  warningMessage: string;
}

export class GearMenu {
  readonly syncButton: MenuItem;
  readonly volumeSpaceInfo: HTMLElement;
  private readonly volumeSpaceInfoLabel_: HTMLElement;
  private readonly volumeSpaceInnerBar_: HTMLElement;
  private readonly volumeSpaceOuterBar_: HTMLElement;
  private readonly volumeSpaceWarning_: HTMLElement;
  private readonly providersMenuItem_: HTMLElement;
  /**
   * Promise to be resolved with volume space info.
   */
  private spaceInfoPromise_: Promise<SpaceInfo|undefined>|null = null;

  constructor(element: HTMLElement) {
    this.syncButton =
        queryRequiredElement('#gear-menu-drive-sync-settings', element) as
        MenuItem;
    this.volumeSpaceInfo = queryRequiredElement('#volume-space-info', element);
    this.volumeSpaceInfoLabel_ =
        queryRequiredElement('#volume-space-info-label', element);
    this.volumeSpaceInnerBar_ =
        queryRequiredElement('#volume-space-info-bar', element);

    assertInstanceof(this.volumeSpaceInnerBar_.parentElement, HTMLElement);
    this.volumeSpaceOuterBar_ = this.volumeSpaceInnerBar_.parentElement;

    this.volumeSpaceWarning_ =
        queryRequiredElement('#volume-space-info-warning', element);
    this.providersMenuItem_ =
        queryRequiredElement('#gear-menu-providers', element);

    // Initialize attributes.
    this.syncButton.checkable = true;
  }

  /**
   * @param shouldHide Whether the providers gear menu item should be hidden or
   *     not.
   */
  updateShowProviders(shouldHide: boolean) {
    this.providersMenuItem_.hidden = shouldHide;
  }

  /**
   * @param spaceInfoPromise Promise to be fulfilled with space info.
   * @param showLoadingCaption Whether to show the loading caption or not.
   */
  async setSpaceInfo(
      spaceInfoPromise: Promise<SpaceInfo|undefined>|null,
      showLoadingCaption: boolean) {
    this.spaceInfoPromise_ = spaceInfoPromise;

    if (!spaceInfoPromise || loadTimeData.getBoolean('HIDE_SPACE_INFO')) {
      this.hideVolumeSpaceInfo();
      return;
    }

    this.showVolumeSpaceInfo();
    this.volumeSpaceInnerBar_.setAttribute('pending', '');
    if (showLoadingCaption) {
      this.volumeSpaceInfoLabel_.innerText = str('WAITING_FOR_SPACE_INFO');
      this.volumeSpaceInnerBar_.style.width = '100%';
    }

    this.volumeSpaceInfo.classList.remove(tastEmptySpaceId);

    let spaceInfo;
    try {
      spaceInfo = await spaceInfoPromise;
    } catch (error: unknown) {
      console.warn('Failed get space info', error);
      return;
    }

    if (this.spaceInfoPromise_ !== spaceInfoPromise) {
      return;
    }

    this.volumeSpaceInnerBar_.removeAttribute('pending');
    this.volumeSpaceOuterBar_.hidden = true;
    this.volumeSpaceWarning_.hidden = true;

    if (!spaceInfo) {
      this.volumeSpaceInfoLabel_.textContent = str('FAILED_SPACE_INFO');
      return;
    }

    const remainingSize = spaceInfo.totalSize - spaceInfo.usedSize;

    if (spaceInfo.totalSize >= 0) {
      if (remainingSize <= 0) {
        this.volumeSpaceInfo.classList.add(tastEmptySpaceId);
      }

      this.volumeSpaceInnerBar_.style.width =
          Math.min(100, 100 * spaceInfo.usedSize / spaceInfo.totalSize) + '%';

      this.volumeSpaceOuterBar_.hidden = false;

      this.volumeSpaceInfoLabel_.textContent =
          strf('SPACE_AVAILABLE', bytesToString(Math.max(0, remainingSize)));
    } else {
      // User has unlimited individual storage.
      this.volumeSpaceInfoLabel_.textContent =
          strf('SPACE_USED', bytesToString(spaceInfo.usedSize));
    }

    if (spaceInfo.warningMessage) {
      this.volumeSpaceWarning_.hidden = false;
      this.volumeSpaceWarning_.textContent = '*' + spaceInfo.warningMessage;
    }
  }

  private hideVolumeSpaceInfo() {
    this.volumeSpaceInfo.hidden = true;
  }

  private showVolumeSpaceInfo() {
    this.volumeSpaceInfo.hidden = false;
  }
}
