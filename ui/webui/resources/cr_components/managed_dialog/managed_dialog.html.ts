// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedDialogElement} from './managed_dialog.js';

export function getHtml(this: ManagedDialogElement) {
  return html`
<cr-dialog id="dialog" close-text="${this.i18n('close')}" show-on-attach>
  <div slot="title">
    <cr-icon icon="cr:domain" role="img"
        aria-label="${this.i18n('controlledSettingPolicy')}">
    </cr-icon>
    ${this.title}
  </div>
  <div slot="body">${this.body}</div>
  <div slot="button-container">
    <cr-button class="action-button" @click="${this.onOkClick_}">
      ${this.i18n('ok')}
    </cr-button>
  </div>
</cr-dialog>`;
}
