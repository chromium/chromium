// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  const dropdown = html`
    <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
        class="${!this.ntpRealboxNextEnabled ? 'dropdownContainer' : nothing}"
        exportparts="dropdown-content"
        role="listbox" .result="${this.result_}"
        selected-match-index="${this.selectedMatchIndex_}"
        @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
        ?can-show-secondary-side="${this.canShowSecondarySide}"
        ?had-secondary-side="${this.hadSecondarySide}"
        @had-secondary-side-changed="${this.onHadSecondarySideChanged_}"
        ?has-secondary-side="${this.hasSecondarySide}"
        @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
        @match-focusin="${this.onMatchFocusin_}"
        @match-click="${this.onMatchClick_}"
        ?hidden="${!this.dropdownIsVisible}"
        ?show-thumbnail="${this.showThumbnail}">
    </cr-searchbox-dropdown>`;

  const contextualEntrypoint = html`
    <contextual-entrypoint-and-carousel id="context"
        part="contextual-entrypoint-and-carousel"
        exportparts="composebox-entrypoint, context-menu-entrypoint-icon, voice-icon, context-menu-and-tools"
        .tabSuggestions="${this.tabSuggestions_}"
        entrypoint-name="Realbox"
        @add-tab-context="${this.addTabContext_}"
        @add-file-context="${this.addFileContext_}"
        @on-file-validation-error="${this.onFileValidationError_}"
        @set-tool-mode="${this.onSetToolMode_}"
        @model-click="${this.onModelClick_}"
        @get-tab-preview="${this.getTabPreview_}"
        @context-menu-container-click="${this.onContextMenuContainerClick_}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
        @context-menu-closed="${this.onContextMenuClosed_}"
        @context-menu-opened="${this.onContextMenuOpened_}"
        ?show-dropdown="${this.dropdownIsVisible}"
        ?show-recent-tab-chip="${this.computeShowRecentTabChip_()}"
        .inputState="${this.inputState_}"
        ?show-model-picker="${this.showModelPicker_}"
        searchbox-layout-mode="${this.searchboxLayoutMode}"
        context-menu-glif-animation-state="${this.contextMenuGlifAnimationState}">
      ${!this.useCompactLayout_() ? dropdown : nothing}
    </contextual-entrypoint-and-carousel>`;

  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout_}"
    @keydown="${this.onInputWrapperKeydown_}"
    @dragenter="${this.dragAndDropHandler?.handleDragEnter}"
    @dragover="${this.dragAndDropHandler?.handleDragOver}"
    @dragleave="${this.dragAndDropHandler?.handleDragLeave}"
    @drop="${this.dragAndDropHandler?.handleDrop}">
  ${this.ntpRealboxNextEnabled ?
    html`
      <ntp-error-scrim id="errorScrim"
          ?compact-mode="${this.searchboxLayoutMode === 'Compact'}"
          .errorMessage="${this.errorMessage_}"
          @dismiss-error-scrim="${this.onErrorScrimDismissed_}">
      </ntp-error-scrim>
      <search-animated-glow animation-state="${this.animationState}" part="animated-glow">
      </search-animated-glow>
    ` : ''}
  <div id="inputInnerContainer" ?inert="${this.getInnerInputInert_()}">
    ${this.ntpRealboxNextEnabled && this.useCompactLayout_() ? html`
      <div class="contextualEntrypointContainer contextualEntrypointContainerCompact">
        ${contextualEntrypoint}
      </div>
    ` : ''}
    <cr-searchbox-icon id="icon" .match="${this.selectedMatch_}"
        default-icon="${this.searchboxIcon_}" in-searchbox>
    </cr-searchbox-icon>
    ${this.showThumbnail ? html`
      <div id="thumbnailContainer">
        <cr-searchbox-thumbnail id="thumbnail"
            thumbnail-url="${this.thumbnailUrl_}"
            ?is-deletable="${this.isThumbnailDeletable_}"
            @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
            role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
            tabindex="${this.getThumbnailTabindex_()}">
        </cr-searchbox-thumbnail>
      </div>
    ` : ''}
    ${this.multiLineEnabled ? html`
      <textarea id="input" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive_}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_(this.placeholderText)}"
          @copy="${this.onInputCutCopy_}"
          @cut="${this.onInputCutCopy_}" @focus="${this.onInputFocus_}"
          @focusout="${this.onInputFocusout_}"
          @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}" @mousedown="${this.onInputMouseDown_}"
          @paste="${this.onInputPaste_}"></textarea>
    ` : html`
      <input id="input" class="truncate" type="search" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive_}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_(this.placeholderText)}"
          @copy="${this.onInputCutCopy_}"
          @cut="${this.onInputCutCopy_}" @focus="${this.onInputFocus_}"
          @focusout="${this.onInputFocusout_}"
          @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}" @mousedown="${this.onInputMouseDown_}"
          @paste="${this.onInputPaste_}">
      </input>
    `}
    ${!this.ntpRealboxNextEnabled || this.useCompactLayout_() ? html`
      ${this.searchboxVoiceSearchEnabled_ ? html`
        <div class="searchbox-icon-button-container voice">
          <button id="voiceSearchButton" class="searchbox-icon-button"
              @click="${this.onVoiceSearchClick_}"
              title="${this.i18n('voiceSearchButtonLabel')}">
          </button>
        </div>
      ` : ''}
      ${this.searchboxLensSearchEnabled_ ? html`
        <div class="searchbox-icon-button-container lens">
          <button id="lensSearchButton" class="searchbox-icon-button lens"
              @click="${this.onLensSearchClick_}"
              title="${this.i18n('lensSearchButtonLabel')}">
          </button>
        </div>
      ` : ''};
    ` : ''}
    ${this.composeButtonEnabled ? html`
      <cr-searchbox-compose-button id="composeButton"
          @compose-click="${this.onComposeButtonClick_}">
      </cr-searchbox-compose-button>
    ` : ''}
  </div>
  ${this.ntpRealboxNextEnabled ? html`
    ${this.useCompactLayout_() ? html`
      <div class="dropdownContainer" ?inert="${this.errorMessage_}">
        ${dropdown}
        ${this.shouldShowRecentTabChipInDropdown_() ? html`
          <div id="recentTabChipContainer">
            <composebox-recent-tab-chip
                .recentTab="${this.recentTabForChip_}"
                @add-tab-context="${this.addTabContext_}">
            </composebox-recent-tab-chip>
          </div>
        ` : ''}
      </div>
    ` : html`
      <div id="inputInnerBottomContainer" ?inert="${this.errorMessage_}">
        <div class="contextualEntrypointContainer">
          ${contextualEntrypoint}
        </div>
        ${this.searchboxVoiceSearchEnabled_ ? html`
          <div class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                @click="${this.onVoiceSearchClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
        ` : ''}
        ${this.searchboxLensSearchEnabled_ ? html`
          <div class="searchbox-icon-button-container lens">
            <button id="lensSearchButton" class="searchbox-icon-button lens"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
        ` : ''};
      </div>
    `}
  ` : html`
    ${dropdown}
  `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
