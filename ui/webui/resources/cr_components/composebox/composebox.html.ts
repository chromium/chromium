// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';
import {getHtml as getContextMenuHtml} from './composebox_context_menu.html.js';
import {getHtml as getSubmitButtonHtml} from './composebox_submit_button.html.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${!this.disableComposeboxAnimation ? html`
    <search-animated-glow
        animation-state="${this.animationState}"
        .entrypointName="${this.entrypointName}"
        .requiresVoice="${this.shouldShowVoiceSearchAnimation_()}"
        .transcript="${this.transcript_}"
        .receivedSpeech="${this.receivedSpeech_}"
        exportparts="composebox-background">
    </search-animated-glow>
  ` : ''}
    <ntp-error-scrim id="errorScrim" part="error-scrim"
        ?compact-mode="${this.searchboxLayoutMode === 'Compact' &&
                         this.files.size === 0}"
        .errorMessage="${this.errorMessage}"
        @dismiss-error-scrim="${this.onDismissErrorScrim_}">
    </ntp-error-scrim>
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
        @keydown="${this.onKeydown_}"
        @focusin="${this.onComposeboxFocusin_}"
        @focusout="${this.onComposeboxFocusout_}"
        @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
        @dragover="${this.dragAndDropHandler_.handleDragOver}"
        @dragleave="${this.dragAndDropHandler_.handleDragLeave}"
        @drop="${this.dragAndDropHandler_.handleDrop}"
        @paste="${this.onPaste_}">
    <div id="inputContainer" part="input-container">
      <cr-composebox-input id="composeboxInput"
          exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
          .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
          .showDropdown="${this.showDropdown_}"
          .inputPlaceholder="${this.inputPlaceholder}"
          .input="${this.input}"
          .smartComposeInlineHint="${this.smartComposeInlineHint_}"
          .isCollapsible="${this.isCollapsible}"
          .submitEnabled="${this.submitEnabled_}"
          .entrypointName="${this.entrypointName}"
          .cancelButtonTitle="${this.computeCancelButtonTitle_()}"
          @input-input="${this.onInputInput_}"
          @input-focusin="${this.onInputFocusin_}"
          @cancel-click="${this.onCancelClick_}">
      </cr-composebox-input>
      <div id="context" part="context-entrypoint"
          class="${this.carouselOnTop_ && this.isCollapsible ? 'icon-fade' : ''}">
        <cr-composebox-file-inputs id="fileInputs"
            @file-change="${this.onFileChange_}"
            .disableFileInputs="${this.shouldDisableFileInputs_()}">
          ${this.searchboxLayoutMode === 'Compact' && !this.isOmniboxInCompactMode_ ?
            getContextMenuHtml.bind(this)()
          : ''}
          <div id="carouselContainer" part="carousel-container">
            <div class="carousel-container-inner">
              ${this.showFileCarousel_ ? html`
                <cr-composebox-file-carousel
                  part="cr-composebox-file-carousel"
                  exportparts="thumbnail, thumbnail-title"
                  id="carousel"
                  class="${this.carouselOnTop_ ? 'top' : ''}"
                  .files="${Array.from(this.files.values())}"
                  ?enable-scrolling="${this.enableCarouselScrolling}"
                  @delete-file="${this.onDeleteFile_}">
                </cr-composebox-file-carousel> ` : ''}
                ${this.searchboxLayoutMode === 'Compact' && this.inToolMode_ ? html`
                <div class="context-menu-container" id="toolChipsContainer"
                    part="tool-chips-container">
                    <cr-composebox-tool-chip
                      exportparts="tool-chip-label"
                      .inputState="${this.inputState}"
                      .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
                      @tool-click="${this.onToolClick_}">
                    </cr-composebox-tool-chip>
                </div>
                ` : ''}
            </div>
            ${this.shouldShowSubmitButton_ && this.searchboxLayoutMode === 'Compact' ?
              getSubmitButtonHtml.bind(this)() :
              ''}
          </div>
          ${this.searchboxLayoutMode === 'TallTopContext' ?
            getContextMenuHtml.bind(this)()
          : ''}
          ${this.shouldShowDivider_ ? html`
            <div class="carousel-divider" part="carousel-divider"></div>
          ` : ''}
          <cr-composebox-dropdown
              id="matches"
              part="dropdown"
              exportparts="match-text-container"
              role="listbox"
              .result="${this.result_}"
              .selectedMatchIndex="${this.selectedMatchIndex_}"
              .maxSuggestions="${this.maxSuggestions}"
              .toolMode="${this.inputState?.activeTool}"
              @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
              @match-focusin="${this.onMatchFocusin_}"
              @match-click="${this.onMatchClick_}"
              ?hidden="${!this.showDropdown_ || !this.dropdownNeeded}"
              .lastQueriedInput="${this.lastQueriedInput_}">
          </cr-composebox-dropdown>
          ${this.searchboxLayoutMode === 'TallBottomContext' || this.searchboxLayoutMode === '' || this.isOmniboxInCompactMode_ ? html`
            ${this.contextMenuEnabled_ ? getContextMenuHtml.bind(this)() : ''}
          `: ''}
          ${this.shouldShowVoiceSearchAtBottom_() ? html`
            <cr-icon-button id="voiceSearchButton" class="voice-icon" part="voice-icon"
                iron-icon="cr:mic" @click="${this.onVoiceSearchButtonClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </cr-icon-button>
          ` : ''}
          ${this.shouldShowSubmitButton_ && this.searchboxLayoutMode === 'TallBottomContext' ?
              getSubmitButtonHtml.bind(this)() :
              ''}
        </cr-composebox-file-inputs>
      </div>
    </div>
    ${this.showLensButton ? html`<cr-icon-button
        class="action-icon"
        id="lensIcon"
        part="action-icon lens-icon"
        title="${this.i18n('lensSearchButtonLabel')}"
        @click="${this.onLensClick_}"
        ?disabled="${this.lensButtonDisabled}"
        @mousedown="${this.onLensIconMousedown_}">
    </cr-icon-button>` : ''}
    <!-- Elements rendered under the input container. -->
    <!-- TODO: Move the submit button and Lens icon into this slot. -->
    <slot name="footer"></slot>
    <!-- A seperate container is needed for the submit button so the
       expand/collapse animation can be applied without affecting the submit
       button enabled/disabled state. -->
    ${!this.searchboxNextEnabled ? getSubmitButtonHtml.bind(this)() : ''}
  </div>
  ${this.shouldShowVoiceSearch_() ? html`
    <cr-composebox-voice-search id="voiceSearch"
        @voice-search-cancel="${this.onVoiceSearchCancel_}"
        @voice-search-final-result="${this.onVoiceSearchFinalResult_}"
        @voice-search-error="${this.onVoiceSearchError_}"
        @transcript-update="${this.onTranscriptUpdate_}"
        @speech-received="${this.onSpeechReceived_}"
        exportparts="voice-close-button">
    </cr-composebox-voice-search>
  ` : ''}
  ${this.shouldShowSuggestionActivityLink_()
      && this.suggestionActivityEnabled ? html`
    <div id="suggestionActivity">
      <localized-link
        localized-string="${this.i18nAdvanced('suggestionActivityLink')}">
      </localized-link>
    </div>
  `: ''}
<!--_html_template_end_-->`;
  // clang-format on
}
