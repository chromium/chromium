// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  const submitContainer = html`
    <div id="submitContainer" class="icon-fade" part="submit"
        slot="${this.ntpRealboxNextEnabled ? 'submit-button' : nothing}"
        tabindex="-1"
        @click="${this.submitQuery_}"
        @focusin="${this.handleSubmitFocusIn_}">
      <div id="submitOverlay" part="submit-overlay"></div>
      <cr-icon-button
        class="action-icon icon-arrow-upward"
        id="submitIcon"
        part="action-icon submit-icon"
        tabindex="0"
        title="${this.i18n('composeboxSubmitButtonTitle')}"
        ?disabled="${!this.submitEnabled_}">
      </cr-icon-button>
    </div>`;
  // clang-format off
  return html`<!--_html_template_start_-->
  <search-animated-glow
      animation-state="${this.animationState}"
      .entrypointName="${this.entrypointName}"
      .requiresVoice="${this.shouldShowVoiceSearch_()}"
      .transcript="${this.transcript_}"
      .receivedSpeech="${this.receivedSpeech_}"
      exportparts="composebox-background">
  </search-animated-glow>
  <ntp-error-scrim id="errorScrim" part="error-scrim"
    ?compact-mode="${this.searchboxLayoutMode === 'Compact' &&
                     this.contextFilesSize_ === 0}"
    @error-scrim-visibility-changed="${this.onErrorScrimVisibilityChanged_}">
  </ntp-error-scrim>
  <div id="composebox" @keydown="${this.onKeydown_}"
      @focusin="${this.handleComposeboxFocusIn_}"
      @focusout="${this.handleComposeboxFocusOut_}"
      @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
      @dragover="${this.dragAndDropHandler_.handleDragOver}"
      @dragleave="${this.dragAndDropHandler_.handleDragLeave}"
      @drop="${this.dragAndDropHandler_.handleDrop}"
      @paste="${this.onPaste_}">
    <div id="inputContainer" part="input-container">
      <div id="textContainer" part="text-container">
        <div id="iconContainer" part="icon-container">
          <div id="aimIcon"></div>
        </div>
        <div id="inputWrapper">
          <textarea
            aria-expanded="${this.showDropdown_}" aria-controls="matches"
            role="combobox" autocomplete="off" id="input"
            type="search" spellcheck="false"
            placeholder="${this.inputPlaceholder_}"
            part="input"
            .value="${this.input_}"
            @input="${this.handleInput_}"
            @scroll="${this.handleScroll_}"
            @focusin="${this.handleInputFocusIn_}"
            @focusout="${this.handleInputFocusOut_}"></textarea>
          ${this.shouldShowSmartComposeInlineHint_() ? html`
            <div id="smartCompose">
              <!-- Comments in between spans to eliminate spacing between
                   spans -->
              <span id="invisibleText">${this.input_}</span><!--
              --><span id="ghostText">${this.smartComposeInlineHint_}</span><!--
              --><span id="tabChip">${this.i18n('composeboxSmartComposeTabTitle')}</span>
            </div>
          `: ''}
        </div>
      </div>
      <contextual-entrypoint-and-carousel id="context" part="context-entrypoint"
          class="${this.carouselOnTop_ && this.isCollapsible ? 'icon-fade' : ''}"
          exportparts="context-menu-entrypoint-icon, cr-composebox-file-carousel, upload-container, voice-icon, carousel-divider, carousel-container"
          .tabSuggestions="${this.tabSuggestions_}"
          .entrypointName="${this.entrypointName ? this.entrypointName : 'Composebox'}"
          @add-tab-context="${this.addTabContext_}"
          @open-voice-search="${this.openAimVoiceSearch_}"
          @add-file-context="${this.addFileContext_}"
          @delete-context="${this.deleteContext_}"
          @on-file-validation-error="${this.onFileValidationError_}"
          @set-deep-search-mode="${this.setDeepSearchMode_}"
          @set-create-image-mode="${this.setCreateImageMode_}"
          @get-tab-preview="${this.getTabPreview_}"
          @context-menu-container-click="${this.searchboxLayoutMode === 'Compact' ?  nothing : this.focusInput}"
          ?show-dropdown="${this.showDropdown_}"
          ?show-context-menu-description="${this.showContextMenuDescription_}"
          searchbox-layout-mode="${this.searchboxLayoutMode}"
          ?carousel-on-top_="${this.carouselOnTop_}"
          ?show-voice-search="${this.shouldShowVoiceSearch_()}"
          .submitButtonShown="${this.ntpRealboxNextEnabled && this.submitEnabled_ && this.showSubmit_}">
        <cr-composebox-dropdown
            id="matches"
            part="dropdown"
            exportparts="match-text-container"
            role="listbox"
            .result="${this.result_}"
            .selectedMatchIndex="${this.selectedMatchIndex_}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
            @match-focusin="${this.onMatchFocusin_}"
            @match-click="${this.onMatchClick_}"
            ?hidden="${!this.showDropdown_}"
            .lastQueriedInput="${this.lastQueriedInput_}">
        </cr-composebox-dropdown>
        ${this.ntpRealboxNextEnabled ? submitContainer : ''}
      </contextual-entrypoint-and-carousel>
    </div>
    <!-- A seperate container is needed for the submit button so the
    expand/collapse animation can be applied without affecting the submit
    button enabled/disabled state. -->
    <div id="cancelContainer" class="icon-fade" part="cancel">
      <cr-icon-button
          class="action-icon icon-clear"
          id="cancelIcon"
          part="action-icon cancel-icon"
          title="${this.computeCancelButtonTitle_()}"
          @click="${this.onCancelClick_}"
          ?disabled="${this.isCollapsible && !this.submitEnabled_}">
      </cr-icon-button>
    </div>
    <cr-icon-button
        class="action-icon"
        id="lensIcon"
        part="action-icon lens-icon"
        title="${this.i18n('lensSearchButtonLabel')}"
        @click="${this.onLensClick_}"
        ?disabled="${this.lensButtonDisabled_}"
        @mousedown="${this.onLensIconMouseDown_}">
    </cr-icon-button>
    <!-- Elements rendered under the input container. -->
    <!-- TODO: Move the submit button and Lens icon into this slot. -->
    <slot name="footer"></slot>
    <!-- A seperate container is needed for the submit button so the
       expand/collapse animation can be applied without affecting the submit
       button enabled/disabled state. -->
    ${this.ntpRealboxNextEnabled ? '' : submitContainer}
  </div>
  <cr-composebox-voice-search id="voiceSearch"
      @voice-search-cancel="${this.onVoiceSearchClose_}"
      @voice-search-final-result="${this.onVoiceSearchFinalResult_}"
      @transcript-update="${this.onTranscriptUpdate_}"
      @speech-received="${this.onSpeechReceived_}"
      exportparts="voice-close-button">
  </cr-composebox-voice-search>
  ${this.shouldShowSuggestionActivityLink_() ? html`
    <div id="suggestionActivity">
      <localized-link
        localized-string="${this.i18nAdvanced('suggestionActivityLink')}">
      </localized-link>
    </div>
  `: ''}
<!--_html_template_end_-->`;
  // clang-format on
}
