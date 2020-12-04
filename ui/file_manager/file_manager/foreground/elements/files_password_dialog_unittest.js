// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * TODO(lucmult): Remove this when converting to JS modules.
 * @suppress {checkTypes}
 */
chrome.fileManagerPrivate = {
  FormatFileSystemType: {
    VFAT: 'vfat',
    EXFAT: 'exfat',
    NTFS: 'ntfs',
  },
};

/**
 * Mock LoadTimeData strings.
 */
function setUpPage() {
  window.loadTimeData.getString = id => {
    switch (id) {
      case 'PASSWORD_DIALOG_INVALID':
        return 'Invalid password';
      default:
        return id;
    }
  };
}

/**
 * Adds a FilesPasswordDialog element to the page.
 */
function setUp() {
  document.body.innerHTML =
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
 * Tests cancel functionality of password dialog for single encrypted archive.
 * The askForPassword method should return a promise that is rejected with
 * FilesPasswordDialog.USER_CANCELLED.
 */
async function testSingleArchiveCancelPasswordPrompt(done) {
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
async function testUnlockSingleArchive(done) {
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
async function testDialogWithWrongPassword(done) {
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
async function testCancelMultiplePasswordPrompts(done) {
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
async function testUnlockMultipleArchives(done) {
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
