// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user to encrypt a personal certificate
 * before it is exported to disk.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import './certificate_shared.css.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_password_encryption_dialog.html.js';
import {CertificatesBrowserProxyImpl} from './certificates_browser_proxy.js';

export interface CertificatePasswordEncryptionDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: CrButtonElement,
  };
}

const CertificatePasswordEncryptionDialogElementBase =
    I18nMixin(PolymerElement);

export class CertificatePasswordEncryptionDialogElement extends
    CertificatePasswordEncryptionDialogElementBase {
  static get is() {
    return 'certificate-password-encryption-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password_: {
        type: String,
        value: '',
      },

      confirmPassword_: {
        type: String,
        value: '',
      },
    };
  }

  private password_: string;
  private confirmPassword_: string;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private onOkClick_() {
    CertificatesBrowserProxyImpl.getInstance()
        .exportPersonalCertificatePasswordSelected(this.password_)
        .then(
            () => {
              this.$.dialog.close();
            },
            error => {
              if (error === null) {
                return;
              }
              this.$.dialog.close();
              this.dispatchEvent(new CustomEvent('certificates-error', {
                bubbles: true,
                composed: true,
                detail: {error: error, anchor: null},
              }));
            });
  }

  private validate_() {
    const isValid =
        this.password_ !== '' && this.password_ === this.confirmPassword_;
    this.$.ok.disabled = !isValid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-password-encryption-dialog':
        CertificatePasswordEncryptionDialogElement;
  }
}

customElements.define(
    CertificatePasswordEncryptionDialogElement.is,
    CertificatePasswordEncryptionDialogElement);
