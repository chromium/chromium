// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ErrorScrimElement} from './error_scrim.js';

export function getHtml(this: ErrorScrimElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.showErrorScrim_ ? html`
    <div id="errorScrim">
      <p id="errorMessage">${this.errorMessage_}</p>
      <cr-button id="dismissErrorButton"
          @click="${this.onDismissErrorButtonClick_}">
        <cr-icon icon="cr:close" slot="prefix-icon"></cr-icon>
        <div>${this.i18n('dismissButton')}</div>
      </cr-button>
    </div>
  `: ''}
<!--_html_template_end_-->`;
  // clang-format on
}