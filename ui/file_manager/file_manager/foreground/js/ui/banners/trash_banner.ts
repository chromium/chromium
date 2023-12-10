// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType} from '../../../../common/js/volume_manager_types.js';

import {StateBanner} from './state_banner.js';
import {getTemplate} from './trash_banner.html.js';
import {BANNER_INFINITE_TIME} from './types.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'trash-banner';

/**
 * A banner that shows users navigating to their Trash directory that files in
 * the Trash directory are automatically removed after 30 days.
 */
export class TrashBanner extends StateBanner {
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
   * Only show the banner when the user has navigated to the Trash rootType.
   */
  override allowedVolumes() {
    return [{root: RootType.TRASH}];
  }

  /**
   * The Trash banner should always be visible in the Trash root.
   */
  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: TrashBanner;
  }
}

customElements.define(TAG_NAME, TrashBanner);
