// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxAimButtonElement} from './composebox_aim_button.js';

export function getHtml(this: ComposeboxAimButtonElement) {
  // Layout: leading icon, label, trailing icon, all inside a cr-button, so the
  // main button node is never re-created and focus is kept across states.
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-button id="button"
    class="${this.getButtonClass_()}"
    ?disabled="${this.disabled}"
    ?noink="${this.showEnergyEffect_()}"
    title="${this.getTitle_()}"
    aria-label="${this.getTitle_()}"
    @click="${this.onClick_}">
  <div id="leadingIcon"
      slot="prefix-icon"
      class="${this.getLeadingIconClass_()}"
      aria-hidden="true">
  </div>
  <span id="label">${this.label}</span>
  <div id="trailingIcon"
      slot="suffix-icon"
      class="${this.getTrailingIcon_()}"
      aria-hidden="true">
  </div>
</cr-button>
${this.showEnergyEffect_() ? html`
  <!-- After #button so the pressed state reaches it via a sibling selector. -->
  <div id="energy" class="energy-glow"></div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
