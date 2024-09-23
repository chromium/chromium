// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-info-dialog' component is for showing
 * a dialog box that displays informational or error messages to the user.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './certificate_info_dialog.html.js';

const CertificateInfoDialogElementBase = I18nMixinLit(CrLitElement);

export interface CertificateInfoDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}


export class CertificateInfoDialogElement extends
    CertificateInfoDialogElementBase {
  static get is() {
    return 'certificate-info-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialogTitle: {type: String},
      dialogMessage: {type: String},
    };
  }

  dialogTitle: string;
  dialogMessage: string;

  protected onOkClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-info-dialog': CertificateInfoDialogElement;
  }
}

customElements.define(
    CertificateInfoDialogElement.is, CertificateInfoDialogElement);
