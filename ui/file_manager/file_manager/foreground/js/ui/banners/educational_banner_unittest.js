// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {mockUtilVisitURL} from '../../../../common/js/mock_util.js';
import {Banner} from '../../../../externs/banner.js';

import {EducationalBanner} from './educational_banner.js';

/** @type{!EducationalBanner} */
let educationalBanner;

export function setUp() {
  const htmlTemplate = `<educational-banner>
      <span slot="title">Banner title</span>
      <span slot="subtitle">Subtitle</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
      <button slot="dismiss-button" id="dismiss-button">
        Dismiss
      </button>
    </educational-banner>
    `;
  document.body.innerHTML = htmlTemplate;
  educationalBanner = /** @type{!EducationalBanner} */ (
      document.body.querySelector('educational-banner'));
}

/**
 * Test that the dismiss handler bubbles the correct event on click.
 */
export async function testDismissHandlerEmitsEvent(done) {
  const handler = () => {
    done();
  };
  educationalBanner.addEventListener(
      Banner.Event.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.querySelector('[slot="dismiss-button"]').click();
}

/**
 * Test that the default dismiss button from the educational banner successfully
 * emits the BANNER_DISMISSED_FOREVER event if no dismiss button has been
 * supplied.
 */
export async function testDefaultDismissButtonEmitsEvent(done) {
  const htmlTemplate = `<educational-banner>
      <span slot="title">Banner title text</span>
      <span slot="subtitle">Banner subtitle text</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
    </educational-banner>
    `;
  document.body.innerHTML = htmlTemplate;
  educationalBanner = /** @type{!EducationalBanner} */ (
      document.body.querySelector('educational-banner'));

  const handler = () => {
    done();
  };
  educationalBanner.addEventListener(
      Banner.Event.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.shadowRoot.querySelector('#dismiss-button').click();
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  const mockVisitURL = mockUtilVisitURL();
  educationalBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), 'http://test.com');
  mockVisitURL.restoreVisitURL();
}

/**
 * Test that the default configuration is set on the warning banners to ensure
 * any overridden banners have sensible configuration.
 */
export function testEducationalBannerDefaults() {
  // Ensure the number of app sessions an educational banner is shown is 3.
  assertEquals(educationalBanner.showLimit(), 3);

  // Ensure the default allowed volume type is empty. This ensures any
  // banners that don't override this property do not show by default.
  assertEquals(educationalBanner.allowedVolumes().length, 0);
}

/**
 * Test that if extra button slot has attribute dismiss-banner-when-clicked the
 * banner emits a DISMISS_FOREVER event.
 */
export async function testDismissBannerWhenClickedAttributeWorks(done) {
  const htmlTemplate = `<educational-banner>
      <span slot="title">Banner title</span>
      <span slot="subtitle">Subtitle</span>
      <button slot="extra-button" href="http://test.com" dismiss-banner-when-clicked>
        Test Button
      </button>
    </educational-banner>
    `;
  document.body.innerHTML = htmlTemplate;
  educationalBanner = /** @type{!EducationalBanner} */ (
      document.body.querySelector('educational-banner'));
  const handler = (event) => {
    assertEquals(event.detail.banner.constructor, EducationalBanner);
    done();
  };
  educationalBanner.addEventListener(
      Banner.Event.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.querySelector('[slot="extra-button"]').click();
}

/**
 * Test that when the custom attribute to dismiss a banner emits a
 * DISMISS_FOREVER event, the banner instance attached is the correct instance.
 */
export async function testDismissWhenClickedAttributeWorksComponents(done) {
  const bannerTagName = 'test-educational-banner-dismiss-attribute';

  const htmlTemplate = html`<educational-banner>
    <button slot="extra-button" href="http://test.com" dismiss-banner-when-clicked>
      Test Button
    </button>
  </educational-banner>`;

  class TestEducationalBanner extends EducationalBanner {
    getTemplate() {
      return htmlTemplate.content.cloneNode(true);
    }
  }

  customElements.define(bannerTagName, TestEducationalBanner);

  document.body.innerHTML = `<${bannerTagName} />`;
  const banner = document.body.querySelector(bannerTagName);
  const handler = (event) => {
    assertEquals(event.detail.banner.constructor, TestEducationalBanner);
    done();
  };
  banner.addEventListener(Banner.Event.BANNER_DISMISSED_FOREVER, handler);

  const mockVisitURL = mockUtilVisitURL();
  banner.shadowRoot.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), 'http://test.com');
  mockVisitURL.restoreVisitURL();
}
