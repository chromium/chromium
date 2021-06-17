// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user to encrypt a personal certificate
 * before it is exported to disk.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import '../../cr_elements/cr_input/cr_input.m.js';
import '../../cr_elements/shared_vars_css.m.js';
import './certificate_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrButtonElement} from '../../cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nBehavior} from '../../js/i18n_behavior.m.js';

import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode} from './certificates_browser_proxy.js';

export interface CertificatePasswordEncryptionDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: CrButtonElement,
  };
}

const CertificatePasswordEncryptionDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

export class CertificatePasswordEncryptionDialogElement extends
    CertificatePasswordEncryptionDialogElementBase {
  static get is() {
    return 'certificate-password-encryption-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificateSubnode} */
      model: Object,

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

  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    CertificatesBrowserProxyImpl.getInstance()
        .exportPersonalCertificatePasswordSelected(this.password_)
        .then(
            () => {
              this.$.dialog.close();
            },
            error => {
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

customElements.define(
    CertificatePasswordEncryptionDialogElement.is,
    CertificatePasswordEncryptionDialogElement);
