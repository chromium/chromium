// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {AsyncUtil} from '../../common/js/async_util.m.js';

/**
 * FilesPasswordDialog template.
 * @const @type {string}
 */
const filesPasswordDialogTemplate = `
  <style>
    [slot='body'] > div {
      margin-bottom: var(--cr-form-field-bottom-spacing);
    }

    [slot='body'] > #input {
      margin-bottom: 0;
      padding-bottom: 2px;
    }

    cr-dialog::part(dialog) {
      width: 384px;
      border-radius: 12px;
    }

    cr-dialog::part(wrapper) {
      /* subtract the internal padding in <cr-dialog> */
      padding: calc(24px - 20px);
    }
  </style>

  <cr-dialog id="password-dialog">
    <div slot="title">
      $i18n{PASSWORD_DIALOG_TITLE}
    </div>
    <div slot="body">
      <div id="name" ></div>
      <cr-input id="input" type="password" auto-validate="true">
      </cr-input>
    </div>
    <div slot="button-container">
      <cr-button class="cancel-button" id="cancel">
      $i18n{CANCEL_LABEL}
      </cr-button>
      <cr-button class="action-button" id="unlock">
          $i18n{PASSWORD_DIALOG_CONFIRM_LABEL}
      </cr-button>
    </div>
  </cr-dialog>
`;

/**
 * Dialog to request user to enter password. Uses the askForPassword() which
 * resolves with either the password or rejected with
 * FilesPasswordDialog.USER_CANCELLED.
 * @extends HTMLElement
 */
/* #export */ class FilesPasswordDialog extends HTMLElement {
  constructor() {
    /*
     * Create element content.
     */
    super().attachShadow({mode: 'open'}).innerHTML =
        filesPasswordDialogTemplate;

    /**
     * Mutex used to serialize modal dialogs and error notifications.
     * @private {?AsyncUtil.Queue}
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
     * @private {?CrDialogElement}
     */
    this.dialog_ = null;

    /**
     * Input field for password.
     * @private {?CrInputElement}
     */
    this.input_ = null;
  }

  get mutex() {
    if (!this.mutex_) {
      this.mutex_ = new AsyncUtil.Queue();
    }
    return this.mutex_;
  }

  /**
   * Called when this element is attached to the DOM.
   *
   * @private
   */
  connectedCallback() {
    this.dialog_ = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#password-dialog'));
    this.input_ = /** @type {!CrInputElement} */ (
        this.shadowRoot.querySelector('#input'));

    const cancelButton = this.shadowRoot.querySelector('#cancel');
    cancelButton.onclick = () => this.cancel_();
    const unlockButton = this.shadowRoot.querySelector('#unlock');
    unlockButton.onclick = () => this.unlock_();

    this.dialog_.addEventListener('close', () => this.onClose_());
    this.input_.errorMessage =
        loadTimeData.getString('PASSWORD_DIALOG_INVALID');
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
        this.input_.focus();
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
