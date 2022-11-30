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
export const TAG_NAME = 'photos-welcome-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows when a user navigates to the Google Photos documents
 * provider. Shows helpful information about using Photos on ChromeOS.
 */
export class PhotosWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Google Photos Welcome banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Documents Provider
   * volume, specifically the Photos document provider.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      id: VolumeManagerCommon.PHOTOS_DOCUMENTS_PROVIDER_VOLUME_ID,
    }];
  }
}

customElements.define(TAG_NAME, PhotosWelcomeBanner);
