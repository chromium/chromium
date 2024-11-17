// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './warning_banner.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {getLastVisitedURL} from '../../../../common/js/util.js';

import {BannerEvent} from './types.js';
import type {WarningBanner} from './warning_banner.js';

let warningBanner: WarningBanner;

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <warning-banner>
      <span slot="text">Banner text</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
      <button slot="dismiss-button" id="dismiss-button">
        Dismiss
      </button>
    </warning-banner>
  `;
  warningBanner = document.body.querySelector<WarningBanner>('warning-banner')!;
}

/**
 * Test that the dismiss handler bubbles the correct event on click.
 */
export async function testDismissHandlerEmitsEvent(done: () => void) {
  const handler = () => {
    done();
  };
  warningBanner.addEventListener(BannerEvent.BANNER_DISMISSED, handler);
  warningBanner.querySelector<CrButtonElement>(
                   '[slot="dismiss-button"]')!.click();
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  warningBanner.addEventListener(
      'click', () => console.info('additional event listner'));
  warningBanner.querySelector<CrButtonElement>(
                   '[slot="extra-button"]')!.click();
  assertEquals(getLastVisitedURL(), 'http://test.com');
}

/**
 * Test that the default configuration is set on the warning banners to ensure
 * any overridden banners have sensible configuration.
 */
export function testWarningBannerDefaults() {
  // Ensure the warning banner default timeout is 36 hours.
  assertEquals(warningBanner.hideAfterDismissedDurationSeconds(), 36 * 60 * 60);

  // Ensure the default allowed volume type is empty. This ensures any
  // banners that don't override this property do not show by default.
  assertEquals(warningBanner.allowedVolumes().length, 0);
}
