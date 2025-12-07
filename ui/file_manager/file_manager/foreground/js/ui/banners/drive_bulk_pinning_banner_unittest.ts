// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../../../../widgets/xf_bulk_pinning_dialog.js';
import './drive_bulk_pinning_banner.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../../../../common/js/unittest_util.js';
import type {XfBulkPinningDialog} from '../../../../widgets/xf_bulk_pinning_dialog.js';

import type {DriveBulkPinningBanner} from './drive_bulk_pinning_banner.js';
import type {EducationalBanner} from './educational_banner.js';

export function setUp() {
  console.info('Setting up drive-bulk-pinning-banner element');
  document.body.innerHTML = getTrustedHTML
  `<xf-bulk-pinning-dialog></xf-bulk-pinning-dialog>
      <drive-bulk-pinning-banner></drive-bulk-pinning-banner>`;
}

// Gets the <drive-bulk-pinning-banner> element.
async function getBanner(): Promise<DriveBulkPinningBanner> {
  const banner = document.body.querySelector<DriveBulkPinningBanner>(
      'drive-bulk-pinning-banner')!;
  assertNotEquals(null, banner, 'banner should not be null');
  assertEquals('DRIVE-BULK-PINNING-BANNER', banner.tagName);
  await waitForElementUpdate(banner);
  return banner;
}

// Gets the <xf-bulk-pinning-dialog> element.
async function getDialog(): Promise<XfBulkPinningDialog> {
  const dialog =
      document.querySelector<XfBulkPinningDialog>('xf-bulk-pinning-dialog')!;
  assertNotEquals(null, dialog);
  assertEquals('XF-BULK-PINNING-DIALOG', dialog.tagName);
  await waitForElementUpdate(dialog);
  return dialog;
}

// Test that the "Get started" button can be clicked, and that clicking it opens
// the bulk-pinning activation dialog.
export async function testClick() {
  const banner = await getBanner();
  const button =
      banner.shadowRoot!.querySelector<EducationalBanner>('educational-banner')!
          .querySelector<CrButtonElement>('[slot="extra-button"]')!;
  assertFalse(button.disabled, 'button should not be disabled');

  const dialog = await getDialog();
  assertFalse(dialog.is_open);

  button.click();
  assertTrue(dialog.is_open);
}
