// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_welcome_banner.html.js';
import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-welcome-banner';

/**
 * A banner that shows when a user navigates to the Google Drive volume. This
 * banner appears for all children of the Google Drive root (e.g. My Drive,
 * Shared with me, Offline etc.)
 */
export class DriveWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Drive Welcome educational banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type.
   */
  override allowedVolumes() {
    return [{
      type: VolumeType.DRIVE,
      root: RootType.DRIVE,
    }];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveWelcomeBanner;
  }
}

customElements.define(TAG_NAME, DriveWelcomeBanner);
