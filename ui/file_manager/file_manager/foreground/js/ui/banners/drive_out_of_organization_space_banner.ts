// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, strf} from '../../../../common/js/translations.js';
import {RootType, VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './drive_out_of_organization_space_banner.html.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'drive-out-of-organization-space-banner';

/**
 * An error banner displayed when the user's Google Drive organization runs
 * out of quota. This is only shown if the user has navigated to the My drive
 * under the Google Drive root excluding other directories such as Computers or
 * Shared drives.
 */
export class DriveOutOfOrganizationSpaceBanner extends WarningBanner {
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

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner.
   */
  override onFilteredContext(context:
                                 chrome.fileManagerPrivate.DriveQuotaMetadata) {
    if (context === undefined || context.organizationName === undefined) {
      console.warn('Context not supplied or missing data');
      return;
    }

    const message =
        strf('DRIVE_ORGANIZATION_QUOTA_OVER', context.organizationName);
    const warning = str('DRIVE_WARNING_QUOTA_OVER');

    const originalSpan =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;

    const replacementSpan = document.createElement('span');
    replacementSpan.setAttribute('slot', 'text');
    replacementSpan.setAttribute('aria-label', `${warning}: ${message}`);

    const replacementSpanInner = document.createElement('span');
    replacementSpanInner.setAttribute('aria-hidden', 'true');
    replacementSpanInner.textContent = message;

    replacementSpan.appendChild(replacementSpanInner);
    originalSpan.replaceWith(replacementSpan);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: DriveOutOfOrganizationSpaceBanner;
  }
}

customElements.define(TAG_NAME, DriveOutOfOrganizationSpaceBanner);
