// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://test/chai_assert.js';
import {mockUtilVisitURL} from '../../../../common/js/mock_util.js';

import {StateBanner} from './state_banner.js';

/** @type{!StateBanner} */
let stateBanner;

export function setUp() {
  const html = `<state-banner>
      <span slot="text">Banner title</span>
      <button slot="extra-button" href="http://test.com">
        Test Banner
      </button>
    </state-banner>
    `;
  document.body.innerHTML = html;
  stateBanner =
      /** @type{!StateBanner} */ (document.body.querySelector('state-banner'));
}

/**
 * Test that the additional button can be set and the link is visited when the
 * button is clicked.
 */
export async function testAdditionalButtonCanBeClicked() {
  const mockVisitURL = mockUtilVisitURL();
  stateBanner.querySelector('[slot="extra-button"]').click();
  assertEquals(mockVisitURL.getURL(), 'http://test.com');
  mockVisitURL.restoreVisitURL();
}

/**
 * Test that the default configuration is set on the state banners to ensure
 * any overridden banners have sensible configuration.
 */
export function testStateBannerDefaults() {
  // Ensure the default allowed volume type is empty. This ensures any
  // banners that don't override this property do not show by default.
  assertEquals(stateBanner.allowedVolumes().length, 0);
}
