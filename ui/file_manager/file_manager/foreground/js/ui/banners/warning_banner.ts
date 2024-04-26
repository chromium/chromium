// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {visitURL} from '../../../../common/js/util.js';

import {type AllowedVolumeOrType, Banner, BannerEvent} from './types.js';
import {getTemplate} from './warning_banner.html.js';

/**
 * WarningBanner is a type of banner that is highest priority and is used to
 * showcase potential underlying issues for the filesystem (e.g. low disk space)
 * or that are contextually relevant (e.g. Google Drive is offline).
 *
 * To implement a WarningBanner, extend from this banner and override the
 * allowedVolumes method where you want the warning message shown. The
 * connectedCallback method can be used to set the warning text and an optional
 * link to provide more information. All other configuration elements are
 * optional and can be found documented on the Banner extern.
 *
 * For example the following banner will show when a user navigates to the
 * Downloads volume type:
 *
 *    class ConcreteWarningBanner extends WarningBanner {
 *      allowedVolumes() {
 *        return [{type: VolumeType.DOWNLOADS}];
 *      }
 *    }
 *
 * Create a HTML template with the same file name as the banner and override
 * the text using slots with the content that you want:
 *
 *    <warning-banner>
 *      <span slot="text">Warning Banner text</span>
 *      <cr-button slot="extra-button" href="{{url_to_navigate}}">
 *        Extra button text
 *      </cr-button>
 *    </warning-banner>
 */
export class WarningBanner extends Banner {
  constructor() {
    super();

    const fragment = this.getTemplate();
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Returns the HTML template for the Warning Banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Called when the web component is connected to the DOM. This will be called
   * for both the inner warning-banner component and the concrete
   * implementations that extend from it.
   */
  override connectedCallback() {
    // If a WarningBanner subclass overrides the default dismiss button, the
    // button will not exist in the shadowRoot. Add the event listener to the
    // overridden dismiss button first and fall back to the default button if
    // no overridden button.
    const overridenDismissButton =
        this.querySelector('[slot="dismiss-button"]');
    const defaultDismissButton =
        this.shadowRoot!.querySelector('#dismiss-button');
    if (overridenDismissButton) {
      overridenDismissButton.addEventListener(
          'click', this.onDismissClickHandler_.bind(this));
    } else if (defaultDismissButton) {
      defaultDismissButton.addEventListener(
          'click', this.onDismissClickHandler_.bind(this));
    }

    // Attach an onclick handler to the extra-button slot. This enables a new
    // element to leverage the href tag on the element to have a URL opened.
    // TODO(crbug.com/40189485): Add UMA trigger to capture number of extra
    // button clicks.
    const extraButton = this.querySelector('[slot="extra-button"]');
    if (extraButton) {
      extraButton.addEventListener('click', (e) => {
        if (extraButton.getAttribute('href')) {
          visitURL(extraButton.getAttribute('href')!);
        }
        e.preventDefault();
      });
    }
  }

  /**
   * When a WarningBanner is dismissed, do not show it again for another 36
   * hours.
   */
  override hideAfterDismissedDurationSeconds() {
    return 36 * 60 * 60;  // 36 hours, 129,600 seconds.
  }

  /**
   * All banners that inherit this class should override with their own
   * volume types to allow. Setting this explicitly as an empty array ensures
   * banners that don't override this are not shown by default.
   */
  override allowedVolumes(): AllowedVolumeOrType[] {
    return [];
  }

  /**
   * Handler for the dismiss button on click, switches to the custom banner
   * dismissal event to ensure the controller can catch the event.
   */
  private onDismissClickHandler_(_: Event) {
    const parent =
        this.getRootNode() && (this.getRootNode() as ShadowRoot).host;
    let bannerInstance: WarningBanner = this;
    // In the case the warning-banner web component is not the root node (e.g.
    // it is contained within another web component) prefer the outer component.
    if (parent && parent instanceof WarningBanner) {
      bannerInstance = parent;
    }
    this.dispatchEvent(new CustomEvent(
        BannerEvent.BANNER_DISMISSED,
        {bubbles: true, composed: true, detail: {banner: bannerInstance}}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'warning-banner': WarningBanner;
  }
}

customElements.define('warning-banner', WarningBanner);
