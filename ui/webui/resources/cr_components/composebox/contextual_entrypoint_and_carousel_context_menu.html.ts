// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';
import {getHtml as getToolChipsHtml} from './contextual_entrypoint_and_carousel_tool_chips.html.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
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
        @open-image-upload="${this.openImageUpload_}"
        @open-file-upload="${this.openFileUpload_}"
        @add-tab-context="${this.addTabContext_}"
        @delete-tab-context="${this.onDeleteFile_}"
        @tool-click="${this.onToolClick_}"
        @deep-search-click="${this.handleDeepSearchClick_}"
        @create-image-click="${this.handleImageGenClick_}"
        .showModelPicker="${this.showModelPicker}"
        .inputState="${this.inputState}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions}"
        .inCreateImageMode="${
            this.activeTool_ === ComposeboxToolMode.kImageGen}"
        .hasImageFiles="${this.hasImageFiles()}"
        .disabledTabIds="${this.addedTabsIds_}"
        .fileNum="${this.files_.size}"
        ?upload-button-disabled="${this.uploadButtonDisabled_}"
        ?show-context-menu-description="${
            this.shouldShowDescription_()}"
        glif-animation-state="${this.contextMenuGlifAnimationState}">
    </cr-composebox-contextual-entrypoint-and-menu>
  ` : html`
    ${this.shouldHideEntrypointButton_ ? '' : html`
      <cr-composebox-contextual-entrypoint-button
          id="contextEntrypoint"
          part="composebox-entrypoint"
          exportparts="context-menu-entrypoint-icon"
          class="upload-button no-overlap"
          ?upload-button-disabled="${this.uploadButtonDisabled_}"
          ?show-context-menu-description="${
                this.shouldShowDescription_()}"
          glif-animation-state="${this.contextMenuGlifAnimationState}">
      </cr-composebox-contextual-entrypoint-button>
    `}
  `}
  ${this.searchboxLayoutMode === 'Compact' && this.showVoiceSearch ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon"
        part="voice-icon" iron-icon="cr:mic"
        @click="${this.onVoiceSearchClick_}"
        title="${this.i18n('voiceSearchButtonLabel')}">
    </cr-icon-button>
  ` : ''}
  ${this.shouldShowToolChipsForTallMode_ ? getToolChipsHtml.bind(this)() : ''}
  ${this.searchboxLayoutMode === 'TallTopContext' ? html`
    ${this.showVoiceSearch ? html`
      <cr-icon-button id="voiceSearchButton" class="voice-icon"
          part="voice-icon" iron-icon="cr:mic"
          @click="${this.onVoiceSearchClick_}"
          title="${this.i18n('voiceSearchButtonLabel')}">
      </cr-icon-button>
    ` : ''}
    ${this.submitButtonShown ? html`
      <slot name="submit-button"></slot>
    ` : ''}
  ` : ''}
</div>
  `;
  // clang-format on
}
