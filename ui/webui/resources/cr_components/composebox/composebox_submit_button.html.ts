// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`
<div id="submitContainer" class="icon-fade" part="submit"
    tabindex="-1" @click="${this.submitQuery_}"
    @focusin="${this.handleSubmitFocusIn_}">
  <div id="submitOverlay" part="submit-overlay"
      title="${this.i18n('composeboxSubmitButtonTitle')}">
  </div>
  <cr-icon-button id="submitIcon"
      class="action-icon icon-arrow-upward"
      part="action-icon submit-icon" tabindex="0"
      title="${this.i18n('composeboxSubmitButtonTitle')}"
      ?disabled="${!this.canSubmitFilesAndInput_}">
  </cr-icon-button>
</div>`;
  // clang-format on
}
