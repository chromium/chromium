// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './dlp_restricted_banner.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {RootType} from '../../../../common/js/volume_manager_types.js';

import type {DlpRestrictedBanner} from './dlp_restricted_banner.js';

let dlpRestrictedBanner: DlpRestrictedBanner;

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
  <dlp-restricted-banner>
    <span slot="text"></span>
  </dlp-restricted-banner>
  `;
  dlpRestrictedBanner = document.body.querySelector<DlpRestrictedBanner>(
      'dlp-restricted-banner')!;
}

/**
 * Test that the banner is shown in all root types.
 */
export function testAllowedVolumes() {
  assertEquals(
      dlpRestrictedBanner.allowedVolumes().length,
      Object.values(RootType).length);
}
