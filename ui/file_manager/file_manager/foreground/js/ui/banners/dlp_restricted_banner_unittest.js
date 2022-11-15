// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';

import {DlpRestrictedBanner} from './dlp_restricted_banner.js';

/** @type{!DlpRestrictedBanner} */
let dlpRestrictedBanner;

export function setUp() {
  const htmlTemplate = `<dlp-restricted-banner>
          <span slot="text"></span>
          </dlp-restricted-banner>`;
  document.body.innerHTML = htmlTemplate;
  dlpRestrictedBanner = /** @type{!DlpRestrictedBanner} */ (
      document.body.querySelector('dlp-restricted-banner'));
}

/**
 * Test that the banner is shown in all root types.
 */
export function testAllowedVolumes() {
  assertEquals(
      dlpRestrictedBanner.allowedVolumes().length,
      Object.values(VolumeManagerCommon.RootType).length);
}
