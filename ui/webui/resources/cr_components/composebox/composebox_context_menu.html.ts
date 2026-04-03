// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {hasAllowedInputs} from './common.js';
import type {ComposeboxElement} from './composebox.js';
import {ToolMode} from './composebox_query.mojom-webui.js';
import {getHtml as getSubmitButtonHtml} from './composebox_submit_button.html.js';

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
        @tool-click="${this.onToolClick_}"
        @model-click="${this.onModelClick_}"
        @get-tab-preview="${this.onGetTabPreview_}"
        @context-menu-closed="${this.onContextMenuClosed_ }"
        @context-menu-opened="${this.onContextMenuOpened_}"
        .showModelPicker="${this.showModelPicker_}"
        .inputState="${this.inputState}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions_}"
        .inCreateImageMode="${
            this.inputState?.activeTool === ToolMode.kImageGen}"
        .hasImageFiles="${this.hasImageFiles_()}"
        .disabledTabIds="${this.addedTabsIds}"
        .fileNum="${this.files.size}"
        ?upload-button-disabled="${this.uploadButtonDisabled_}"
        ?show-context-menu-description="${this.showContextMenuDescription_}">
    </cr-composebox-contextual-entrypoint-and-menu>
  ` : (hasAllowedInputs(this.inputState, this.showModelPicker_) ? html`
    <cr-composebox-contextual-entrypoint-button
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button no-overlap"
        ?upload-button-disabled="${this.uploadButtonDisabled_}"
        ?show-context-menu-description="${this.showContextMenuDescription_}">
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
    ${this.inToolMode_ ? html`
      <cr-composebox-tool-chip
        exportparts="tool-chip-label"
        .inputState="${this.inputState}"
        .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
        @tool-click="${this.onToolClick_}">
      </cr-composebox-tool-chip>
    ` : ''}
  ` : ''}
  ${this.searchboxLayoutMode === 'TallTopContext' ? html`
    ${this.shouldShowVoiceSearch_() ? html`
      <cr-icon-button id="voiceSearchButton" class="voice-icon"
          part="voice-icon" iron-icon="cr:mic"
          @click="${this.onVoiceSearchButtonClick_}"
          title="${this.i18n('voiceSearchButtonLabel')}">
      </cr-icon-button>
    ` : ''}
    ${this.shouldShowSubmitButton_ ? html`
      ${getSubmitButtonHtml.bind(this)()}
    ` : ''}
  ` : ''}
</div>
  `;
  // clang-format on
}
