// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {mockUtilVisitURL} from '../../../../common/js/mock_util.js';
import {Banner} from '../../../../externs/banner.js';

import {WarningBanner} from './warning_banner.js';

/** @type{!WarningBanner} */
let warningBanner;

export function setUp() {
  const html = `<warning-banner>
      <span slot="text">Banner text</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
      <button slot="dismiss-button" id="dismiss-button">
        Dismiss
      </button>
    </warning-banner>
    `;
  document.body.innerHTML = html;
  warningBanner = /** @type{!WarningBanner} */ (
      document.body.querySelector('warning-banner'));
}

/**
 * Test that the dismiss handler bubbles the correct event on click.
 */
export async function testDismissHandlerEmitsEvent(done) {
  const handler = () => {
    done();
  };
  warningBanner.addEventListener(Banner.Event.BANNER_DISMISSED, handler);
  warningBanner.querySelector('[slot="dismiss-button"]').click();
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  const mockVisitURL = mockUtilVisitURL();
  warningBanner.addEventListener(
      'click', () => console.log('additional event listner'));
  warningBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), 'http://test.com');
  mockVisitURL.restoreVisitURL();
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
