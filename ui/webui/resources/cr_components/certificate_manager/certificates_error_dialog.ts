// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog for showing SSL certificate related error messages.
 * The user can only close the dialog, there is no other possible interaction.
 */
import '../../cr_elements/cr_button/cr_button.js';
import '../../cr_elements/cr_dialog/cr_dialog.js';
import './certificate_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '../../cr_elements/i18n_mixin.js';
import {loadTimeData} from '../../js/load_time_data.js';

import {CertificatesError, CertificatesImportError} from './certificates_browser_proxy.js';
import {getTemplate} from './certificates_error_dialog.html.js';

interface CertificatesErrorDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const CertificatesErrorDialogElementBase = I18nMixin(PolymerElement);

class CertificatesErrorDialogElement extends
    CertificatesErrorDialogElementBase {
  static get is() {
    return 'certificates-error-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
    };
  }

  model: CertificatesError|CertificatesImportError;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onOkTap_() {
    this.$.dialog.close();
  }

  private getCertificateErrorText_(importError: {name: string, error: string}):
      string {
    return loadTimeData.getStringF(
        'certificateImportErrorFormat', importError.name, importError.error);
  }
}

customElements.define(
    CertificatesErrorDialogElement.is, CertificatesErrorDialogElement);
