// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {visitURL} from '../../../../common/js/util.js';
import {Command} from '../command.js';

import {getTemplate} from './state_banner.html.js';
import {type AllowedVolumeOrType, Banner} from './types.js';

/**
 * State banner is a type of banner that indicates the Files app has reached a
 * certain state, e.g. the current folder is shared with Linux.
 *
 * To implement an StateBanner, extend from this banner and override the
 * allowedVolumes method to define the VolumeType you want the banner to be
 * shown on. All other configuration elements are optional and can be found
 * documented on the Banner externs.
 *
 * For example the following banner will show when a user navigates to the
 * Downloads volume type:
 *
 *    class ConcreteStateBanner extends StateBanner {
 *      allowedVolumes() {
 *        return [{type: VolumeType.DOWNLOADS}];
 *      }
 *    }
 *
 * Create a HTML template with the same file name as the banner and override
 * the text using slots with the content that you want:
 *
 *    <state-banner>
 *      <span slot="text">Main banner text</span>
 *      <cr-button slot="extra-button" href="{{url_to_navigate}}">
 *        Extra button text
 *      </cr-button>
 *    </state-banner>
 */
export class StateBanner extends Banner {
  constructor() {
    super();

    const fragment = this.getTemplate();
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Returns the HTML template for the State Banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Called when the web component is connected to the DOM. This will be called
   * for both the inner state-banner component and the concrete
   * implementations that extend from it.
   */
  override connectedCallback() {
    // Attach an onclick handler to the extra-button slot. This enables a new
    // element to leverage the href tag on the element to have a URL opened.
    // TODO(crbug.com/40189485): Add UMA trigger to capture number of extra
    // button clicks.
    const extraButton = this.querySelector('[slot="extra-button"]');
    if (extraButton) {
      extraButton.addEventListener('click', (e: Event) => {
        const href = extraButton.getAttribute('href');
        const chromeOsSettingsSubpage =
            href && href.replace('chrome://os-settings/', '');
        if (chromeOsSettingsSubpage && chromeOsSettingsSubpage !== href) {
          chrome.fileManagerPrivate.openSettingsSubpage(
              chromeOsSettingsSubpage);
          e.preventDefault();
          return;
        }
        const commandName = extraButton.getAttribute('command');
        if (commandName) {
          const command =
              assertInstanceof(document.querySelector(commandName), Command);
          // Unit tests don't enclose a StateBanner inside a concrete banner,
          // so we want to ensure the event is appropriately dispatched from the
          // outer scope otherwise it won't bubble up to the commands.
          let bannerInstance: StateBanner = this;
          const parentBanner =
              this.getRootNode() && (this.getRootNode() as ShadowRoot).host;
          if (parentBanner && parentBanner instanceof StateBanner) {
            bannerInstance = parentBanner;
          }
          command.execute(bannerInstance);
          e.preventDefault();
          return;
        }
        visitURL(extraButton.getAttribute('href')!);
        e.preventDefault();
      });
    }
  }

  /**
   * All banners that inherit this class should override with their own
   * volume types to allow. Setting this explicitly as an empty array ensures
   * banners that don't override this are not shown by default.
   */
  override allowedVolumes(): AllowedVolumeOrType[] {
    return [];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'state-banner': StateBanner;
  }
}

customElements.define('state-banner', StateBanner);
