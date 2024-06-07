// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrChipElement} from './cr_chip.js';

export function getHtml(this: CrChipElement) {
  return html`
<button id="button" ?selected="${this.selected}"
    ?disabled="${this.disabled}" aria-pressed="${this.selected}"
    role="${this.chipRole}" aria-label="${this.chipAriaLabel}">
  <div id="hoverLayer"></div>
  <slot></slot>
</button>`;
}
