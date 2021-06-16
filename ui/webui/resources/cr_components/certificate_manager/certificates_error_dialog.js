// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog for showing SSL certificate related error messages.
 * The user can only close the dialog, there is no other possible interaction.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import './certificate_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {CertificatesError, CertificatesImportError} from './certificates_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CertificatesErrorDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class CertificatesErrorDialogElement extends
    CertificatesErrorDialogElementBase {
  static get is() {
    return 'certificates-error-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificatesError|!CertificatesImportError} */
      model: Object,
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  }

  /** @private */
  onOkTap_() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  }

  /**
   * @param {{name: string, error: string}} importError
   * @return {string}
   * @private
   */
  getCertificateErrorText_(importError) {
    return loadTimeData.getStringF(
        'certificateImportErrorFormat', importError.name, importError.error);
  }
}

customElements.define(
    CertificatesErrorDialogElement.is, CertificatesErrorDialogElement);
