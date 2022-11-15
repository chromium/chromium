// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import {ConflictResolveType, XfConflictDialog} from './xf_conflict_dialog.js';

/**
 * Creates new <xf-conflict-dialog> for each test.
 */
export function setUp() {
  document.body.innerHTML = '<xf-conflict-dialog></xf-conflict-dialog>';
}

/**
 * Returns the <xf-conflict-dialog> element.
 */
function getConflictDialogElement(): XfConflictDialog {
  const element = document.querySelector('xf-conflict-dialog');
  assertNotEquals(null, element);
  assertEquals('XF-CONFLICT-DIALOG', element!.tagName);
  return element! as XfConflictDialog;
}

/*
 * Tests that the dialog opens with no 'Apply to all' checkbox shown.
 */
export async function testDialogShow(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name.
  element.show('file.txt');
  await waitUntil(() => dialog.open);

  // Check: the dialog message should contain 'file.txt'.
  const message = element.getMessageElement();
  assertNotEquals('none', window.getComputedStyle(message).display);
  assertTrue(message.innerText.includes('file.txt'));
  assertFalse(message.hidden);

  // Check: the 'Apply to all' checkbox should not be shown.
  const checkbox = element.getCheckboxElement();
  assertEquals('none', window.getComputedStyle(checkbox).display);
  assertFalse(checkbox.checked);
  assertTrue(checkbox.hidden);

  // Check: the dialog should have the focus.
  assertNotEquals('none', window.getComputedStyle(dialog).display);
  await waitUntil(() => element.shadowRoot!.activeElement === dialog);

  done();
}

/*
 * Tests that the dialog can open with the 'Apply to all' checkbox shown.
 */
export async function testDialogShowCheckbox(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name, with a checkbox.
  const withCheckbox = true;
  element.show('image.jpg', withCheckbox);
  await waitUntil(() => dialog.open);

  // Check: the dialog message should contain 'image.jpg'.
  const message = element.getMessageElement();
  assertNotEquals('none', window.getComputedStyle(message).display);
  assertTrue(message.innerText.includes('image.jpg'));
  assertFalse(message.hidden);

  // Check: the 'Apply to all' checkbox should be shown.
  const checkbox = element.getCheckboxElement();
  assertNotEquals('none', window.getComputedStyle(checkbox).display);
  assertFalse(checkbox.hasAttribute('disabled'));
  assertFalse(checkbox.checked);
  assertFalse(checkbox.hidden);

  // Check: the dialog should have the focus.
  assertNotEquals('none', window.getComputedStyle(dialog).display);
  await waitUntil(() => element.shadowRoot!.activeElement === dialog);

  done();
}

/*
 * Tests that the dialog consumes keyboard events.
 */
export async function testDialogConsumesKeyboardEvents(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name.
  element.show('file.txt');
  await waitUntil(() => dialog.open);

  // Keyboard events should not bubble up to the <xf-conflict-dialog>. The
  // <cr-dialog> should consume them (modal behavior).
  assertTrue(dialog.consumeKeydownEvent);
  let consumedKeyEvent = true;
  element.addEventListener('keydown', () => {
    console.error('FAILED: <xf-conflict-dialog> keydown event');
    consumedKeyEvent = false;
  });

  // Create a <ctrl>-A keyboard event. The event has 'composed' true so it
  // can cross shadow DOM boundaries and bubble up into the DOM.
  const keyboardEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    composed: true,
    ctrlKey: true,
    key: 'A',
  });

  // Add <cr-dialog> listener to confirm it saw the keyboard event.
  let keydownSeen = false;
  dialog.addEventListener('keydown', () => {
    keydownSeen = true;
  });

  // Check: the <cr-dialog> should consume keyboard events.
  assertTrue(dialog.dispatchEvent(keyboardEvent));
  await waitUntil(() => keydownSeen);
  assertTrue(consumedKeyEvent);

  done();
}

/*
 * Tests that cancel button closes the dialog with a cancelled error.
 */
export async function testDialogCancelButton(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name.
  const resultPromise = element.show('file.txt');
  await waitUntil(() => dialog.open);

  // Check: the cancel button should be shown.
  const cancel = element.getCancelButton();
  assertNotEquals('none', window.getComputedStyle(cancel).display);
  assertFalse(cancel.hasAttribute('disabled'));
  assertFalse(cancel.hidden);

  // Clicking the cancel button should close the dialog.
  cancel.click();
  await waitUntil(() => !dialog.open);

  // And reject resultPromise with a dialog cancelled Error.
  try {
    await resultPromise;
    assertNotReached();
  } catch (error: any) {
    assertEquals('Error: dialog cancelled', error?.toString());
  }

  done();
}

/*
 * Tests that replace button closes the dialog with a 'replace' result.
 */
