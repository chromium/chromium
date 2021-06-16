// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'ca-trust-edit-dialog' allows the user to:
 *  - specify the trust level of a certificate authority that is being
 *    imported.
 *  - edit the trust level of an already existing certificate authority.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_checkbox/cr_checkbox.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './certificate_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {CaTrustInfo, CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, NewCertificateSubNode} from './certificates_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CaTrustEditDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CaTrustEditDialogElement extends CaTrustEditDialogElementBase {
  static get is() {
    return 'ca-trust-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificateSubnode|!NewCertificateSubNode} */
      model: Object,

      /** @private {?CaTrustInfo} */
      trustInfo_: Object,

      /** @private {string} */
      explanationText_: String,
    };
  }

  constructor() {
    super();
    /** @private {?CertificatesBrowserProxy} */
    this.browserProxy_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation', this.model.name);

    // A non existing |model.id| indicates that a new certificate is being
    // imported, otherwise an existing certificate is being edited.
    if (this.model.id) {
      this.browserProxy_.getCaCertificateTrust(this.model.id)
          .then(trustInfo => {
            this.trustInfo_ = trustInfo;
            this.$.dialog.showModal();
          });
    } else {
      /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
    }
  }

  /** @private */
  onCancelTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  }

  /** @private */
  onOkTap_() {
    this.$.spinner.active = true;

    const whenDone = this.model.id ?
        this.browserProxy_.editCaCertificateTrust(
            this.model.id, this.$.ssl.checked, this.$.email.checked,
            this.$.objSign.checked) :
        this.browserProxy_.importCaCertificateTrustSelected(
            this.$.ssl.checked, this.$.email.checked, this.$.objSign.checked);

    whenDone.then(
        () => {
          this.$.spinner.active = false;
          /** @type {!CrDialogElement} */ (this.$.dialog).close();
        },
        error => {
          /** @type {!CrDialogElement} */ (this.$.dialog).close();
          this.dispatchEvent(new CustomEvent('certificates-error', {
            bubbles: true,
            composed: true,
            detail: {error: error, anchor: null},
          }));
        });
  }
}

customElements.define(CaTrustEditDialogElement.is, CaTrustEditDialogElement);
