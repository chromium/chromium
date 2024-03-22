// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user for a decryption password such that
 * a previously exported personal certificate can be imported.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import './certificate_shared.css.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_password_decryption_dialog.html.js';
import {CertificatesBrowserProxyImpl} from './certificates_browser_proxy.js';

export interface CertificatePasswordDecryptionDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: CrButtonElement,
  };
}

const CertificatePasswordDecryptionDialogElementBase =
    I18nMixin(PolymerElement);

export class CertificatePasswordDecryptionDialogElement extends
    CertificatePasswordDecryptionDialogElementBase {
  static get is() {
    return 'certificate-password-decryption-dialog';
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
    };
  }

  private password_: string;

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private onOkClick_() {
    CertificatesBrowserProxyImpl.getInstance()
        .importPersonalCertificatePasswordSelected(this.password_)
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
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-password-decryption-dialog':
        CertificatePasswordDecryptionDialogElement;
  }
}

customElements.define(
    CertificatePasswordDecryptionDialogElement.is,
    CertificatePasswordDecryptionDialogElement);
