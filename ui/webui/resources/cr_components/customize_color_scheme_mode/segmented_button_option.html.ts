// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SegmentedButtonOptionElement} from './segmented_button_option.js';

export function getHtml(this: SegmentedButtonOptionElement) {
  return html`
<div role="radio"
    aria-checked="${this.getAriaChecked()}"
    aria-disabled="${this.getAriaDisabled()}"
    id="button"
    tabindex="${this.getButtonTabIndex()}"
    aria-labelledby="content"
    @keydown="${this.onInputKeydown}">
  <div id="container">
    <cr-icon icon="cr:check" id="checkmark"></cr-icon>
    <slot id="prefixIcon" name="prefix-icon"></slot>
    <span id="content"><slot></slot></span>
  </div>
  <div id="overlay"></div>
</div>`;
}
