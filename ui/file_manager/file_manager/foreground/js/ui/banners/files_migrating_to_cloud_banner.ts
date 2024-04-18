// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * The context contains the current default download location, which is
   * usually My Files/Downloads. When Skyvault is enabled, it should be Google
   * Drive or OneDrive.
   */
  override onFilteredContext(context: {
    defaultLocation: chrome.fileManagerPrivate.DefaultLocation,
  }) {
    if (isNullOrUndefined(context) ||
        isNullOrUndefined(context.defaultLocation)) {
      console.warn('Context not supplied or defaultLocation key missing.');
      return;
    }
    const text =
        this.shadowRoot!.querySelector('span[slot="text"]')! as HTMLSpanElement;

    function getMessage(driveLabel: string): string {
      // TODO(b/334511998): Replace with i18n strings.
      return 'Your administrator has restricted the use of local storage. All ' +
          `of your files are being automatically migrated to ${driveLabel}. ` +
          'You can modify these files once the transfer has been completed.';
    }

    switch (context.defaultLocation) {
      case chrome.fileManagerPrivate.DefaultLocation.GOOGLE_DRIVE:
        text.innerText = getMessage('Google Drive');
        return;
      case chrome.fileManagerPrivate.DefaultLocation.ONEDRIVE:
        text.innerText = getMessage('Microsoft OneDrive');
        return;
      case chrome.fileManagerPrivate.DefaultLocation.MY_FILES:
      default:
        console.warn(
            `Unsupported default location ${context.defaultLocation}.`);
        return;
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
