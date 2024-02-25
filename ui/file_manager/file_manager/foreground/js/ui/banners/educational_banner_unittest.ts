// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {isCrosComponentsEnabled} from '../../../../common/js/flags.js';
import {getLastVisitedURL} from '../../../../common/js/util.js';

import {EducationalBanner} from './educational_banner.js';
import type {Banner} from './types.js';
import {BannerEvent} from './types.js';


let educationalBanner: EducationalBanner;

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <educational-banner>
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
  educationalBanner =
      document.body.querySelector<EducationalBanner>('educational-banner')!;
}

/**
 * Test that the dismiss handler bubbles the correct event on click.
 */
export async function testDismissHandlerEmitsEvent(done: () => void) {
  const handler = () => {
    done();
  };
  educationalBanner.addEventListener(
      BannerEvent.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.querySelector<CrButtonElement>(
                       '[slot="dismiss-button"]')!.click();
}

/**
 * Test that the default dismiss button from the educational banner successfully
 * emits the BANNER_DISMISSED_FOREVER event if no dismiss button has been
 * supplied.
 */
export async function testDefaultDismissButtonEmitsEvent(done: () => void) {
  document.body.innerHTML = getTrustedHTML
  `<educational-banner>
      <span slot="title">Banner title text</span>
      <span slot="subtitle">Banner subtitle text</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
    </educational-banner>
    `;
  educationalBanner =
      document.body.querySelector<EducationalBanner>('educational-banner')!;

  const handler = () => {
    done();
  };
  educationalBanner.addEventListener(
      BannerEvent.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.shadowRoot!
      .querySelector<HTMLElement>(
          isCrosComponentsEnabled() ? '#dismiss-button' :
                                      '#dismiss-button-old')!.click();
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  educationalBanner.querySelector<CrButtonElement>(
                       '[slot="extra-button"]')!.click();
  assertEquals(getLastVisitedURL(), 'http://test.com');
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
export async function testDismissBannerWhenClickedAttributeWorks(
    done: () => void) {
  document.body.innerHTML = getTrustedHTML`
    <educational-banner>
      <span slot="title">Banner title</span>
      <span slot="subtitle">Subtitle</span>
      <button slot="extra-button" href="http://test.com" dismiss-banner-when-clicked>
        Test Button
      </button>
    </educational-banner>
    `;
  educationalBanner =
      document.body.querySelector<EducationalBanner>('educational-banner')!;
  const handler = (event: BannerDismissedEvent) => {
    assertEquals(event.detail.banner.constructor, EducationalBanner);
    done();
  };
  educationalBanner.addEventListener(
      BannerEvent.BANNER_DISMISSED_FOREVER, handler);
  educationalBanner.querySelector<CrButtonElement>(
                       '[slot="extra-button"]')!.click();
}

/**
 * Test that when the custom attribute to dismiss a banner emits a
 * DISMISS_FOREVER event, the banner instance attached is the correct instance.
 */
export async function testDismissWhenClickedAttributeWorksComponents(
    done: () => void) {
  const bannerTagName = 'test-educational-banner-dismiss-attribute';

  const htmlTemplate = document.createElement('template');
  htmlTemplate.innerHTML = getTrustedHTML`<educational-banner>
  <button slot="extra-button" href="http://test.com" dismiss-banner-when-clicked>
    Test Button
  </button>
</educational-banner>`;

  class TestEducationalBanner extends EducationalBanner {
    override getTemplate() {
      return htmlTemplate.content.cloneNode(true);
    }
  }

  customElements.define(bannerTagName, TestEducationalBanner);

  document.body.innerHTML =
      getTrustedHTML`<test-educational-banner-dismiss-attribute />`;
  const banner = document.body.querySelector<Banner>(bannerTagName)!;
  const handler = (event: BannerDismissedEvent) => {
    assertEquals(event.detail.banner.constructor, TestEducationalBanner);
    done();
  };
  banner.addEventListener(BannerEvent.BANNER_DISMISSED_FOREVER, handler);

  banner.shadowRoot!.querySelector<CrButtonElement>(
                        '[slot="extra-button"]')!.click();
  assertEquals(getLastVisitedURL(), 'http://test.com');
}
