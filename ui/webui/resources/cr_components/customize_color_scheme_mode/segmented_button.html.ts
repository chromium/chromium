// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SegmentedButtonElement} from './segmented_button.js';

export function getHtml(this: SegmentedButtonElement) {
  return html`
<cr-radio-group
    .selected="${this.selected}"
    selectable-elements="segmented-button-option"
    aria-label="${this.groupAriaLabel}">
  <slot></slot>
</cr-radio-group>`;
}
