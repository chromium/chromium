// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {StateBanner} from './state_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'trash-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows users navigating to their Trash directory that files in
 * the Trash directory are automatically removed after 30 days.
 */
export class TrashBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Trash rootType.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{root: VolumeManagerCommon.RootType.TRASH}];
  }

  /**
   * The Trash banner should always be visible in the Trash root.
   * @returns {number}
   */
  timeLimit() {
    return Banner.INIFINITE_TIME;
  }
}

customElements.define(TAG_NAME, TrashBanner);
