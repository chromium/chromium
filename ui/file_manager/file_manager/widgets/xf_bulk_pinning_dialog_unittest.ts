// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfBulkPinningDialog} from './xf_bulk_pinning_dialog.js';

export function setUp() {
  document.body.innerHTML = '<xf-bulk-pinning-dialog></xf-bulk-pinning-dialog>';
}

/** Gets the <xf-bulk-pinning-dialog> element. */
async function getDialog(): Promise<XfBulkPinningDialog> {
  const dialog =
      document.querySelector<XfBulkPinningDialog>('xf-bulk-pinning-dialog');
  assertNotEquals(null, dialog);
  assertEquals('XF-BULK-PINNING-DIALOG', dialog!.tagName);
  await waitForElementUpdate(dialog!);
  return dialog!;
}


/** Gets a footer of the given dialog. */
function getFooter(dialog: XfBulkPinningDialog, id: string): HTMLDivElement {
  return dialog.shadowRoot!.querySelector(`#${id}`)!;
}

/** Gets the "Continue" button of the given dialog. */
function getButton(dialog: XfBulkPinningDialog): HTMLButtonElement {
  return dialog.shadowRoot!.querySelector('#continue-button')!;
}

/** Tests that the initial XfBulkPinningDialog exists. */
export async function testInit(done: () => void) {
  const dialog = await getDialog();
  assertNotEquals(null, dialog);
  dialog.show();

  // At first, listing-footer should be displayed.
  assertEquals('none', getFooter(dialog, 'offline-footer').style.display, '1');
  assertEquals(
      'none', getFooter(dialog, 'battery-saver-footer').style.display, '2');
  assertEquals('flex', getFooter(dialog, 'listing-footer').style.display, '3');
  assertEquals('none', getFooter(dialog, 'error-footer').style.display, '4');
  assertEquals(
      'none', getFooter(dialog, 'not-enough-space-footer').style.display, '5');
  assertEquals('none', getFooter(dialog, 'ready-footer').style.display, '6');
  assertTrue(getButton(dialog).disabled);

  // But eventually, listing-footer gets replaced.
  while (getFooter(dialog, 'listing-footer').style.display !== 'none') {
    console.log('Waiting for listing-footer to disappear...');
    await waitForElementUpdate(dialog);
  }

  // The error-footer should now be visible.
  assertEquals('none', getFooter(dialog, 'offline-footer').style.display, '1');
  assertEquals(
      'none', getFooter(dialog, 'battery-saver-footer').style.display, '2');
  assertEquals('none', getFooter(dialog, 'listing-footer').style.display, '3');
  assertEquals('initial', getFooter(dialog, 'error-footer').style.display, '4');
  assertEquals(
      'none', getFooter(dialog, 'not-enough-space-footer').style.display, '5');
  assertEquals('none', getFooter(dialog, 'ready-footer').style.display, '6');
  assertTrue(getButton(dialog).disabled);

  done();
}
