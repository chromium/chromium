// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrSegmentedButtonElement} from './cr_segmented_button.js';

export function getHtml(this: CrSegmentedButtonElement) {
  return html`
<cr-radio-group .selected="${this.selected}"
    @selected-changed="${this.onSelectedChanged_}"
    selectable-elements="cr-segmented-button-option"
    aria-label="${this.groupAriaLabel}">
  <slot></slot>
</cr-radio-group>`;
}
