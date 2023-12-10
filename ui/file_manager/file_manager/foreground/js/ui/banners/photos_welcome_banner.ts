// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PHOTOS_DOCUMENTS_PROVIDER_VOLUME_ID, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {EducationalBanner} from './educational_banner.js';
import {getTemplate} from './photos_welcome_banner.html.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'photos-welcome-banner';

/**
 * A banner that shows when a user navigates to the Google Photos documents
 * provider. Shows helpful information about using Photos on ChromeOS.
 */
export class PhotosWelcomeBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Google Photos Welcome banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Only show the banner when the user has navigated to the Documents Provider
   * volume, specifically the Photos document provider.
   */
  override allowedVolumes() {
    return [{
      type: VolumeType.DOCUMENTS_PROVIDER,
      id: PHOTOS_DOCUMENTS_PROVIDER_VOLUME_ID,
    }];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: PhotosWelcomeBanner;
  }
}

customElements.define(TAG_NAME, PhotosWelcomeBanner);
