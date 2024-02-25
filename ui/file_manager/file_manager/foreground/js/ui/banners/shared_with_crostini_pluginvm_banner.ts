// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str} from '../../../../common/js/translations.js';
import {RootType} from '../../../../common/js/volume_manager_types.js';
import {DEFAULT_CROSTINI_VM, PLUGIN_VM} from '../../constants.js';

import {getTemplate} from './shared_with_crostini_pluginvm_banner.html.js';
import {StateBanner} from './state_banner.js';
import {BANNER_INFINITE_TIME} from './types.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'shared-with-crostini-pluginvm-banner';

/**
 * A banner that shows if the current navigated directory has been shared with
 * Crostini or PluginVM.
 */
export class SharedWithCrostiniPluginVmBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * This banner relies on a custom trigger registered in the BannerController
   * and thus the following list are root types where sharing to Crostini or
   * PluginVM is allowed and thus a banner may appear.
   */
  override allowedVolumes() {
    return [
      {root: RootType.DOWNLOADS},
      {root: RootType.REMOVABLE},
      {root: RootType.ANDROID_FILES},
      {root: RootType.COMPUTERS_GRAND_ROOT},
      {root: RootType.COMPUTER},
      {root: RootType.DRIVE},
      {root: RootType.SHARED_DRIVES_GRAND_ROOT},
      {root: RootType.SHARED_DRIVE},
      {root: RootType.DRIVE_SHARED_WITH_ME},
      {root: RootType.CROSTINI},
      {root: RootType.ARCHIVE},
      {root: RootType.SMB},
    ];
  }

  /**
   * Persist the banner at all times if the folder is shared.
   */
  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. This type is used to identify if this folder is
   * shared with Crostini, PluginVM or both and update the text and links
   * accordingly.
   */
  override onFilteredContext(context: {type: string}) {
    if (!context || !context.type) {
      console.warn('Context not supplied or type key missing');
      return;
    }
    const text =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;
    const button = this.shadowRoot!.querySelector<HTMLButtonElement>(
        'cr-button[slot="extra-button"]')!;
    if (context.type === (DEFAULT_CROSTINI_VM + PLUGIN_VM)) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI_AND_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    if (context.type === PLUGIN_VM) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI');
    button.setAttribute('href', 'chrome://os-settings/crostini/sharedPaths');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: SharedWithCrostiniPluginVmBanner;
  }
}

customElements.define(TAG_NAME, SharedWithCrostiniPluginVmBanner);
