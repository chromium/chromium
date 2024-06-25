// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeHueSliderDialogElement} from './theme_hue_slider_dialog.js';

export function getHtml(this: ThemeHueSliderDialogElement) {
  // clang-format off
  return html`
<dialog id="dialog">
  <div id="header">
    <h2 id="title">${this.i18n('hueSliderTitle')}</h2>
    <slot name="headerSuffix"></slot>
    <cr-icon-button id="close" class="icon-clear"
        aria-label="${this.i18n('close')}"
        title="${this.i18n('close')}"
        @click="${this.hide}">
    </cr-icon-button>
  </div>
  <cr-slider id="slider" .min="${this.minHue_}" .max="${this.maxHue_}"
      .value="${this.selectedHue}"
      @cr-slider-value-changed="${this.onCrSliderValueChanged_}"
      @pointerup="${this.updateSelectedHueValue_}"
      @keyup="${this.updateSelectedHueValue_}"
      .style="--hue-gradient_: ${this.hueGradient_}; --knob-hue_: ${this.knobHue_}"
      aria-label="${this.i18n('hueSliderAriaLabel', this.minHue_, this.maxHue_)}">
  </cr-slider>
</dialog>`;
  // clang-format on
}
