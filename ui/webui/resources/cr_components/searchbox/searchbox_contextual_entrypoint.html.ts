// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';
import {getHtml as getRecentTabChipHtml} from './searchbox_recent_tab_chip.html.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`
<cr-composebox-file-inputs @on-file-change="${this.onFileChange_}">
  <div class="context-menu-container" id="contextMenuContainer"
      @mousedown="${this.onContextMenuContainerMouseDown_}"
      @click="${this.onContextMenuContainerClick_}">
    <cr-composebox-contextual-entrypoint-and-menu id="context"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button"
        @add-tab-context="${this.addTabContext_}"
        @tool-click="${this.onToolClick_}"
        @deep-search-click="${this.handleDeepSearchClick_}"
        @create-image-click="${this.handleImageGenClick_}"
        @model-click="${this.onModelClick_}"
        @get-tab-preview="${this.getTabPreview_}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
        @context-menu-closed="${this.onContextMenuClosed_}"
        @context-menu-opened="${this.onContextMenuOpened_}"
        .showModelPicker="${this.showModelPicker_}"
        .inputState="${this.inputState_}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions_}"
        ?show-context-menu-description="${
            !this.useCompactLayout_() && !this.computeShowRecentTabChip_()}"
        glif-animation-state="${this.contextMenuGlifAnimationState}">
    </cr-composebox-contextual-entrypoint-and-menu>
    ${this.useCompactLayout_() ? nothing : getRecentTabChipHtml.bind(this)()}
  </div>
</cr-composebox-file-inputs>
`;
  // clang-format on
}

