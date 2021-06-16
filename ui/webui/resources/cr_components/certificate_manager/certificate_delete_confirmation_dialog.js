// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A confirmation dialog allowing the user to delete various types
 * of certificates.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import './certificate_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertNotReached} from '../../js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CertificateDeleteConfirmationDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CertificateDeleteConfirmationDialogElement extends
    CertificateDeleteConfirmationDialogElementBase {
  static get is() {
    return 'certificate-delete-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificateSubnode} */
      model: Object,

      /** @type {!CertificateType} */
      certificateType: String,
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
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  }

  /**
   * @private
   * @return {string}
   */
  getTitleText_() {
    /**
     * @param {string} localizedMessageId
     * @return {string}
     */
    const getString = localizedMessageId =>
        loadTimeData.getStringF(localizedMessageId, this.model.name);

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserTitle');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerTitle');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaTitle');
      case CertificateType.OTHER:
        return getString('certificateManagerDeleteOtherTitle');
    }
    assertNotReached();
  }

  /**
   * @private
   * @return {string}
   */
  getDescriptionText_() {
    const getString = loadTimeData.getString.bind(loadTimeData);
    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserDescription');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerDescription');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaDescription');
      case CertificateType.OTHER:
        return '';
    }
    assertNotReached();
  }

  /** @private */
  onCancelTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  }

  /** @private */
  onOkTap_() {
    this.browserProxy_.deleteCertificate(this.model.id)
        .then(
            () => {
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

customElements.define(
    CertificateDeleteConfirmationDialogElement.is,
    CertificateDeleteConfirmationDialogElement);
