// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {metrics} from '../../../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';

import {EducationalBanner} from './educational_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'google-one-offer-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * GoogleOneOfferBanner shows when the user navigates to the Google Drive
 * volume. This banner will be shown instead of DriveWelcomeBanner if
 * GoogleOneOfferFilesBanner flag is on.
 */
export class GoogleOneOfferBanner extends EducationalBanner {
  /**
   * Returns the HTML template for the Google One offer educational banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Only show the banner when the user has navigated to the Drive volume type.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [{
      type: VolumeManagerCommon.VolumeType.DRIVE,
      root: VolumeManagerCommon.RootType.DRIVE,
    }];
  }

  /**
   * Called when the banner gets connected to the DOM.
   */
  connectedCallback() {
    const getPerkButton =
        this.shadowRoot.querySelector('[slot="extra-button"]');
    if (getPerkButton) {
      getPerkButton.addEventListener(
          'click', event => this.onGetPerkButtonClickHandler_());
    }

    this.addEventListener(
        Banner.Event.BANNER_DISMISSED_FOREVER,
        event => this.onBannerDismissedForever_(
            /** @type {!CustomEvent} */ (event)));
  }

  /**
   * Called when the banner gets shown in the UI.
   */
  onShow() {
    metrics.recordUserAction(GoogleOneOfferBanner.UserActions.SHOWN);
  }

  /**
   * Called when the get perk button gets clicked.
   */
  onGetPerkButtonClickHandler_() {
    metrics.recordUserAction(GoogleOneOfferBanner.UserActions.GET_PERK);
  }

  /**
   * Called when DismissedForeverEvent gets dispatched for this banner. Note
   * that the event can gets dispatched for dismiss caused by a click of get
   * perk button.
   * @param {!CustomEvent} event
   */
  onBannerDismissedForever_(event) {
    // UserActions.DISMISS should not be recorded for dismiss caused by a click
    // of the get perk button.
    if (event.detail.eventSource ===
        Banner.DismissedForeverEventSource.EXTRA_BUTTON) {
      return;
    }

    metrics.recordUserAction(GoogleOneOfferBanner.UserActions.DISMISS);
  }
}

/**
 * User actions of GoogleOneOfferBanner.
 * @enum {string}
 * @const
 */
GoogleOneOfferBanner.UserActions = {
  SHOWN: 'GoogleOneOffer.Shown',
  GET_PERK: 'GoogleOneOffer.GetPerk',
  DISMISS: 'GoogleOneOffer.Dismiss',
};

customElements.define(TAG_NAME, GoogleOneOfferBanner);