// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrCheckboxElement} from './cr_checkbox.js';

export function getHtml(this: CrCheckboxElement) {
  return html`
<div id="checkbox" tabindex="${this.tabIndex}" role="checkbox"
    @keydown="${this.onKeyDown_}" @keyup="${this.onKeyUp_}"
    aria-disabled="${this.getAriaDisabled_()}"
    aria-checked="${this.getAriaChecked_()}"
    aria-labelledby="labelContainer"
    aria-describedby="ariaDescription">
  <!-- Inline SVG paints faster than loading it from a separate file. -->
  <svg id="checkmark" width="12" height="12" viewBox="0 0 12 12"
      fill="none" xmlns="http://www.w3.org/2000/svg">
    <path fill-rule="evenodd" clip-rule="evenodd"
        d="m10.192 2.121-6.01 6.01-2.121-2.12L1 7.07l2.121 2.121.707.707.354.354 7.071-7.071-1.06-1.06Z">
  </svg>
  <div id="hover-layer"></div>
</div>
<div id="labelContainer" aria-hidden="true"
    aria-label="${this.ariaLabelOverride || nothing}" part="label-container">
  <slot></slot>
</div>
<div id="ariaDescription" aria-hidden="true">${this.ariaDescription}</div>`;
}
