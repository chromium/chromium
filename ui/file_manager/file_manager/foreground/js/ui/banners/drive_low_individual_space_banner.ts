// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bytesToString, strf} from '../../../../common/js/translations.js';
import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_low_individual_space_banner.html.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-individual-low-space-banner';

/**
 * A banner that shows a warning when the remaining space on a Google Drive goes
 * below 20%. This is only shown if the user has navigated to the My drive under
 * the Google Drive root excluding other directories such as Computers or
 * Shared drives.
 */
export class DriveLowIndividualSpaceBanner extends WarningBanner {
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
   * Only show the banner when the user has navigated to the My drive directory
   * and all children. Having root and type means this warning does not show on
   * Shared drives, Team drives, Computers or Offline.
   */
  override allowedVolumes() {
    return [{
      type: VolumeType.DRIVE,
      root: RootType.DRIVE,
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
            'DRIVE_INDIVIDUAL_QUOTA_LOW',
            Math.ceil(
                (context.totalBytes - context.usedBytes) / context.totalBytes *
                100),
            bytesToString(context.totalBytes));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveLowIndividualSpaceBanner;
  }
}

customElements.define(TAG_NAME, DriveLowIndividualSpaceBanner);
