// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str} from '../../../../common/js/translations.js';
import {isNullOrUndefined} from '../../../../common/js/util.js';
import {VolumeType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './files_migrating_to_cloud_banner.html.js';
import {BANNER_INFINITE_TIME} from './types.js';
import {WarningBanner} from './warning_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'files-migrating-to-cloud-banner';

/**
 * A banner that shows a warning when SkyVault policies are set and the user
 * still has some files stored locally that are being moved to the cloud. It's
 * only shown in local directories (My Files/).
 */
export class FilesMigratingToCloudBanner extends WarningBanner {
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
   * The context contains the CloudProvider set as SkyVault migration
   * destination.
   */
  override onFilteredContext(context: {
    cloudProvider: chrome.fileManagerPrivate.CloudProvider,
  }) {
    if (isNullOrUndefined(context) ||
        isNullOrUndefined(context.cloudProvider)) {
      console.warn('Context not supplied or defaultLocation key missing.');
      return;
    }
    const text =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;

    switch (context.cloudProvider) {
      case chrome.fileManagerPrivate.CloudProvider.GOOGLE_DRIVE:
        text.innerText = str('SKYVAULT_MIGRATION_BANNER_GOOGLE_DRIVE');
        return;
      case chrome.fileManagerPrivate.CloudProvider.ONEDRIVE:
        text.innerText = str('SKYVAULT_MIGRATION_BANNER_ONEDRIVE');
        return;
      case chrome.fileManagerPrivate.CloudProvider.NOT_SPECIFIED:
        console.warn(`Cloud provider must be specified.`);
    }
  }


  /**
   * Only show the banner when the user has navigated to a local volume.
   */
  override allowedVolumes() {
    return [
      {type: VolumeType.DOWNLOADS},
      {type: VolumeType.MY_FILES},
    ];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: FilesMigratingToCloudBanner;
  }
}

customElements.define(TAG_NAME, FilesMigratingToCloudBanner);
