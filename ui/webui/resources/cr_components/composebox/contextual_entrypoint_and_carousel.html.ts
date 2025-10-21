// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
  const showDescription = this.realboxLayoutMode !== 'Compact' &&
      this.showContextMenuDescription_ && !this.shouldShowRecentTabChip_;
  const toolChipsVisible = this.shouldShowRecentTabChip_ ||
      this.inDeepSearchMode_ || this.inCreateImageMode_;
  const toolChips = html`
        ${
      this.shouldShowRecentTabChip_ ? html`
        <composebox-recent-tab-chip id="recentTabChip"
            class="upload-icon"
            .recentTab_=${this.tabSuggestions_[0]}
            .inputsDisabled_=${this.inputsDisabled_}
            @add-tab-context="${this.addTabContext_}">
        </composebox-recent-tab-chip>
        ` :
                                      ''}
        <composebox-tool-chip
            icon="composebox:deepSearch"
            label="${this.i18n('deepSearch')}"
            ?visible="${this.inDeepSearchMode_}"
            @click="${this.onDeepSearchClick_}">
        </composebox-tool-chip>
        <composebox-tool-chip
            icon="composebox:nanoBanana"
            label="${this.i18n('createImages')}"
            ?visible="${this.inCreateImageMode_}"
            @click="${this.onCreateImageClick_}">
        </composebox-tool-chip>
  `;
  const contextMenu = html`
      <div class="context-menu-container">
        <composebox-context-menu-entrypoint id="contextEntrypoint"
            part="composebox-entrypoint"
            class="upload-icon no-overlap"
            .tabSuggestions_=${this.tabSuggestions_}
            .entrypointName="${this.entrypointName}"
            @open-image-upload="${this.openImageUpload_}"
            @open-file-upload="${this.openFileUpload_}"
            @add-tab-context="${this.addTabContext_}"
            @deep-search-click="${this.onDeepSearchClick_}"
            @create-image-click="${this.onCreateImageClick_}"
            .inCreateImageMode="${this.inCreateImageMode_}"
            .hasImageFiles="${this.hasImageFiles()}"
            .disabledTabIds="${this.addedTabsIds_}"
            .fileNum="${this.files_.size}"
            ?inputs-disabled="${this.inputsDisabled_}"
            ?show-context-menu-description="${showDescription}">
        </composebox-context-menu-entrypoint>
        ${this.realboxLayoutMode !== 'Compact' ? toolChips : ''}
      </div>
  `;

  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.realboxLayoutMode === 'Compact' ? contextMenu : ''}
  ${this.showFileCarousel_ ? html`
    <ntp-composebox-file-carousel
      part="composebox-file-carousel"
      id="carousel"
      .files=${Array.from(this.files_.values())}
      @delete-file=${this.onDeleteFile_}>
    </ntp-composebox-file-carousel> ` : ''}
  ${this.realboxLayoutMode === 'TallTopContext' ? contextMenu : ''}
  ${this.showDropdown && (this.showFileCarousel_ || this.realboxLayoutMode === 'TallTopContext') ? html`
  <div class="carousel-divider" part="carousel-divider"></div>` : ''}
  <!-- Suggestions are slotted in from the parent component. -->
  <slot id="dropdownMatches"></slot>
  ${this.realboxLayoutMode === 'Compact' && toolChipsVisible ? html`
    <div class="context-menu-container" id='toolChipsContainer'
        part="tool-chips-container">${toolChips}</div>
  ` : ''}
  ${this.realboxLayoutMode === 'TallBottomContext' || this.realboxLayoutMode === '' ? html`
    ${this.contextMenuEnabled_ ? contextMenu : html`
      <div id="uploadContainer" class="icon-fade">
          <cr-icon-button
              class="upload-icon no-overlap"
              id="imageUploadButton"
              iron-icon="composebox:imageUpload"
              title="${this.i18n('composeboxImageUploadButtonTitle')}"
              .disabled="${this.inputsDisabled_}"
              @click="${this.openImageUpload_}">
          </cr-icon-button>
          ${this.composeboxShowPdfUpload_ ? html`
          <cr-icon-button
              class="upload-icon no-overlap"
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
<!--_html_template_end_-->`;
  // clang-format on
}
