// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {util} from '../../../../common/js/util.js';
import {Banner} from '../../../../externs/banner.js';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

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
 *        return [{type: VolumeManagerCommon.VolumeType.DOWNLOADS}];
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
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Get the concrete banner instance.
   * @returns {!EducationalBanner}
   * @private
   */
  getBannerInstance_() {
    const parent = this.getRootNode() && this.getRootNode().host;
    let bannerInstance = this;
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
  connectedCallback() {
    // If an EducationalBanner subclass overrides the default dismiss button
    // the button will not exist in the shadowRoot. Add the event listener to
    // the overridden dismiss button first and fall back to the default button
    // if no overridden button.
    const overridenDismissButton =
        this.querySelector('[slot="dismiss-button"]');
    const defaultDismissButton =
        this.shadowRoot.querySelector('#dismiss-button');
    if (overridenDismissButton) {
      overridenDismissButton.addEventListener(
          'click',
          event => this.onDismissClickHandler_(
              event,
              Banner.DismissedForeverEventSource.OVERRIDEN_DISMISS_BUTTON));
    } else if (defaultDismissButton) {
      defaultDismissButton.addEventListener(
          'click',
          event => this.onDismissClickHandler_(
              event,
              Banner.DismissedForeverEventSource.DEFAULT_DISMISS_BUTTON));
    }

    // Attach an onclick handler to the extra-button slot. This enables a new
    // element to leverage the href tag on the element to have a URL opened.
    // TODO(crbug.com/1228128): Add UMA trigger to capture number of extra
    // button clicks.
    const extraButton = this.querySelector('[slot="extra-button"]');
    const href = extraButton?.getAttribute('href');
    if (href) {
      extraButton.addEventListener('click', (e) => {
        util.visitURL(/** @type {!string} */ (href));
        if (extraButton.hasAttribute('dismiss-banner-when-clicked')) {
          this.dispatchEvent(
              new CustomEvent(Banner.Event.BANNER_DISMISSED_FOREVER, {
                bubbles: true,
                composed: true,
                detail: {
                  banner: this.getBannerInstance_(),
                  eventSource: Banner.DismissedForeverEventSource.EXTRA_BUTTON,
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
   * @returns {number}
   */
  showLimit() {
    return 3;
  }

  /**
   * All banners that inherit this class should override with their own
   * volume types to allow. Setting this explicitly as an empty array ensures
   * banners that don't override this are not shown by default.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [];
  }

  /**
   * Handler for the dismiss button on click, switches to the custom banner
   * dismissal event to ensure the controller can catch the event.
   * @param {!Event} event The click event.
   * @param {!Banner.DismissedForeverEventSource} dismissedForeverEventSource A
   *     source of this event.
   * @private
   */
  onDismissClickHandler_(event, dismissedForeverEventSource) {
    this.dispatchEvent(new CustomEvent(Banner.Event.BANNER_DISMISSED_FOREVER, {
      bubbles: true,
      composed: true,
      detail: {
        banner: this.getBannerInstance_(),
        eventSource: dismissedForeverEventSource,
      },
    }));
  }
}

customElements.define('educational-banner', EducationalBanner);
