// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/button/button.js';

import {isCrosComponentsEnabled} from '../../../../common/js/flags.js';
import {visitURL} from '../../../../common/js/util.js';

import {getTemplate} from './educational_banner.html.js';
import {type AllowedVolumeOrType, Banner, BannerEvent, DismissedForeverEventSource} from './types.js';

/**
 * EducationalBanner is a type of banner that is the second highest priority
 * type of banner (below WarningBanner). It is used to highlight new features or
 * contextually relevant information in specific part of the Files app.
 *
 * To implement an EducationalBanner, extend from this banner and override the
 * allowedVolumes method to define the VolumeType you want the banner to be
 * shown on. All other configuration elements are optional and can be found
 * documented on the Banner externs.
 *
 * For example the following banner will show when a user navigates to the
 * Downloads volume type:
 *
 *    class ConcreteEducationalBanner extends EducationalBanner {
 *      allowedVolumes() {
 *        return [{type: VolumeType.DOWNLOADS}];
 *      }
 *    }
 *
 * Create a HTML template with the same file name as the banner and override
 * the text using slots with the content that you want:
 *
 *    <educational-banner>
 *      <span slot="title">Main banner text</span>
 *      <span slot="subtitle">Extra information that appears smaller</span>
 *      <cr-button slot="extra-button" href="{{url_to_navigate}}">
 *        Extra button text
 *      </cr-button>
 *    </educational-banner>
 *
 * There is also an optional HTML attribute that can be added to the
 * extra-button slot called dismiss-banner-when-clicked that will dismiss the
 * banner forever when the extra button is pressed. Example:
 *
 *      <cr-button
 *          slot="extra-button"
 *          href="{{url_to_navigate}}"
 *          dismiss-banner-when-clicked>
 *        Extra button text
 *      </cr-button>
 */
export class EducationalBanner extends Banner {
  constructor() {
    super();

    const fragment = this.getTemplate();
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Returns the HTML template for the Educational Banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * Get the concrete banner instance.
   */
  private getBannerInstance_() {
    const parent =
        this.getRootNode() && (this.getRootNode() as ShadowRoot).host;
    let bannerInstance: EducationalBanner = this;
    // In the case the educational-banner web component is not the root node
    // (e.g. it is contained within another web component) prefer the outer
    // component.
    if (parent && parent instanceof EducationalBanner) {
      bannerInstance = parent;
    }
    return bannerInstance;
  }

  /**
   * Called when the web component is connected to the DOM. This will be called
   * for both the inner warning-banner component and the concrete
   * implementations that extend from it.
   */
  override connectedCallback() {
    // If an EducationalBanner subclass overrides the default dismiss button
    // the button will not exist in the shadowRoot. Add the event listener to
    // the overridden dismiss button first and fall back to the default button
    // if no overridden button.
    const overridenDismissButton =
        this.querySelector('[slot="dismiss-button"]');
    const defaultDismissButton = this.shadowRoot!.querySelector(
        isCrosComponentsEnabled() ? '#dismiss-button' : '#dismiss-button-old');
    if (overridenDismissButton) {
      overridenDismissButton.addEventListener(
          'click',
          (event: Event) => this.onDismissClickHandler_(
              event, DismissedForeverEventSource.OVERRIDEN_DISMISS_BUTTON));
    } else if (defaultDismissButton) {
      defaultDismissButton.addEventListener(
          'click',
          (event: Event) => this.onDismissClickHandler_(
              event, DismissedForeverEventSource.DEFAULT_DISMISS_BUTTON));
    }

    // Attach an onclick handler to the extra-button slot. This enables a new
    // element to leverage the href tag on the element to have a URL opened.
    // TODO(crbug.com/40189485): Add UMA trigger to capture number of extra
    // button clicks.
    const extraButton = this.querySelector('[slot="extra-button"]');
    const href = extraButton?.getAttribute('href');
    if (href && extraButton) {
      extraButton.addEventListener('click', (e) => {
        visitURL(href);
        if (extraButton.hasAttribute('dismiss-banner-when-clicked')) {
          this.dispatchEvent(
              new CustomEvent(BannerEvent.BANNER_DISMISSED_FOREVER, {
                bubbles: true,
                composed: true,
                detail: {
                  banner: this.getBannerInstance_(),
                  eventSource: DismissedForeverEventSource.EXTRA_BUTTON,
                },
              }));
        }
        e.preventDefault();
      });
    }
  }

  /**
   * Only show the banner 3 Files app sessions (unless dismissed). Please refer
   * to the Banner externs for information about Files app session.
   */
  override showLimit() {
    return 3;
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
  private onDismissClickHandler_(
      _: Event, dismissedForeverEventSource: DismissedForeverEventSource) {
    this.dispatchEvent(new CustomEvent(BannerEvent.BANNER_DISMISSED_FOREVER, {
      bubbles: true,
      composed: true,
      detail: {
        banner: this.getBannerInstance_(),
        eventSource: dismissedForeverEventSource,
      },
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'educational-banner': EducationalBanner;
  }
}

customElements.define('educational-banner', EducationalBanner);
