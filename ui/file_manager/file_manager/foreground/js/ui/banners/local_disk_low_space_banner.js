// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'local-disk-low-space-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows a warning when the remaining space on a local disk goes
 * below 1GB. This is only shown if the user has navigated to the My files /
 * Downloads directories (and any children).
 */
export class LocalDiskLowSpaceBanner extends WarningBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Show the banner when the Downloads volume (local disk) is less than or
   * equal to 1 GB of remaining space.
   * @returns {!Banner.DiskThresholdMinSize}
   */
  diskThreshold() {
    return {
      type: VolumeManagerCommon.VolumeType.DOWNLOADS,
      minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
    };
  }

  /**
   * Only show the banner when the user has navigated to the Downloads volume
   * type (this includes the My files directory).
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{type: VolumeManagerCommon.VolumeType.DOWNLOADS}];
  }
}

customElements.define(TAG_NAME, LocalDiskLowSpaceBanner);