export async function testDialogReplaceButton(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name.
  const resultPromise = element.show('file1.txt');
  await waitUntil(() => dialog.open);

  // Check: the replace button should be shown.
  const replace = element.getReplaceButton();
  assertNotEquals('none', window.getComputedStyle(replace).display);
  assertFalse(replace.hasAttribute('disabled'));
  assertFalse(replace.hidden);

  // Clicking the replace button should close the dialog.
  replace.click();
  await waitUntil(() => !dialog.open);

  // And resolve the resultPromise with a 'replace' result.
  try {
    const result = await resultPromise;
    assertEquals(ConflictResolveType.REPLACE, result.resolve);
    assertEquals(false, result.checked);
  } catch (error) {
    console.error('FAILED: <xf-conflict-dialog>', error);
    assertNotReached();
  }

  // Open the conflict dialog for a given file name, with the checkbox.
  const resultCheckboxPromise = element.show('file2.txt', true);
  await waitUntil(() => dialog.open);

  // Check: the 'Apply to all' checkbox should be shown.
  const checkbox = element.getCheckboxElement();
  assertNotEquals('none', window.getComputedStyle(checkbox).display);
  assertFalse(checkbox.hasAttribute('disabled'));
  assertFalse(checkbox.checked);
  assertFalse(checkbox.hidden);

  // Clicking the checkbox should toggle its checked state.
  checkbox.click();
  await waitUntil(() => checkbox.checked);

  // Check: the replace button should be shown.
  assertNotEquals('none', window.getComputedStyle(replace).display);
  assertFalse(replace.hasAttribute('disabled'));
  assertFalse(replace.hidden);

  // Clicking the replace button should close the dialog.
  replace.click();
  await waitUntil(() => !dialog.open);

  // And resolve the resultCheckboxPromise with a 'replace' result.
  try {
    const result = await resultCheckboxPromise;
    assertEquals(ConflictResolveType.REPLACE, result.resolve);
    assertEquals(true, result.checked);
  } catch (error) {
    console.error('FAILED: <xf-conflict-dialog>', error);
    assertNotReached();
  }

  done();
}

/*
 * Tests that keepboth button closes the dialog with a 'keepboth' result.
 */
export async function testDialogKeepbothButton(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Open the conflict dialog for a given file name.
  const resultPromise = element.show('file1.txt');
  await waitUntil(() => dialog.open);

  // Check: the keepboth button should be shown.
  const keepboth = element.getKeepbothButton();
  assertNotEquals('none', window.getComputedStyle(keepboth).display);
  assertFalse(keepboth.hasAttribute('disabled'));
  assertFalse(keepboth.hidden);

  // Clicking the keepboth button should close the dialog.
  keepboth.click();
  await waitUntil(() => !dialog.open);

  // And resolve the resultPromise with a 'keepboth' result.
  try {
    const result = await resultPromise;
    assertEquals(ConflictResolveType.KEEPBOTH, result.resolve);
    assertEquals(false, result.checked);
  } catch (error) {
    console.error('FAILED: <xf-conflict-dialog>', error);
    assertNotReached();
  }

  // Open the conflict dialog for a given file name, with the checkbox.
  const resultCheckboxPromise = element.show('file2.txt', true);
  await waitUntil(() => dialog.open);

  // Check: the 'Apply to all' checkbox should be shown.
  const checkbox = element.getCheckboxElement();
  assertNotEquals('none', window.getComputedStyle(checkbox).display);
  assertFalse(checkbox.hasAttribute('disabled'));
  assertFalse(checkbox.checked);
  assertFalse(checkbox.hidden);

  // Clicking the checkbox should toggle its checked state.
  checkbox.click();
  await waitUntil(() => checkbox.checked);

  // Check: the keepboth button should be shown.
  assertNotEquals('none', window.getComputedStyle(keepboth).display);
  assertFalse(keepboth.hasAttribute('disabled'));
  assertFalse(keepboth.hidden);

  // Clicking the keepboth button should close the dialog.
  keepboth.click();
  await waitUntil(() => !dialog.open);

  // And resolve the resultCheckboxPromise with a 'keepboth' result.
  try {
    const result = await resultCheckboxPromise;
    assertEquals(ConflictResolveType.KEEPBOTH, result.resolve);
    assertEquals(true, result.checked);
  } catch (error) {
    console.error('FAILED <xf-conflict-dialog>', error);
    assertNotReached();
  }

  done();
}

/**
 * Tests that requests to open the dialog are serialized.
 */
export async function testDialogShowRequestsSerialize(done: () => void) {
  const element = getConflictDialogElement();

  // Check: the dialog should not be open.
  const dialog = element.getDialogElement();
  assertFalse(dialog.open);

  // Send multiple show requests to the <xf-conflict-dialog>.
  const resultPromise1 = element.show('file1.txt');
  const resultPromise2 = element.show('file2.txt');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check: the dialog message should be for 'file1.txt'.
  const message = element.getMessageElement();
  await waitUntil(() => message.innerText.includes('file1.txt'));

  // Click the keepboth button.
  const keepboth = element.getKeepbothButton();
  keepboth.click();

  // Check: resultPromise1 should resolve with a 'keepboth' result.
  try {
    const result = await resultPromise1;
    assertEquals(ConflictResolveType.KEEPBOTH, result.resolve);
    assertEquals(false, result.checked);
  } catch (error) {
    console.error('FAILED: <xf-conflict-dialog>', error);
    assertNotReached();
  }

  // Wait until the dialog is re-opened for 'file2.txt'.
  await waitUntil(() => dialog.open);
  await waitUntil(() => message.innerText.includes('file2.txt'));

  // Click the replace button.
  const replace = element.getReplaceButton();
  replace.click();

  // Check: resultPromise2 should resolve with a 'replace' result.
  try {
    const result = await resultPromise2;
    assertEquals(ConflictResolveType.REPLACE, result.resolve);
    assertEquals(false, result.checked);
  } catch (error) {
    console.error('FAILED: <xf-conflict-dialog>', error);
    assertNotReached();
  }

  done();
}
