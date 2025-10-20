// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div class="gradient gradient-outer-glow"></div>
  <div class="gradient"></div>
  <div class="background"></div>
  <ntp-error-scrim id="errorScrim"
    ?compact-mode="${this.realboxLayoutMode === 'Compact' &&
                     this.contextFilesSize_ === 0}"
    @error-scrim-visibility-changed="${this.onErrorScrimVisibilityChanged_}">
  </ntp-error-scrim>
  <div id="composebox" @keydown="${this.onKeydown_}"
      @focusin=${this.handleComposeboxFocusIn_}
      @focusout=${this.handleComposeboxFocusOut_}>
    <div id="inputContainer">
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
            @input=${this.handleInput_}
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
          .tabSuggestions_=${this.tabSuggestions_}
          entrypoint-name="Composebox"
          @add-tab-context="${this.addTabContext_}"
          @add-file-context="${this.addFileContext_}"
          @delete-context="${this.deleteContext_}"
          @on-file-validation-error="${this.onFileValidationError_}"
          @set-deep-search-mode="${this.setDeepSearchMode_}"
          @set-create-image-mode="${this.setCreateImageMode_}"
          @get-tab-preview="${this.getTabPreview_}"
          ?show-dropdown="${this.showDropdown_}"
          ?inputs-disabled="${this.inputsDisabled_}"
          ?show-context-menu-description="${this.showContextMenuDescription_}"
          realbox-layout-mode="${this.realboxLayoutMode}">
        <ntp-composebox-dropdown
            id="matches"
            part="dropdown"
            role="listbox"
            .result="${this.result_}"
            .selectedMatchIndex="${this.selectedMatchIndex_}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
            @match-focusin="${this.onMatchFocusin_}"
            @match-click="${this.onMatchClick_}"
            ?hidden="${!this.showDropdown_}"
            .lastQueriedInput=${this.lastQueriedInput_}>
        </ntp-composebox-dropdown>
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
    <!-- A seperate container is needed for the submit button so the
       expand/collapse animation can be applied without affecting the submit
       button enabled/disabled state. -->
    <div id="submitContainer" class="icon-fade" part="submit">
      <cr-icon-button
        class="action-icon icon-arrow-upward"
        id="submitIcon"
        part="action-icon submit-icon"
        title="${this.i18n('composeboxSubmitButtonTitle')}"
        @click="${this.submitQuery_}"
        ?disabled="${!this.submitEnabled_}"
        @focusin="${this.handleSubmitFocusIn_}">
      </cr-icon-button>
    </div>
  </div>
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
