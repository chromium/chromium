// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxComposeButtonElement} from './searchbox_compose_button.js';

export function getHtml(this: SearchboxComposeButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="glowAnimationWrapper" class="glow-container play"
    part="glow-container">
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
      part="compose-button"
      exportparts="hoverBackground"
      title="${this.i18n('searchboxComposeButtonTitle')}">
    <div slot="prefix-icon" class="compose-icon" part="compose-icon"
        style="-webkit-mask-image: url(${this.composeIcon_});
               mask-image: url(${this.composeIcon_});">
    </div>
    <span id="label" part="label">${this.i18n('searchboxComposeButtonText')}</span>
    <div id="arrowIcon" slot="suffix-icon" class="arrow-icon" part="arrow-icon"
        style="-webkit-mask-image: url(${this.arrowIcon_});
               mask-image: url(${this.arrowIcon_});">
    </div>
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
