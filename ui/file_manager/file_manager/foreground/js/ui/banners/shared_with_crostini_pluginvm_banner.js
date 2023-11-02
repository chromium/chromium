// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {str} from '../../../../common/js/util.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';
import {constants} from '../../constants.js';
import {StateBanner} from './state_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'shared-with-crostini-pluginvm-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows if the current navigated directory has been shared with
 * Crostini or PluginVM.
 */
export class SharedWithCrostiniPluginVmBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * This banner relies on a custom trigger registered in the BannerController
   * and thus the following list are root types where sharing to Crostini or
   * PluginVM is allowed and thus a banner may appear.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [
      {root: VolumeManagerCommon.RootType.DOWNLOADS},
      {root: VolumeManagerCommon.RootType.REMOVABLE},
      {root: VolumeManagerCommon.RootType.ANDROID_FILES},
      {root: VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT},
      {root: VolumeManagerCommon.RootType.COMPUTER},
      {root: VolumeManagerCommon.RootType.DRIVE},
      {root: VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT},
      {root: VolumeManagerCommon.RootType.SHARED_DRIVE},
      {root: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME},
      {root: VolumeManagerCommon.RootType.CROSTINI},
      {root: VolumeManagerCommon.RootType.ARCHIVE},
      {root: VolumeManagerCommon.RootType.SMB},
    ];
  }

  /**
   * Persist the banner at all times if the folder is shared.
   * @returns {number}
   */
  timeLimit() {
    return Banner.INIFINITE_TIME;
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. This type is used to identify if this folder is
   * shared with Crostini, PluginVM or both and update the text and links
   * accordingly.
   * @param {!Object} context The type of VM sharing this folder.
   */
  onFilteredContext(context) {
    if (!context || !context.type) {
      console.warn('Context not supplied or type key missing');
      return;
    }
    const text = this.shadowRoot.querySelector('span[slot="text"]');
    const button =
        this.shadowRoot.querySelector('cr-button[slot="extra-button"]');
    if (context.type ===
        (constants.DEFAULT_CROSTINI_VM + constants.PLUGIN_VM)) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI_AND_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    if (context.type === constants.PLUGIN_VM) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI');
    button.setAttribute('href', 'chrome://os-settings/crostini/sharedPaths');
  }
}

customElements.define(TAG_NAME, SharedWithCrostiniPluginVmBanner);
