// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfBulkPinningDialog} from './xf_bulk_pinning_dialog.js';

/**
 * Creates new <xf-bulk-pinning-dialog> for each test.
 */
export function setUp() {
  document.body.innerHTML = '<xf-bulk-pinning-dialog></xf-bulk-pinning-dialog>';
}

/**
 * Returns the <xf-bulk-pinning-dialog> element.
 */
async function getDialog(): Promise<XfBulkPinningDialog> {
  const dialog =
      document.querySelector<XfBulkPinningDialog>('xf-bulk-pinning-dialog');
  assertNotEquals(null, dialog);
  assertEquals('XF-BULK-PINNING-DIALOG', dialog!.tagName);
  await waitForElementUpdate(dialog!);
  return dialog!;
}

/**
 * Tests that the initial XfBulkPinningDialog exists.
 */
export async function testInit(done: () => void) {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  dialog.show();
  await waitForElementUpdate(dialog);
  done();
}
