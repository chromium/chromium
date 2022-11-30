// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {str} from '../../../../common/js/util.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {StateBanner} from './state_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'invalid-usb-filesystem-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows is a removable device is plugged in and it has a
 * filesystem that is either unknown or unsupported. It includes an action
 * button for the user to format the device.
 */
export class InvalidUSBFileSystemBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Removable root type
   * this is used in conjunction with a custom filter to ensure only removable
   * roots with errors are shown the banner.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{root: VolumeManagerCommon.RootType.REMOVABLE}];
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. This is used to identify if the device has an
   * unsupported OR unknown file system.
   * @param {!Object} context The device error of the removable device.
   */
  onFilteredContext(context) {
    if (!context || !context.error) {
      console.warn('Context not supplied or error key missing');
      return;
    }
    const text = this.shadowRoot.querySelector('span[slot="text"]');
    if (context.error ===
        VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
      text.innerText = str('UNSUPPORTED_FILESYSTEM_WARNING');
      return;
    }

    text.innerText = str('UNKNOWN_FILESYSTEM_WARNING');
  }
}

customElements.define(TAG_NAME, InvalidUSBFileSystemBanner);
