// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_out_of_individual_space_banner.html.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-out-of-individual-space-banner';

/**
 * An error banner displayed when the user runs out of their Google Drive's
 * individual quota. This is only shown if the user has navigated to the My
 * drive under the Google Drive root excluding other directories such as
 * Computers or Shared drives.
 */
export class DriveOutOfIndividualSpaceBanner extends WarningBanner {
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
   * Only show the banner when the user has navigated to the My drive directory
   * and all children. Having root and type means this error does not show on
   * Shared drives, Team drives, Computers or Offline.
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
    [TAG_NAME]: DriveOutOfIndividualSpaceBanner;
  }
}

customElements.define(TAG_NAME, DriveOutOfIndividualSpaceBanner);
