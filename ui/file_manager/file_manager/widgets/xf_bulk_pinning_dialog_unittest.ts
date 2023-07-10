// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

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
function getDialog(): XfBulkPinningDialog {
  const dialog = document.querySelector('xf-bulk-pinning-dialog');
  assertNotEquals(null, dialog, 'xf-bulk-pinning-dialog is null');
  assertEquals('XF-BULK-PINNING-DIALOG', dialog!.tagName);
  return dialog! as XfBulkPinningDialog;
}

/**
 * Tests that the initial XfBulkPinningDialog exists.
 */
export async function testInit(done: () => void) {
  const dialog = getDialog();
  assertNotEquals(dialog, null);
  done();
}
