// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
  const showDescription =
      this.showContextMenuDescription_ && !this.shouldShowRecentTabChip_;
  const toolChipsVisible = this.shouldShowRecentTabChip_ ||
      this.inDeepSearchMode_ || this.inCreateImageMode_;
  const toolChips = html`
        ${
      this.shouldShowRecentTabChip_ ? html`
        <composebox-recent-tab-chip id="recentTabChip"
            class="upload-button"
            .recentTab="${this.recentTabForChip_}"
            @add-tab-context="${this.addTabContext_}">
        </composebox-recent-tab-chip>
        ` :
                                      ''}
      ${
      this.shouldShowLensSearchChip_ ? html`
        <cr-composebox-lens-search id="lensSearchChip" class="upload-button">
        </cr-composebox-lens-search>
      ` :
                                       ''}
        <cr-composebox-tool-chip
            icon="composebox:deepSearch"
            label="${this.i18n('deepSearch')}"
            remove-chip-aria-label="${
      this.i18n('removeToolChipAriaLabel', this.i18n('deepSearch'))}"
            ?visible="${this.inDeepSearchMode_}"
            @click="${this.onDeepSearchClick_}">
        </cr-composebox-tool-chip>
        <cr-composebox-tool-chip
            icon="composebox:nanoBanana"
            label="${this.i18n('createImages')}"
            remove-chip-aria-label="${
      this.i18n('removeToolChipAriaLabel', this.i18n('createImages'))}"
            ?visible="${this.inCreateImageMode_}"
            @click="${this.onCreateImageClick_}">
        </cr-composebox-tool-chip>
  `;

  const voiceSearchButton = html`
          <cr-icon-button id="voiceSearchButton" class="voice-icon"
              part="voice-icon" iron-icon="cr:mic"
              @click="${this.onVoiceSearchClick_}"
              title="${this.i18n('voiceSearchButtonLabel')}">
          </cr-icon-button>
        `;

  const contextMenu = html`
      <div class="context-menu-container" part="context-menu-and-tools"
          @mousedown="${this.preventFocus_}"
          @click="${this.onContextMenuContainerClick_}">
        <cr-composebox-context-menu-entrypoint id="contextEntrypoint"
            part="composebox-entrypoint"
            exportparts="context-menu-entrypoint-icon"
            class="upload-button no-overlap"
            .tabSuggestions="${this.tabSuggestions}"
            .entrypointName="${this.entrypointName}"
            @open-image-upload="${this.openImageUpload_}"
            @open-file-upload="${this.openFileUpload_}"
            @add-tab-context="${this.addTabContext_}"
            @deep-search-click="${this.onDeepSearchClick_}"
            @create-image-click="${this.onCreateImageClick_}"
            @delete-tab-context="${this.onDeleteFile_}"
            .inCreateImageMode="${this.inCreateImageMode_}"
            .hasImageFiles="${this.hasImageFiles()}"
            .hideEntrypointButton="${this.shouldHideEntrypointButton_}"
            .disabledTabIds="${this.addedTabsIds_}"
            .fileNum="${this.files_.size}"
            .searchboxLayoutMode="${this.searchboxLayoutMode}"
            ?inputs-disabled="${this.inputsDisabled_}"
            ?show-context-menu-description="${showDescription}"
            glif-animation-state="${this.contextMenuGlifAnimationState}">
        </cr-composebox-context-menu-entrypoint>
        ${
      this.searchboxLayoutMode === 'Compact' && this.showVoiceSearch ?
          voiceSearchButton :
          ''}
        ${this.shouldShowToolChips_ ? toolChips : ''}
        ${
      this.searchboxLayoutMode === 'TallTopContext' && this.showVoiceSearch ?
          voiceSearchButton :
          ''}
        ${
      this.searchboxLayoutMode === 'TallTopContext' && this.submitButtonShown ?
          html`<slot name="submit-button"></slot>` :
          ''}
      </div>
  `;

  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.searchboxLayoutMode === 'Compact' ? contextMenu : ''}
  <div part="carousel-container">
    ${this.showFileCarousel_ ? html`
      <cr-composebox-file-carousel
        part="cr-composebox-file-carousel"
        exportparts="thumbnail"
        id="carousel"
        class="${this.carouselOnTop_ ? 'top' : ''}"
        .files="${Array.from(this.files_.values())}"
        @delete-file="${this.onDeleteFile_}">
      </cr-composebox-file-carousel> ` : ''}
    ${this.submitButtonShown && (this.searchboxLayoutMode === 'Compact' || this.searchboxLayoutMode === 'TallBottomContext') ?
      html`<slot name="submit-button"></slot>` :
      ''}
  </div>
  ${this.searchboxLayoutMode === 'TallTopContext' ? contextMenu : ''}
  ${this.shouldShowDivider_ ? html`
    <div class="carousel-divider" part="carousel-divider"></div>
  ` : ''}
  <!-- Suggestions are slotted in from the parent component. -->
  <slot id="dropdownMatches"></slot>
  ${this.searchboxLayoutMode === 'Compact' && toolChipsVisible && this.entrypointName === 'Realbox' ? html`
    <div class="context-menu-container" id="toolChipsContainer"
        part="tool-chips-container">${toolChips}</div>
  ` : ''}
  ${this.searchboxLayoutMode === 'TallBottomContext' || this.searchboxLayoutMode === '' ? html`
    ${this.contextMenuEnabled_ ? contextMenu : html`
      <div part="upload-container" id="uploadContainer" class="icon-fade">
          <cr-icon-button
              class="upload-button no-overlap"
              id="imageUploadButton"
              iron-icon="composebox:imageUpload"
              title="${this.i18n('composeboxImageUploadButtonTitle')}"
              .disabled="${this.inputsDisabled_}"
              @click="${this.openImageUpload_}">
          </cr-icon-button>
          ${this.composeboxShowPdfUpload_ ? html`
          <cr-icon-button
              class="upload-button no-overlap"
              id="fileUploadButton"
              iron-icon="composebox:fileUpload"
              title="${this.i18n('composeboxPdfUploadButtonTitle')}"
              .disabled="${this.inputsDisabled_}"
              @click="${this.openFileUpload_}">
          </cr-icon-button>
          `: ''}
      </div>
    `}
  `: ''}
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
  ${(this.searchboxLayoutMode === 'TallBottomContext' || !this.searchboxLayoutMode) && this.showVoiceSearch ?
          voiceSearchButton :
          ''}
<!--_html_template_end_-->`;
  // clang-format on
}
