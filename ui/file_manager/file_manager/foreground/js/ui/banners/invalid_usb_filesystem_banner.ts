// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str} from '../../../../common/js/translations.js';
import {RootType, VolumeError} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './invalid_usb_filesystem_banner.html.js';
import {StateBanner} from './state_banner.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'invalid-usb-filesystem-banner';

/**
 * A banner that shows is a removable device is plugged in and it has a
 * filesystem that is either unknown or unsupported. It includes an action
 * button for the user to format the device.
 */
export class InvalidUsbFileSystemBanner extends StateBanner {
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
   * Only show the banner when the user has navigated to the Removable root type
   * this is used in conjunction with a custom filter to ensure only removable
   * roots with errors are shown the banner.
   */
  override allowedVolumes() {
    return [{root: RootType.REMOVABLE}];
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. This is used to identify if the device has an
   * unsupported OR unknown file system.
   */
  override onFilteredContext(context: {error: VolumeError}) {
    if (!context || !context.error) {
      console.warn('Context not supplied or error key missing');
      return;
    }
    const text =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;
    if (context.error === VolumeError.UNSUPPORTED_FILESYSTEM) {
      text.innerText = str('UNSUPPORTED_FILESYSTEM_WARNING');
      return;
    }

    text.innerText = str('UNKNOWN_FILESYSTEM_WARNING');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: InvalidUsbFileSystemBanner;
  }
}

customElements.define(TAG_NAME, InvalidUsbFileSystemBanner);
