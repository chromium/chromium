// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxToolChipElement} from './composebox_tool_chip.js';

export function getHtml(this: ComposeboxToolChipElement) {
  // clang-format off
  return html`
<cr-button id="toolEnabledButton" class="upload-icon no-overlap"
  aria-label="${this.removeChipAriaLabel}">
  <div class="icon-container" slot="prefix-icon">
    <cr-icon class="tool-icon" .icon="${this.icon}"></cr-icon>
    <cr-icon class="close-icon" icon="cr:close"></cr-icon>
  </div>
  <div>${this.label}</div>
</cr-button>`;
  // clang-format on
}
