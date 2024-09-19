// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-confirmation-dialog' component is for showing
 * a dialog box that prompts the user to confirm or cancel an operation such as
 * deleting a certificate.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './certificate_confirmation_dialog.html.js';

const CertificateConfirmationDialogElementBase = I18nMixinLit(CrLitElement);

export interface CertificateConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: CrButtonElement,
    cancel: CrButtonElement,
  };
}

export class CertificateConfirmationDialogElement extends
    CertificateConfirmationDialogElementBase {
  static get is() {
    return 'certificate-confirmation-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialogTitle: {type: String},
      dialogMessage: {type: String},
    };
  }

  dialogTitle: string;
  dialogMessage: string;

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
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
    'certificate-confirmation-dialog': CertificateConfirmationDialogElement;
  }
}

customElements.define(
    CertificateConfirmationDialogElement.is,
    CertificateConfirmationDialogElement);
