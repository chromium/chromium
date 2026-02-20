// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';
import {getHtml as getDropdownHtml} from './searchbox_searchbox_dropdown.html.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`
<contextual-entrypoint-and-carousel id="context"
    part="contextual-entrypoint-and-carousel"
    exportparts="composebox-entrypoint, context-menu-entrypoint-icon, voice-icon, context-menu-and-tools"
    .tabSuggestions="${this.tabSuggestions_}"
    .recentTabForChip="${this.recentTabForChip_}"
    entrypoint-name="Realbox"
    @add-tab-context="${this.addTabContext_}"
    @add-file-context="${this.addFileContext_}"
    @set-tool-mode="${this.onSetToolMode_}"
    @model-click="${this.onModelClick_}"
    @get-tab-preview="${this.getTabPreview_}"
    @context-menu-container-click="${this.onContextMenuContainerClick_}"
    @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
    @context-menu-closed="${this.onContextMenuClosed_}"
    @context-menu-opened="${this.onContextMenuOpened_}"
    ?show-dropdown="${this.dropdownIsVisible}"
    ?show-recent-tab-chip="${!this.useCompactLayout_() &&
        this.computeShowRecentTabChip_()}"
    .inputState="${this.inputState_}"
    ?show-model-picker="${this.showModelPicker_}"
    searchbox-layout-mode="${this.searchboxLayoutMode}"
    context-menu-glif-animation-state="${this.contextMenuGlifAnimationState}">
  ${!this.useCompactLayout_() ? getDropdownHtml.bind(this)() : nothing}
</contextual-entrypoint-and-carousel>
`;
  // clang-format on
}
