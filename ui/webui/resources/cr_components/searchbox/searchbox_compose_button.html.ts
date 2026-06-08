// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxComposeButtonElement} from './searchbox_compose_button.js';

export function getHtml(this: SearchboxComposeButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="glowAnimationWrapper" class="glow-container play">
  ${this.energyEffectAnimationEnabled_ ? html`
  <div class="input-plate-gradient">
    <div class="input-plate-gradient-mask">
      <div class="gradient-blur-wrapper">
        <div class="gradient-mask">
          <div class="gradient"></div>
        </div>
      </div>
      <div class="gradient-blur-wrapper sharp-tip">
        <div class="gradient-mask">
          <div class="gradient"></div>
        </div>
      </div>
    </div>
  </div>` : html`
  <div class="gradient-and-mask-wrapper outer-glow">
    <div class="gradient"></div>
    <div class="mask"></div>
  </div>
  <div class="gradient-and-mask-wrapper">
    <div class="gradient"></div>
    <div class="mask"></div>
  </div>`}
  <cr-button @click="${this.onClick_}" id="composeButton"
      class="compose-container"
      title="${this.i18n('searchboxComposeButtonTitle')}">
    <img slot="prefix-icon" src="${this.composeIcon_}" class="compose-icon">
    ${this.i18n('searchboxComposeButtonText')}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
