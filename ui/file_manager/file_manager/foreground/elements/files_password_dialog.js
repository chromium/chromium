// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AsyncQueue} from '../../common/js/async_util.js';

/** @type {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * Dialog to request user to enter password. Uses the askForPassword() which
 * resolves with either the password or rejected with
 * FilesPasswordDialog.USER_CANCELLED.
 * @extends HTMLElement
 */
export class FilesPasswordDialog extends HTMLElement {
  constructor() {
    super();

    // Create element content.
    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    /**
     * Mutex used to serialize modal dialogs and error notifications.
     * @private {?AsyncQueue}
     */
    this.mutex_ = null;

    /**
     * Name of the encrypted file.
     * @private {string}
     */
    this.filename_ = '';

    /**
     * Controls whether the user is validating the password (Unlock button or
     * Enter key) or cancelling the dialog (Cancel button or Escape key).
     * @private {boolean}
     */
    this.success_ = false;

    /**
     * Return input password using the resolve method of a Promise.
     * @private {?function(string)}
     */
    this.resolve_ = null;

    /**
     * Return password prompt error using the reject method of a Promise.
     * @private {?function(!Error)}
     */
    this.reject_ = null;

    /**
     * Password dialog.
     * TODO(https://crbug.com/1353205): This type should be CrDialogElement, and
     * an import of that type from cr_dialog.js should be added to this file.
     * @private {!HTMLElement}
     */
    this.dialog_ = /** @type {!HTMLElement} */
        (this.shadowRoot.querySelector('#password-dialog'));
    this.dialog_.consumeKeydownEvent = true;

    /**
     * Input field for password.
     * @private {!CrInputElement}
     */
    this.input_ = /** @type {!CrInputElement} */
        (this.shadowRoot.querySelector('#input'));
    this.input_.errorMessage =
        loadTimeData.getString('PASSWORD_DIALOG_INVALID');
  }

  get mutex() {
    if (!this.mutex_) {
      this.mutex_ = new AsyncQueue();
    }
    return this.mutex_;
  }

  /**
   * Called when this element is attached to the DOM.
   *
   * @private
   */
  connectedCallback() {
    const cancelButton = this.shadowRoot.querySelector('#cancel');
    cancelButton.onclick = () => this.cancel_();

    const unlockButton = this.shadowRoot.querySelector('#unlock');
    unlockButton.onclick = () => this.unlock_();

    this.dialog_.addEventListener('close', () => this.onClose_());
  }

  /**
   * Asks the user for a password to open the given file.
   * @param {string} filename Name of the file to open.
   * @param {?string} password Previously entered password. If not null, it
   *     indicates that an invalid password was previously tried.
   * @return {!Promise<!string>} Password provided by the user. The returned
   *     promise is rejected with FilesPasswordDialog.USER_CANCELLED if the user
   *     presses Cancel.
   */
  async askForPassword(filename, password = null) {
    const mutexUnlock = await this.mutex.lock();
    try {
      return await new Promise((resolve, reject) => {
        this.success_ = false;
        this.resolve_ = resolve;
        this.reject_ = reject;
        if (password != null) {
          this.input_.value = password;
          // An invalid password has previously been entered for this file.
          // Display an 'invalid password' error message.
          this.input_.invalid = true;
        } else {
          this.input_.invalid = false;
        }
        this.showModal_(filename);
        this.input_.inputElement.select();
      });
    } finally {
      mutexUnlock();
    }
  }

  /**
   * Shows the password prompt represented by |filename|.
   * @param {!string} filename
   * @private
   */
  showModal_(filename) {
    this.filename_ = filename;
    this.dialog_.querySelector('#name').innerText = filename;
    this.dialog_.showModal();
  }

  /**
   * Triggers a 'Cancelled by user' error.
   * @private
   */
  cancel_() {
    this.dialog_.close();
  }

  /**
   * Sends user input password.
   * @private
   */
  unlock_() {
    this.dialog_.close();
    this.success_ = true;
  }

  /**
   * Resolves the promise when the dialog is closed.
   * This can be triggered by the buttons, Esc key or anything that closes the
   * dialog.
   * @private
   */
  onClose_() {
    if (this.success_) {
      this.resolve_(this.input_.value);
    } else {
      this.reject_(FilesPasswordDialog.USER_CANCELLED);
    }
    this.input_.value = '';
  }
}

/**
 * Exception thrown when user cancels the password dialog box.
 * @const {!Error}
 */
FilesPasswordDialog.USER_CANCELLED = new Error('Cancelled by user');

window.customElements.define('files-password-dialog', FilesPasswordDialog);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_password_dialog.js
