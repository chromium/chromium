// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bytesToString, strf} from '../../../../common/js/translations.js';
import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_low_shared_drive_space_banner.html.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-shared-drive-low-space-banner';

/**
 * A banner that shows a warning when the remaining space on a Google Shared
 * Drive goes below 20%.
 */
export class DriveLowSharedDriveSpaceBanner extends WarningBanner {
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
   * Show the banner when the Drive volume has gone below 20% remaining space.
   */
  override diskThreshold() {
    return {
      type: RootType.DRIVE,
      minRatio: 0.2,
    };
  }

  /**
   * Only show the banner when the user has navigated to a Shared Drive.
   */
  override allowedVolumes() {
    return [{
      type: VolumeType.DRIVE,
      root: RootType.SHARED_DRIVE,
    }];
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner.
   */
  override onFilteredContext(context:
                                 chrome.fileManagerPrivate.DriveQuotaMetadata) {
    if (!context || context.totalBytes === null || context.usedBytes === null) {
      console.warn('Context not supplied or missing data');
      return;
    }
    this.shadowRoot!.querySelector<HTMLSpanElement>(
                        'span[slot="text"]')!.innerText =
        strf(
            'DRIVE_SHARED_DRIVE_QUOTA_LOW',
            Math.ceil(
                (context.totalBytes - context.usedBytes) / context.totalBytes *
                100),
            bytesToString(context.totalBytes));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveLowSharedDriveSpaceBanner;
  }
}

customElements.define(TAG_NAME, DriveLowSharedDriveSpaceBanner);
