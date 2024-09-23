// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrRadioButtonElement} from './cr_radio_button.js';

export function getHtml(this: CrRadioButtonElement) {
  return html`
<div aria-checked="${this.getAriaChecked()}"
    aria-describedby="slotted-content"
    aria-disabled="${this.getAriaDisabled()}"
    aria-labelledby="label"
    class="disc-wrapper"
    id="button"
    role="radio"
    tabindex="${this.getButtonTabIndex()}"
    @keydown="${this.onInputKeydown}">
  <div class="disc-border"></div>
  <div class="disc"></div>
  <div id="overlay"></div>
</div>

<div id="labelWrapper">
  <span id="label" ?hidden="${!this.label}" aria-hidden="true">
    ${this.label}
  </span>
  <span id="slotted-content">
    <slot></slot>
  </span>
</div>`;
}
