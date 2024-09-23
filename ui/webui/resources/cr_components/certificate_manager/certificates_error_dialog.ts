// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog for showing SSL certificate related error messages.
 * The user can only close the dialog, there is no other possible interaction.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import './certificate_shared.css.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificatesError, CertificatesImportError} from './certificates_browser_proxy.js';
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

  private onOkClick_() {
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
