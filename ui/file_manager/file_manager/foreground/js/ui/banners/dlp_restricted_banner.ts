// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str} from '../../../../common/js/translations.js';
import {RootType} from '../../../../common/js/volume_manager_types.js';
import {DialogType} from '../../../../state/state.js';

import {getTemplate} from './dlp_restricted_banner.html.js';
import {StateBanner} from './state_banner.js';
import {BANNER_INFINITE_TIME} from './types.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'dlp-restricted-banner';

/**
 * A banner that shows that some of the files or folders in the current
 * directory are restricted by Data Leak Prevention (DLP).
 */
export class DlpRestrictedBanner extends StateBanner {
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
   * This banner relies on a custom trigger registered in the BannerController.
   * It is shown in SELECT_OPEN_FILE and SELECT_OPEN_MULTI_FILE dialog types
   * when some files are restricted by DLP, and in SELECT_SAVEAS_FILE dialog
   * when some destinations are restricted. Regardless of the dialog type, the
   * user can navigate to different roots so the banner can be shown in any of
   * them.
   */
  override allowedVolumes() {
    return Object.values(RootType).map(x => ({root: x}));
  }

  /**
   * Persist the banner at all times if the folder is shared.
   */
  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. The type, which is either File Picker or File Saver,
   * determines the text used in the banner.
   */
  override onFilteredContext(context: {type: DialogType}) {
    if (!context || context.type === null) {
      console.warn('Context not supplied or dialog type key missing.');
      return;
    }
    const text =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;
    switch (context.type) {
      case DialogType.SELECT_OPEN_FILE:
      case DialogType.SELECT_OPEN_MULTI_FILE:
        text.innerText = str('DLP_FILE_PICKER_BANNER');
        return;
      case DialogType.SELECT_SAVEAS_FILE:
        text.innerText = str('DLP_FILE_SAVER_BANNER');
        return;
      default:
        console.warn(`The DLP banner should not be shown for ${context.type}.`);
        return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DlpRestrictedBanner;
  }
}

customElements.define(TAG_NAME, DlpRestrictedBanner);
