// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotReached} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../../common/js/test_error_reporting.js';

import {FilesPasswordDialog} from './files_password_dialog.js';

/** @type {!FilesPasswordDialog} */
let passwordDialog;
/** @type {!CrDialogElement} */
let dialog;
/** @type {!HTMLElement} */
let name;
/** @type {!CrInputElement} */
let input;
/** @type {!CrButtonElement} */
let cancel;
/** @type {!CrButtonElement} */
let unlock;

/**
 * Adds a FilesPasswordDialog element to the page.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML
  `<files-password-dialog id="test-files-password-dialog" hidden>
    </files-password-dialog>`;

  // Get the <files-password-dialog> element.
  passwordDialog = assert(/** @type {!FilesPasswordDialog} */ (
      document.querySelector('#test-files-password-dialog')));

  // Get its sub-elements.
  dialog = assert(/** @type {!CrDialogElement} */ (
      passwordDialog.shadowRoot.querySelector('#password-dialog')));
  name = assert(/** @type {!HTMLElement} */ (
      passwordDialog.shadowRoot.querySelector('#name')));
  input = assert(/** @type {!CrInputElement} */ (
      passwordDialog.shadowRoot.querySelector('#input')));
  cancel = assert(/** @type {!CrButtonElement} */ (
      passwordDialog.shadowRoot.querySelector('#cancel')));
  unlock = assert(/** @type {!CrButtonElement} */ (
      passwordDialog.shadowRoot.querySelector('#unlock')));
}

/**
 * Tests that the password dialog is modal.
 */
export async function testPasswordDialogIsModal(done) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open the password dialog.
  const passwordPromise = passwordDialog.askForPassword('encrypted.zip');

  // Wait until the dialog is open.
  await waitUntil(() => dialog.open);

  // Check that the name of the encrypted zip displays correctly on the dialog.
  assertEquals('encrypted.zip', name.innerText);

  // Enter password.
  input.value = 'password';

  // Keyboard events should not propagate out to the <files-password-dialog>
  // element. The internal <cr-dialog> element should handle keyboard events
  // and should prevent them from bubbling up to the <files-password-dialog>
  // element (modal operation).
  let isModal = true;
  passwordDialog.addEventListener('keydown', event => {
    console.error('<files-password-dialog> keydown event', event);
    isModal = false;
  });

  // Add a <cr-dialog> listener to confirm it saw the test keyboard event.
  let keydownSeen = false;
  dialog.addEventListener('keydown', event => {
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
  const inputElement = input.shadowRoot.querySelector('input');
  assertEquals(inputElement.value, 'password');
  assert(inputElement.dispatchEvent(keyEvent));

  // Check: the <files-password-dialog> should be modal.
  await waitUntil(() => keydownSeen);
  assert(isModal, 'FAILED: <files-password-dialog> should be modal');

  done();
}

/**
 * Tests cancel functionality of password dialog for single encrypted archive.
 * The askForPassword method should return a promise that is rejected with
 * FilesPasswordDialog.USER_CANCELLED.
 */
export async function testSingleArchiveCancelPasswordPrompt(done) {
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
  // FilesPasswordDialog.USER_CANCELLED.
  try {
    await passwordPromise;
    assertNotReached();
  } catch (e) {
    assertEquals(FilesPasswordDialog.USER_CANCELLED, e);
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
export async function testUnlockSingleArchive(done) {
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
export async function testDialogWithWrongPassword(done) {
  // Check that the dialog is closed.
  assertFalse(dialog.open);

  // Open password prompt.
  const passwordPromise =
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
export async function testCancelMultiplePasswordPrompts(done) {
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
  // FilesPasswordDialog.USER_CANCELLED.
  try {
    await passwordPromise;
    assertNotReached();
  } catch (e) {
    assertEquals(FilesPasswordDialog.USER_CANCELLED, e);
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
  // FilesPasswordDialog.USER_CANCELLED.
  try {
    await passwordPromise2;
    assertNotReached();
  } catch (e) {
    assertEquals(FilesPasswordDialog.USER_CANCELLED, e);
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
export async function testUnlockMultipleArchives(done) {
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
