// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './local_disk_low_space_banner.html.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'local-disk-low-space-banner';

/**
 * A banner that shows a warning when the remaining space on a local disk goes
 * below 1GB. This is only shown if the user has navigated to the My files /
 * Downloads directories (and any children).
 */
export class LocalDiskLowSpaceBanner extends WarningBanner {
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
   * Show the banner when the Downloads volume (local disk) is less than or
   * equal to 1 GB of remaining space.
   */
  override diskThreshold() {
    return {
      type: RootType.DOWNLOADS,
      minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
    };
  }

  /**
   * Only show the banner when the user has navigated to the Downloads volume
   * type (this includes the My files directory).
   */
  override allowedVolumes() {
    return [{type: VolumeType.DOWNLOADS}];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: LocalDiskLowSpaceBanner;
  }
}

customElements.define(TAG_NAME, LocalDiskLowSpaceBanner);
