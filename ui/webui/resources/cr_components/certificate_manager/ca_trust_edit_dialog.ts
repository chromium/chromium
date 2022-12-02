// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'ca-trust-edit-dialog' allows the user to:
 *  - specify the trust level of a certificate authority that is being
 *    imported.
 *  - edit the trust level of an already existing certificate authority.
 */
import '../../cr_elements/cr_button/cr_button.js';
import '../../cr_elements/cr_checkbox/cr_checkbox.js';
import '../../cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './certificate_shared.css.js';

import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrCheckboxElement} from '../../cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '../../cr_elements/i18n_mixin.js';
import {loadTimeData} from '../../js/load_time_data.js';

import {getTemplate} from './ca_trust_edit_dialog.html.js';
import {CaTrustInfo, CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, NewCertificateSubNode} from './certificates_browser_proxy.js';

export interface CaTrustEditDialogElement {
  $: {
    dialog: CrDialogElement,
    email: CrCheckboxElement,
    objSign: CrCheckboxElement,
    ok: HTMLElement,
    spinner: PaperSpinnerLiteElement,
    ssl: CrCheckboxElement,
  };
}

const CaTrustEditDialogElementBase = I18nMixin(PolymerElement);

export class CaTrustEditDialogElement extends CaTrustEditDialogElementBase {
  static get is() {
    return 'ca-trust-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      trustInfo_: Object,
      explanationText_: String,
    };
  }

  model: CertificateSubnode|NewCertificateSubNode;
  private trustInfo_: CaTrustInfo|null;
  private explanationText_: string;
  private browserProxy_: CertificatesBrowserProxy|null = null;

  override ready() {
    super.ready();
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation', this.model.name);

    // A non existing |model.id| indicates that a new certificate is being
    // imported, otherwise an existing certificate is being edited.
    if ((this.model as CertificateSubnode).id) {
      this.browserProxy_!
          .getCaCertificateTrust((this.model as CertificateSubnode).id)
          .then(trustInfo => {
            this.trustInfo_ = trustInfo;
            this.$.dialog.showModal();
          });
    } else {
      this.$.dialog.showModal();
    }
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    this.$.spinner.active = true;

    const whenDone = (this.model as CertificateSubnode).id ?
        this.browserProxy_!.editCaCertificateTrust(
            (this.model as CertificateSubnode).id, this.$.ssl.checked,
            this.$.email.checked, this.$.objSign.checked) :
        this.browserProxy_!.importCaCertificateTrustSelected(
            this.$.ssl.checked, this.$.email.checked, this.$.objSign.checked);

    whenDone.then(
        () => {
          this.$.spinner.active = false;
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
    'ca-trust-edit-dialog': CaTrustEditDialogElement;
  }
}

customElements.define(CaTrustEditDialogElement.is, CaTrustEditDialogElement);
