// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {hasAllowedInputs} from './common.js';
import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`
<div class="context-menu-container" id="contextMenuContainer"
    part="context-menu-and-tools"
    @mousedown="${this.onContextMenuContainerMousedown_}"
    @click="${this.onContextMenuContainerClick_}">
  ${this.showMenuOnClick ? html`
    <cr-composebox-contextual-entrypoint-and-menu
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button no-overlap"
        @add-tab-context="${this.onAddTabContext_}"
        @delete-tab-context="${this.onDeleteTabContext_}"
        @tool-click="${this.onToolClick}"
        @model-click="${this.onModelClick}"
        @get-tab-preview="${this.onGetTabPreview}"
        @context-menu-closed="${this.onContextMenuClosed_ }"
        @context-menu-opened="${this.onContextMenuOpened_}"
        @open-image-upload="${this.onOpenImageUpload}"
        @open-file-upload="${this.onOpenFileUpload}"
        @smart-tab-sharing-active-changed="${
            this.onSmartTabSharingActiveChanged_}"
        .inputState="${this.inputState}"
        .smartTabSharingActive="${this.smartTabSharingActive_}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions}"
        .hasImageFiles="${this.hasImageFiles()}"
        .disabledTabIds="${this.addedTabsIds}"
        .fileNum="${this.files.size}"
        ?upload-button-disabled="${this.uploadButtonDisabled}"
        ?show-context-menu-description="${this.showContextMenuDescription}">
    </cr-composebox-contextual-entrypoint-and-menu>
  ` : (hasAllowedInputs(this.inputState, this.usePecApi) ? html`
    <cr-composebox-contextual-entrypoint-button
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button no-overlap"
        .inputState="${this.inputState}"
        ?upload-button-disabled="${this.uploadButtonDisabled}"
        ?show-context-menu-description="${this.showContextMenuDescription}">
    </cr-composebox-contextual-entrypoint-button>
  ` : '')}
  ${this.searchboxLayoutMode === 'Compact' && this.shouldShowVoiceSearch_() ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon"
        part="voice-icon" iron-icon="cr:mic"
        @click="${this.onVoiceSearchButtonClick_}"
        title="${this.i18n('voiceSearchButtonLabel')}">
    </cr-icon-button>
  ` : ''}
  ${this.searchboxLayoutMode !== 'Compact' ? html`
    ${this.inToolMode ? html`
      <cr-composebox-tool-chip
        exportparts="tool-chip-label"
        .inputState="${this.inputState}"
        .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
        @tool-click="${this.onToolClick}">
      </cr-composebox-tool-chip>
    ` : ''}
  ` : ''}
</div>
  `;
  // clang-format on
}
