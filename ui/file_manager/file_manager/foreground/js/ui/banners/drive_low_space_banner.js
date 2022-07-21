// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {strf, util} from '../../../../common/js/util.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'drive-low-space-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows a warning when the remaining space on a Google Drive goes
 * below 10%. This is only shown if the user has navigated to the My drive under
 * the Google Drive root excluding other directories such as Computers or
 * Shared drives.
 */
export class DriveLowSpaceBanner extends WarningBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Show the banner when the Drive volume has gone below 10% remaining space.
   * @returns {!Banner.DiskThresholdMinRatio}
   */
  diskThreshold() {
    return {
      type: VolumeManagerCommon.VolumeType.DRIVE,
      minRatio: 0.1,
    };
  }

  /**
   * Only show the banner when the user has navigated to the My drive directory
   * and all children. Having root and type means this warning does not show on
   * Shared drives, Team drives, Computers or Offline.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DRIVE,
      root: VolumeManagerCommon.RootType.DRIVE,
    }];
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. The remainingSize key contains the available space
   * left for this user to display via the banner.
   * @param {!Object} context chrome.fileManagerPrivate.MountPointSizeStats
   */
  onFilteredContext(context) {
    if (!context || context.remainingSize == null ||
        context.totalSize == null) {
      console.warn('Context not supplied or missing remainingSize');
      return;
    }
    const text = this.shadowRoot.querySelector('span[slot="text"]');
    text.innerText = strf(
        'DRIVE_INDIVIDUAL_QUOTA_LOW',
        Math.ceil(context.remainingSize / context.totalSize * 100),
        util.bytesToString(context.totalSize));
  }
}

customElements.define(TAG_NAME, DriveLowSpaceBanner);
