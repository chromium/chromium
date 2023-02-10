// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';

import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'drive-bulk-pinning-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that prompts users to bulk pin their files.
 */
export class DriveBulkPinningBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Drive Bulk Pinning educational banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type
   * and the feature flag is enabled.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DRIVE,
      root: VolumeManagerCommon.RootType.DRIVE,
    }];
  }

  showLimit() {
    return 0;
  }
}

customElements.define(TAG_NAME, DriveBulkPinningBanner);
