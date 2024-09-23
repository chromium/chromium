// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-password-dialog' component is for showing
 * a dialog box that prompts the user to enter a password to decrypt a file
 * during client certificate import.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './certificate_password_dialog.html.js';

const CertificatePasswordDialogElementBase = I18nMixinLit(CrLitElement);

export interface CertificatePasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    password: CrInputElement,
    ok: CrButtonElement,
    cancel: CrButtonElement,
  };
}

export class CertificatePasswordDialogElement extends
    CertificatePasswordDialogElementBase {
  static get is() {
    return 'certificate-password-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  value(): string|null {
    return this.wasConfirmed() ? this.$.password.value : null;
  }

  protected onOkClick_() {
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-password-dialog': CertificatePasswordDialogElement;
  }
}

customElements.define(
    CertificatePasswordDialogElement.is, CertificatePasswordDialogElement);
