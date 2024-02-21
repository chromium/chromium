// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';
import type {XfBulkPinningDialog} from '../../../../widgets/xf_bulk_pinning_dialog.js';

import {getTemplate} from './drive_bulk_pinning_banner.html.js';
import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-bulk-pinning-banner';

/**
 * A banner that prompts users to bulk pin their files.
 */
export class DriveBulkPinningBanner extends EducationalBanner {
  constructor() {
    super();

    this.shadowRoot!.querySelector('.action-button')!.addEventListener(
        'click', (e: Event) => {
          e.preventDefault();
          const dialog = document.querySelector<XfBulkPinningDialog>(
              'xf-bulk-pinning-dialog')!;
          dialog.show();
        });
  }

  /**
   * Returns the HTML template for the Bulk pinning banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type
   * and the feature flag is enabled.
   */
  override allowedVolumes() {
    return [{
      type: VolumeType.DRIVE,
      root: RootType.DRIVE,
    }];
  }

  /**
   * Show this banner for an unlimited number of sessions.
   */
  override showLimit() {
    return 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveBulkPinningBanner;
  }
}

customElements.define(TAG_NAME, DriveBulkPinningBanner);
