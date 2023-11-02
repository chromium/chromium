// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {HoldingSpaceUtil} from '../../holding_space_util.js';
import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'holding-space-welcome-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows when a user navigates to a volume that allows pinning
 * of files to the shelf. Highlights to the user how to use the Holding space
 * feature.
 */
export class HoldingSpaceWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Holding Space Welcome educational banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Returns the list of allow volume types remapping over the canonical source
   * at HoldingSpaceUtil.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return HoldingSpaceUtil.getAllowedVolumeTypes().map(type => {
      if (type === VolumeManagerCommon.VolumeType.DRIVE) {
        return {
          type: VolumeManagerCommon.VolumeType.DRIVE,
          root: VolumeManagerCommon.RootType.DRIVE,
        };
      }
      return {type};
    });
  }

  /**
   * Store the time the banner was first shown.
   */
  onShow() {
    HoldingSpaceUtil.maybeStoreTimeOfFirstWelcomeBannerShow();
  }
}

customElements.define(TAG_NAME, HoldingSpaceWelcomeBanner);
