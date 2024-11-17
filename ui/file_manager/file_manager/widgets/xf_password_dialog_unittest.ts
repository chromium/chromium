// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './xf_password_dialog.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotReached} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import type {XfPasswordDialog} from './xf_password_dialog.js';
import {USER_CANCELLED} from './xf_password_dialog.js';

let passwordDialog: XfPasswordDialog;
let dialog: CrDialogElement;
let name: HTMLElement;
let input: CrInputElement;
let cancel: CrButtonElement;
let unlock: CrButtonElement;

/**
 * Adds a XfPasswordDialog element to the page.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML
  `<xf-password-dialog hidden>
    </xf-password-dialog>`;

  // Get the <xf-password-dialog> element.
  passwordDialog = document.querySelector('xf-password-dialog')!;

  // Get its sub-elements.
  dialog = passwordDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#password-dialog')!;
  name = passwordDialog.shadowRoot!.querySelector('#name')!;
  input = passwordDialog.shadowRoot!.querySelector('#input')!;
  cancel = passwordDialog.shadowRoot!.querySelector('#cancel')!;
  unlock = passwordDialog.shadowRoot!.querySelector('#unlock')!;
}

/**
 * Tests that the password dialog is modal.
 */
export async function testPasswordDialogIsModal(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open the password dialog.
  passwordDialog.askForPassword('encrypted.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter password.
  input.value = 'password';

  // Keyboard events should not propagate out to the <xf-password-dialog>
  // element. The internal <cr-dialog> element should handle keyboard events
  // and should prevent them from bubbling up to the <xf-password-dialog>
  // element (modal operation).
  let isModal = true;
  passwordDialog.addEventListener('keydown', event => {
    console.error('<xf-password-dialog> keydown event', event);
    isModal = false;
  });

  // Add a <cr-dialog> listener to confirm it saw the test keyboard event.
  let keydownSeen = false;
  dialog.addEventListener('keydown', () => {
    keydownSeen = true;
  });

  // Create a <ctrl>-A keyboard event. The event has 'composed' true, so it
  // can cross shadow DOM bondaries and bubble up into the DOM document.
  const keyEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    composed: true,
    ctrlKey: true,
    key: 'A',
  });

  // Dispatch the keyboard event to the <cr-input> inner <input> element.
  const inputElement =
      input.shadowRoot!.querySelector<HTMLInputElement>('input')!;
  assertEquals(inputElement.value, 'password');
  assert(inputElement.dispatchEvent(keyEvent));

  // Check: the <xf-password-dialog> should be modal.
  await waitUntil(() => keydownSeen);
  assert(isModal, 'FAILED: <xf-password-dialog> should be modal');

  done();
}

/**
 * Tests cancel functionality of password dialog for single encrypted archive.
 * The askForPassword method should return a promise that is rejected with
 * USER_CANCELLED.
 */
export async function testSingleArchiveCancelPasswordPrompt(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open password prompt.
  const passwordPromise = passwordDialog.askForPassword('encrypted.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter password.
  input.value = 'password';

  // Click the cancel button closes the dialog.
  cancel.click();

  // The dialog should be hidden.
  await waitUntil(() => !dialog.open);

  // The passwordPromise should be rejected with
  // USER_CANCELLED.
  try {
    await passwordPromise;
    assertNotReached();
  } catch (e) {
    assertEquals(USER_CANCELLED, e);
  }

  // The password input field should be cleared.
  assertEquals('', input.value);

  // Check that the dialog is closed.
  assertFalse(dialog.open);

  done();
}

/**
 * Tests unlock functionality for single encrypted archive. The askForPassword
 * method should return a promise that resolves with the expected input
 * password.
 */
export async function testUnlockSingleArchive(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open password prompt.
  const passwordPromise = passwordDialog.askForPassword('encrypted.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter password.
  input.value = 'password';

  // Click the unlock button.
  unlock.click();

  // Wait until the second password promise is resolved with the password value
  // found in the input field.
  assertEquals('password', await passwordPromise);

  // The password input field should be cleared.
  assertEquals('', input.value);

  // Check that the dialog is closed.
  assertFalse(dialog.open);

  done();
}

/**
 * Tests opening the password dialog with an 'Invalid password' message. This
 * message is displayed when a wrong password was previously entered.
 */
export async function testDialogWithWrongPassword(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open password prompt.
  passwordDialog.askForPassword('encrypted.zip', 'wrongpassword');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Check that the previous erroneous password is prefilled in the dialog.
  assertEquals('wrongpassword', input.value);

  // Check that the previous erroneous password is prefilled in the dialog.
  assertEquals('Invalid password', input.errorMessage);

  done();
}

/**
 * Tests cancel functionality for multiple encrypted archives.
 */
export async function testCancelMultiplePasswordPrompts(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Simulate password prompt for multiple archives.
  const passwordPromise = passwordDialog.askForPassword('encrypted.zip');
  const passwordPromise2 = passwordDialog.askForPassword('encrypted2.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter password.
  input.value = 'password';

  // Click the cancel button closes the dialog.
  cancel.click();

  // The passwordPromise should be rejected with
  // USER_CANCELLED.
  try {
    await passwordPromise;
    assertNotReached();
  } catch (e) {
    assertEquals(USER_CANCELLED, e);
  }

  // The password input field should be cleared.
  assertEquals('', input.value);

  // Wait for password prompt dialog for second archive.
  await waitUntil(() => name.innerText === 'encrypted2.zip');

  // Enter password.
  input.value = 'password';

  // Click the cancel button closes the dialog.
  cancel.click();

  // The passwordPromise should be rejected with
  // USER_CANCELLED.
  try {
    await passwordPromise2;
    assertNotReached();
  } catch (e) {
    assertEquals(USER_CANCELLED, e);
  }

  // The password input field should be cleared.
  assertEquals('', input.value);

  // Check that the dialog is closed.
  assertFalse(dialog.open);

  done();
}

/**
 * Tests unlock functionality for multiple encrypted archives.
 */
export async function testUnlockMultipleArchives(done: () => void) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Simulate password prompt for multiple archives.
  const passwordPromise = passwordDialog.askForPassword('encrypted.zip');
  const passwordPromise2 = passwordDialog.askForPassword('encrypted2.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter second password.
  input.value = 'password';

  // Click the unlock button.
  unlock.click();

  // Wait until the second password promise is resolved with the password value
  // found in the input field.
  assertEquals('password', await passwordPromise);

  // Wait until the dialog is open for the second password.
  assertEquals('', input.value);

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted2.zip', name.innerText);

  // Enter third password.
  input.value = 'password2';

  // Click the unlock button.
  unlock.click();

  // Wait until the second password promise resolves with the password value
  // found in the input field.
  assertEquals('password2', await passwordPromise2);

  // The password input field should be cleared.
  assertEquals('', input.value);

  // Check that the dialog is closed.
  assertFalse(dialog.open);

  done();
}
