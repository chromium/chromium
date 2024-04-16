// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {AsyncQueue} from '../common/js/async_util.js';

import {getTemplate} from './xf_password_dialog.html.js';


/**
 * The custom element tag name.
 */
export const TAG_NAME = 'xf-password-dialog';

/**
 * Exception thrown when user cancels the password dialog box.
 */
export const USER_CANCELLED: Error = new Error('Cancelled by user');

/**
 * Dialog to request user to enter password. Uses the askForPassword() which
 * resolves with either the password or rejected with USER_CANCELLED.
 */
export class XfPasswordDialog extends HTMLElement {
  /**
   * Mutex used to serialize modal dialogs and error notifications.
   */
  private mutex_: AsyncQueue = new AsyncQueue();

  /**
   * Controls whether the user is validating the password (Unlock button or
   * Enter key) or cancelling the dialog (Cancel button or Escape key).
   */
  private success_: boolean = false;

  /**
   * Return input password using the resolve method of a Promise.
   */
  private resolve_: ((password: string) => void)|null = null;

  /**
   * Return password prompt error using the reject method of a Promise.
   */
  private reject_: ((error: Error) => void)|null = null;

  /**
   * Password dialog.
   * TODO(crbug.com/40858292): This type should be CrDialogElement, and
   * an import of that type from cr_dialog.js should be added to this file.
   */
  private dialog_: CrDialogElement;

  /**
   * Input field for password.
   */
  private input_: CrInputElement;

  constructor() {
    super();

    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.dialog_ = this.shadowRoot!.querySelector('#password-dialog')!;
    this.dialog_.consumeKeydownEvent = true;
    this.input_ = this.shadowRoot!.querySelector('#input')!;
    this.input_.errorMessage =
        loadTimeData.getString('PASSWORD_DIALOG_INVALID');
  }

  /**
   * Called when this element is attached to the DOM.
   */
  connectedCallback() {
    const cancelButton =
        this.shadowRoot!.querySelector<CrButtonElement>('#cancel')!;
    cancelButton.onclick = () => this.cancel_();

    const unlockButton =
        this.shadowRoot!.querySelector<CrButtonElement>('#unlock')!;
    unlockButton.onclick = () => this.unlock_();

    this.dialog_.addEventListener('close', () => this.onClose_());
  }

  /**
   * Asks the user for a password to open the given file.
   * @param filename Name of the file to open.
   * @param password Previously entered password. If not null, it
   *     indicates that an invalid password was previously tried.
   * @return Password provided by the user. The returned
   *     promise is rejected with USER_CANCELLED if the user
   *     presses Cancel.
   */
  async askForPassword(filename: string, password: string|null = null):
      Promise<string> {
    const mutexUnlock = await this.mutex_.lock();
    try {
      return await new Promise((resolve, reject) => {
        this.success_ = false;
        this.resolve_ = resolve;
        this.reject_ = reject;
        if (password !== null) {
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
   * @param filename
   */
  private showModal_(filename: string) {
    this.dialog_.querySelector<HTMLElement>('#name')!.innerText = filename;
    this.dialog_.showModal();
  }

  /**
   * Triggers a 'Cancelled by user' error.
   */
  private cancel_() {
    this.dialog_.close();
  }

  /**
   * Sends user input password.
   */
  private unlock_() {
    this.dialog_.close();
    this.success_ = true;
  }

  /**
   * Resolves the promise when the dialog is closed.
   * This can be triggered by the buttons, Esc key or anything that closes the
   * dialog.
   */
  private onClose_() {
    if (this.success_) {
      this.resolve_!(this.input_.value);
    } else {
      this.reject_!(USER_CANCELLED);
    }
    this.input_.value = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: XfPasswordDialog;
  }
}

customElements.define(TAG_NAME, XfPasswordDialog);
