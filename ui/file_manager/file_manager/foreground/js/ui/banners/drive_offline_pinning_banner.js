// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {str, strf, util} from '../../../../common/js/util.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'drive-offline-pinning-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows users they can pin Docs / Sheets / Slides in Google Drive
 * and have them available offline.
 */
export class DriveOfflinePinningBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Drive Offline Pinning educational banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * The Drive offline pinning banner uses a templatised string to ensure the
   * toggle name is a reference. This can't be achieved with C++ template
   * replacements, so set it once the web component has been connected to the
   * DOM.
   */
  connectedCallback() {
    const subtitle = this.shadowRoot.querySelector('span[slot="subtitle"]');
    subtitle.innerText =
        strf('DRIVE_OFFLINE_BANNER_SUBTITLE', str('OFFLINE_COLUMN_LABEL'));
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type
   * and the feature flag is enabled.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    if (util.isDriveDssPinEnabled()) {
      return [{
        type: VolumeManagerCommon.VolumeType.DRIVE,
        root: VolumeManagerCommon.RootType.DRIVE,
      }];
    }
    return [];
  }
}

customElements.define(TAG_NAME, DriveOfflinePinningBanner);
