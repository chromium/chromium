// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrButtonElement} from './cr_button.js';

export function getHtml(this: CrButtonElement) {
  return html`
<div id="background"></div>
<slot id="prefixIcon" name="prefix-icon"
    @slotchange="${this.onPrefixIconSlotChanged_}">
</slot>
<span id="content"><slot></slot></span>
<slot id="suffixIcon" name="suffix-icon"
    @slotchange="${this.onSuffixIconSlotChanged_}">
</slot>
<div id="hoverBackground" part="hoverBackground"></div>`;
}
