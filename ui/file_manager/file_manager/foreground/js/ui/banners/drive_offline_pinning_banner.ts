// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, strf} from '../../../../common/js/translations.js';
import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_offline_pinning_banner.html.js';
import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-offline-pinning-banner';

/**
 * A banner that shows users they can pin Docs / Sheets / Slides in Google Drive
 * and have them available offline.
 */
export class DriveOfflinePinningBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Drive Offline Pinning educational banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * The Drive offline pinning banner uses a templatised string to ensure the
   * toggle name is a reference. This can't be achieved with C++ template
   * replacements, so set it once the web component has been connected to the
   * DOM.
   */
  override connectedCallback() {
    super.connectedCallback();
    const subtitle = this.shadowRoot!.querySelector<HTMLSpanElement>(
        'span[slot="subtitle"]')!;
    subtitle.innerText =
        strf('DRIVE_OFFLINE_BANNER_SUBTITLE', str('OFFLINE_COLUMN_LABEL'));
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
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveOfflinePinningBanner;
  }
}

customElements.define(TAG_NAME, DriveOfflinePinningBanner);
