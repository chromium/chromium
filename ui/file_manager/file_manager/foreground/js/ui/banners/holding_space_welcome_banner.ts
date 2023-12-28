// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';
import {getAllowedVolumeTypes, maybeStoreTimeOfFirstWelcomeBannerShow} from '../../holding_space_util.js';

import {EducationalBanner} from './educational_banner.js';
import {getTemplate} from './holding_space_welcome_banner.html.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'holding-space-welcome-banner';

/**
 * A banner that shows when a user navigates to a volume that allows pinning
 * of files to the shelf. Highlights to the user how to use the Holding space
 * feature.
 */
export class HoldingSpaceWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Holding Space Welcome educational banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Returns the list of allow volume types remapping over the canonical source
   * at HoldingSpaceUtil.
   */
  override allowedVolumes() {
    return getAllowedVolumeTypes().map((type: VolumeType|null) => {
      if (type === VolumeType.DRIVE) {
        return {
          type: VolumeType.DRIVE,
          root: RootType.DRIVE,
        };
      }
      return {type: type!};
    });
  }

  /**
   * Store the time the banner was first shown.
   */
  override onShow() {
    maybeStoreTimeOfFirstWelcomeBannerShow();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: HoldingSpaceWelcomeBanner;
  }
}

customElements.define(TAG_NAME, HoldingSpaceWelcomeBanner);
