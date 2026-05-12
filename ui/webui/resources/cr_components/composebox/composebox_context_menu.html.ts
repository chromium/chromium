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
    @mousedown="${this.onContextMenuContainerMousedown}"
    @click="${this.onContextMenuContainerClick}">
  ${this.showMenuOnClick ? html`
    <cr-composebox-contextual-entrypoint-and-menu
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon, entrypoint-button"
        class="upload-button no-overlap"
        @add-tab-context="${this.onAddTabContext}"
        @delete-tab-context="${this.onDeleteTabContext}"
        @tool-click="${this.onToolClick}"
        @model-click="${this.onModelClick}"
        @get-tab-preview="${this.onGetTabPreview}"
        @context-menu-closed="${this.onContextMenuClosed}"
        @context-menu-opened="${this.onContextMenuOpened}"
        @open-image-upload="${this.onOpenImageUpload}"
        @open-file-upload="${this.onOpenFileUpload}"
        @open-drive-upload="${this.onOpenDriveUpload}"
        @smart-tab-sharing-active-changed="${
            this.onSmartTabSharingActiveChanged}"
        .inputState="${this.inputState}"
        .usePecApi="${this.usePecApi}"
        .smartTabSharingActive="${this.smartTabSharingActive}"
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
        exportparts="context-menu-entrypoint-icon, entrypoint-button"
        class="upload-button no-overlap"
        .inputState="${this.inputState}"
        ?upload-button-disabled="${this.uploadButtonDisabled}"
        ?show-context-menu-description="${this.showContextMenuDescription}">
    </cr-composebox-contextual-entrypoint-button>
  ` : '')}
  ${this.searchboxLayoutMode === 'Compact' && this.shouldShowVoiceSearch() ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon"
        part="voice-icon" iron-icon="cr:mic"
        @click="${this.onVoiceSearchButtonClick}"
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
