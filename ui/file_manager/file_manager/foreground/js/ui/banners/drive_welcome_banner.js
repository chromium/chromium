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
export const TAG_NAME = 'drive-welcome-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows when a user navigates to the Google Drive volume. This
 * banner appears for all children of the Google Drive root (e.g. My Drive,
 * Shared with me, Offline etc.)
 */
export class DriveWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Drive Welcome educational banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DRIVE,
      root: VolumeManagerCommon.RootType.DRIVE,
    }];
  }
}

customElements.define(TAG_NAME, DriveWelcomeBanner);
