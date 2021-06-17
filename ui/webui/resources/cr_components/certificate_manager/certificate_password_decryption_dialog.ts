// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user for a decryption password such that
 * a previously exported personal certificate can be imported.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import '../../cr_elements/cr_input/cr_input.m.js';
import './certificate_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nBehavior} from '../../js/i18n_behavior.m.js';

import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl} from './certificates_browser_proxy.js';

export interface CertificatePasswordDecryptionDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const CertificatePasswordDecryptionDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

export class CertificatePasswordDecryptionDialogElement extends
    CertificatePasswordDecryptionDialogElementBase {
  static get is() {
    return 'certificate-password-decryption-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
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

  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    CertificatesBrowserProxyImpl.getInstance()
        .importPersonalCertificatePasswordSelected(this.password_)
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
}

customElements.define(
    CertificatePasswordDecryptionDialogElement.is,
    CertificatePasswordDecryptionDialogElement);
