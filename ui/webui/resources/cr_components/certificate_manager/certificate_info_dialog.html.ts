// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateInfoDialogElement} from './certificate_info_dialog.js';

export function getHtml(this: CertificateInfoDialogElement) {
  return html`
    <cr-dialog id="dialog" show-on-attach>
      <div slot="title">${this.dialogTitle}</div>
      <div slot="body">${this.dialogMessage}</div>
      <div slot="button-container">
        <cr-button class="action-button" @click="${this.onOkClick_}">
          ${this.i18n('ok')}
        </cr-button>
      </div>
    </cr-dialog>`;
}
