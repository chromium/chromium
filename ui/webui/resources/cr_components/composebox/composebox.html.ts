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
  ${this.showErrorScrim_ ? html`
    <div id="errorScrim">
      <p>${this.errorMessage_}</p>
      <cr-button id="dismissErrorButton"
          @click="${this.onDismissErrorButtonClick_}">
        <cr-icon icon="cr:close" slot="prefix-icon"></cr-icon>
        <div>$i18n{dismissButton}</div>
      </cr-button>
    </div>
  `: ''}
  <div id="composebox" @keydown="${this.onKeydown_}"
      ?inert=${this.showErrorScrim_}
      @focusin=${this.handleComposeboxFocusIn_}
      @focusout=${this.handleComposeboxFocusOut_}>
    <div id="inputContainer">
      <div id="textContainer" part="text-container">
        <div id="iconContainer" part="icon-container">
          <div id="aimIcon"></div>
        </div>
        <div id="inputWrapper">
          <textarea autocomplete="off" id="input"
            type="search" spellcheck="false"
            placeholder="${this.inputPlaceholder_}"
            part="input"
            @input=${this.handleInput_}
            @scroll="${this.handleScroll_}"
            @focusin="${this.handleInputFocusIn_}"></textarea>
          ${this.shouldShowSmartComposeInlineHint_() ? html`
            <div id="smartCompose">
              <!-- Comments in between spans to eliminate spacing between
                   spans -->
              <span id="invisibleText">${this.input_}</span><!--
              --><span id="ghostText">${this.smartComposeInlineHint_}</span><!--
              --><span id="tabChip">Tab</span>
            </div>
          `: ''}
        </div>
      </div>
      ${this.showFileCarousel_ ? html`
        <ntp-composebox-file-carousel
          id="carousel"
          .files=${Array.from(this.files_.values())}
          @delete-file=${this.onDeleteFile_}>
      </ntp-composebox-file-carousel>
      <div class="carousel-divider"></div> ` : ''}
      <ntp-composebox-dropdown
          id="matches"
          role="listbox"
          .result="${this.result_}"
          .selectedMatchIndex="${this.selectedMatchIndex_}"
          @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
          @match-focusin="${this.onMatchFocusin_}"
          @match-click="${this.onMatchClick_}"
          ?hidden="${!this.showDropdown_}">
      </ntp-composebox-dropdown>
    ${this.contextMenuEnabled_ ? html`
      <composebox-context-menu-entrypoint id="contextEntrypoint"
          class="upload-icon no-overlap"
          .tabSuggestions="${this.tabSuggestions_}"
          @open-image-upload="${this.openImageUpload_}"
          @open-file-upload="${this.openFileUpload_}"
          @add-tab-context="${this.addTabContext_}"
          @refresh-tab-suggestions="${this.refreshTabSuggestions_}"
          ?inputs-disabled="${this.inputsDisabled_}">
      </composebox-context-menu-entrypoint>
    ` : html`
      <div id="uploadContainer" class="icon-fade">
          <cr-icon-button
              class="upload-icon no-overlap"
              id="imageUploadButton"
              iron-icon="composebox:imageUpload"
              title="$i18n{composeboxImageUploadButtonTitle}"
              .disabled="${this.inputsDisabled_}"
              @click="${this.openImageUpload_}">
          </cr-icon-button>
          ${this.composeboxShowPdfUpload_ ? html`
          <cr-icon-button
              class="upload-icon no-overlap"
              id="fileUploadButton"
              iron-icon="composebox:fileUpload"
              title="$i18n{composeboxPdfUploadButtonTitle}"
              .disabled="${this.inputsDisabled_}"
              @click="${this.openFileUpload_}">
          </cr-icon-button>
          `: ''}
      </div>
    `}
    </div>
    <cr-icon-button
        class="action-icon icon-fade icon-clear"
        id="cancelIcon"
        part="action-icon cancel-icon"
        title="${this.computeCancelButtonTitle_()}"
        @click="${this.onCancelClick_}">
    </cr-icon-button>
    <!-- A seperate container is needed for the submit button so the
       expand/collapse animation can be applied without affecting the submit
       button enabled/disabled state. -->
    <div id="submitContainer" class="icon-fade" part="submit">
      <cr-icon-button
        class="action-icon icon-arrow-upward"
        id="submitIcon"
        part="action-icon"
        title="${this.i18n('composeboxSubmitButtonTitle')}"
        @click="${this.submitQuery_}"
        ?disabled="${!this.submitEnabled_}"
        @focusin="${this.handleSubmitFocusIn_}">
      </cr-icon-button>
    </div>
  </div>
  <input type="file"
      accept="${this.imageFileTypes_}"
      id="imageInput"
      @change="${this.onFileChange_}"
      hidden>
  </input>
  <input type="file"
      accept="${this.attachmentFileTypes_}"
      id="fileInput"
      @change="${this.onFileChange_}"
      hidden>
  </input>
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
