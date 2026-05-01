// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxToolChipElement} from './composebox_tool_chip.js';

export function getHtml(this: ComposeboxToolChipElement) {
  // clang-format off
  return html`
<cr-button id="toolEnabledButton"
  class="upload-icon no-overlap ${this.isCanvasActive_() ? 'unremovable' : ''}"
  aria-label="${
      this.isCanvasActive_() ?
          this.getToolChipLabel_() :
          this.i18n('removeToolChipAriaLabel', this.getToolChipLabel_())}"
  ?noink="${this.isCanvasActive_()}"
  @click="${this.onClick_}">
  <div class="icon-container" slot="prefix-icon">
    <cr-icon class="tool-icon" .icon="${this.getIcon_()}"></cr-icon>
    ${this.isCanvasActive_() ? '' :
        html`<cr-icon class="close-icon" id="leftCloseIcon" icon="cr:close">
             </cr-icon>`}
  </div>
  <div part="tool-chip-label" class="tool-label">
    ${this.getToolChipLabel_()}
  </div>
  ${this.isCanvasActive_() ? '' :
        html`<cr-icon class="close-icon" id="rightCloseIcon"
                icon="aim:closeSmall" slot="suffix-icon"></cr-icon>`}
</cr-button>`;
  // clang-format on
}
