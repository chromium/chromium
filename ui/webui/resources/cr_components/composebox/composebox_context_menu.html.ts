// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ComposeboxElement} from './composebox.js';
import {getHtml as getSubmitButtonHtml} from './composebox_submit_button.html.js';
import {getHtml as getToolChipsHtml} from './composebox_tool_chips.html.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`
<div class="context-menu-container" id="contextMenuContainer"
    part="context-menu-and-tools"
    @mousedown="${this.onContextMenuContainerMouseDown_}"
    @click="${this.onContextMenuContainerClick_}">
  ${this.showMenuOnClick ? html`
    <cr-composebox-contextual-entrypoint-and-menu
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button no-overlap"
        @add-tab-context="${this.addTabContext_}"
        @delete-tab-context="${this.onDeleteFile_}"
        @tool-click="${this.onToolClick_}"
        @deep-search-click="${this.handleDeepSearchClick_}"
        @create-image-click="${this.handleImageGenClick_}"
        @model-click="${this.onModelClick_}"
        @get-tab-preview="${this.getTabPreview_}"
        @context-menu-closed="${this.onContextMenuClosed_ }"
        @context-menu-opened="${this.onContextMenuOpened_}"
        .showModelPicker="${this.showModelPicker_}"
        .inputState="${this.inputState_}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions_}"
        .inCreateImageMode="${
            this.activeToolMode_ === ComposeboxToolMode.kImageGen}"
        .hasImageFiles="${this.hasImageFiles_()}"
        .disabledTabIds="${this.addedTabsIds_}"
        .fileNum="${this.files_.size}"
        ?upload-button-disabled="${this.uploadButtonDisabled_}"
        ?show-context-menu-description="${this.showContextMenuDescription_}">
    </cr-composebox-contextual-entrypoint-and-menu>
  ` : html`
    <cr-composebox-contextual-entrypoint-button
        id="contextEntrypoint"
        part="composebox-entrypoint"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button no-overlap"
        .inputState="${this.inputState_}"
        ?upload-button-disabled="${this.uploadButtonDisabled_}"
        ?show-context-menu-description="${this.showContextMenuDescription_}">
    </cr-composebox-contextual-entrypoint-button>
  `}
  ${this.searchboxLayoutMode === 'Compact' && this.shouldShowVoiceSearch_() ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon"
        part="voice-icon" iron-icon="cr:mic"
        @click="${this.openAimVoiceSearch_}"
        title="${this.i18n('voiceSearchButtonLabel')}">
    </cr-icon-button>
  ` : ''}
  ${this.searchboxLayoutMode !== 'Compact' ? getToolChipsHtml.bind(this)() : ''}
  ${this.searchboxLayoutMode === 'TallTopContext' ? html`
    ${this.shouldShowVoiceSearch_() ? html`
      <cr-icon-button id="voiceSearchButton" class="voice-icon"
          part="voice-icon" iron-icon="cr:mic"
          @click="${this.openAimVoiceSearch_}"
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
