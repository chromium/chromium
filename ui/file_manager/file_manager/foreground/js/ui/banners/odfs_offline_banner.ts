// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './odfs_offline_banner.html.js';
import {BANNER_INFINITE_TIME} from './types.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'odfs-offline-banner';

/**
 * A banner to emphasize that OneDrive isn't usable while the device is offline.
 */
export class OdfsOfflineBanner extends WarningBanner {
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
   * Persist the banner at all times.
   */
  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }

  /**
   * Only show the banner when the user has navigated to a provided volume.
   */
  override allowedVolumes() {
    return [
      {type: VolumeType.PROVIDED},
    ];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: OdfsOfflineBanner;
  }
}

customElements.define(TAG_NAME, OdfsOfflineBanner);
