// Copyright 2023 The Chromium Authors
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
export const TAG_NAME = 'drive-out-of-shared-drive-space-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * An error banner displayed when the user runs out of their Google Drive's
 * individual quota. This is only shown if the user has navigated to the My
 * drive under the Google Drive root excluding other directories such as
 * Computers or Shared drives.
 */
export class DriveOutOfSharedDriveSpaceBanner extends WarningBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the My drive directory
   * and all children. Having root and type means this error does not show on
   * Shared drives, Team drives, Computers or Offline.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DRIVE,
      root: VolumeManagerCommon.RootType.SHARED_DRIVE,
    }];
  }
}

customElements.define(TAG_NAME, DriveOutOfSharedDriveSpaceBanner);
