// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxSubmitElement} from './composebox_submit.js';

export function getHtml(this: ComposeboxSubmitElement) {
  // clang-format off
  return html`
<div id="submitContainer" class="icon-fade" part="submit"
    tabindex="-1" @click="${this.onSubmitClick_}"
    @focusin="${this.onSubmitFocusin_}">
  <div id="submitEnergy" part="button-energy"></div>
  <div id="submitOverlay" part="submit-overlay"
      title="${this.submitButtonTitle || this.i18n('composeboxSubmitButtonTitle')}">
  </div>
  <cr-icon-button id="submitIcon"
      class="action-icon ${this.submitButtonIconClass_()}"
      part="action-icon submit-icon" tabindex="0"
      title="${this.submitButtonTitle || this.i18n('composeboxSubmitButtonTitle')}"
      ?disabled="${this.disabled}">
  </cr-icon-button>
</div>`;
  // clang-format on
}
