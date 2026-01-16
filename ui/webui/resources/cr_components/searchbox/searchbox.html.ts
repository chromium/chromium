// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  const compactLayout = this.ntpRealboxNextEnabled && this.searchboxLayoutMode === 'Compact';

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
        @set-deep-search-mode="${this.setDeepSearchMode_}"
        @set-create-image-mode="${this.setCreateImageMode_}"
        @open-voice-search="${this.onVoiceSearchClick_}"
        @get-tab-preview="${this.getTabPreview_}"
        @context-menu-container-click="${this.onContextMenuContainerClick_}"
        ?show-dropdown="${this.dropdownIsVisible}"
        ?show-recent-tab-chip="${this.computeShowRecentTabChip_()}"
        ?show-voice-search="${this.shouldShowVoiceSearch_}"
        searchbox-layout-mode="${this.searchboxLayoutMode}"
        context-menu-glif-animation-state="${this.contextMenuGlifAnimationState}">
      ${!compactLayout ? dropdown : nothing}
    </contextual-entrypoint-and-carousel>`;

  const inputContent = html`
  <cr-searchbox-icon id="icon" .match="${this.selectedMatch_}"
      default-icon="${this.searchboxIcon_}" in-searchbox>
  </cr-searchbox-icon>
  ${this.showThumbnail ? html`
    <div id="thumbnailContainer">
      <cr-searchbox-thumbnail id="thumbnail" thumbnail-url_="${this.thumbnailUrl_}"
          ?is-deletable_="${this.isThumbnailDeletable_}"
          @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
          role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
          tabindex="${this.getThumbnailTabindex_()}">
      </cr-searchbox-thumbnail>
    </div>
  ` : nothing}
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
  `}`;

  const voiceSearchButton = html`
    ${this.searchboxVoiceSearchEnabled_ ? html`
      <div class="searchbox-icon-button-container voice">
        <button id="voiceSearchButton" class="searchbox-icon-button"
            @click="${this.onVoiceSearchClick_}"
            title="${this.i18n('voiceSearchButtonLabel')}">
        </button>
      </div>
    ` : nothing}`;

  const lensSearchButton = html`
    ${this.searchboxLensSearchEnabled_ ? html`
      <div class="searchbox-icon-button-container lens">
        <button id="lensSearchButton" class="searchbox-icon-button lens"
            @click="${this.onLensSearchClick_}"
            title="${this.i18n('lensSearchButtonLabel')}">
        </button>
      </div>
    ` : nothing}`;

  const composeButton = html`
    ${this.composeButtonEnabled ? html`
      <cr-searchbox-compose-button id="composeButton"
          @compose-click="${this.onComposeButtonClick_}">
      </cr-searchbox-compose-button>
    ` : nothing}`;

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
          ?compact-mode="${this.searchboxLayoutMode === 'Compact'}">
      </ntp-error-scrim>
      <search-animated-glow animation-state="${this.animationState}" part="animated-glow">
      </search-animated-glow>
      ${compactLayout ? html`
        <div id="inputInnerContainer">
          <div class="contextualEntrypointContainer contextualEntrypointContainerCompact">
            ${contextualEntrypoint}
          </div>
          ${inputContent}
          ${voiceSearchButton}
          ${lensSearchButton}
          ${composeButton}
        </div>
        <div class="dropdownContainer">
          ${dropdown}
          ${this.recentTabForChip_ && this.dropdownIsVisible && this.isInputEmpty() ? html`
          <div id="recentTabChipContainer">
            <composebox-recent-tab-chip
                .recentTab="${this.recentTabForChip_}"
                @add-tab-context="${this.addTabContext_}">
            </composebox-recent-tab-chip>
          </div>
          ` : nothing}
        </div>
      ` : html`
        <div id="inputInnerContainer">
          ${inputContent}
          ${composeButton}
        </div>
        <div id="inputInnerBottomContainer">
          <div class="contextualEntrypointContainer">
            ${contextualEntrypoint}
          </div>
          ${voiceSearchButton}
          ${lensSearchButton}
        </div>
      `}
    ` :
    html`
      <div id="inputInnerContainer">
        ${inputContent}
        ${voiceSearchButton}
        ${lensSearchButton}
        ${composeButton}
      </div>
      ${dropdown}
    `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
