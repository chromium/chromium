// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
  // clang-format off
  // eslint-disable-next-line @webui-eslint/lit-element-template-structure
  const toolChips = html`
    ${this.shouldShowRecentTabChip_ ? html`
      <composebox-recent-tab-chip id="recentTabChip"
          class="upload-button contextual-chip"
          .recentTab="${this.recentTabForChip_}"
          @add-tab-context="${this.addTabContext_}">
      </composebox-recent-tab-chip>
    ` : ''}
    ${this.shouldShowLensSearchChip_ ? html`
      <cr-composebox-lens-search id="lensSearchChip"
          class="upload-button contextual-chip">
      </cr-composebox-lens-search>
    ` : ''}
    ${this.activeTool_ === ComposeboxToolMode.kDeepSearch ? html`
      <cr-composebox-tool-chip
          id="deepSearchChip"
          exportparts="tool-chip-label"
          icon="composebox:deepSearch"
          label="${this.getToolChipLabel_(ComposeboxToolMode.kDeepSearch)}"
          remove-chip-aria-label="${
          this.i18n(
              'removeToolChipAriaLabel',
              this.getToolChipLabel_(ComposeboxToolMode.kDeepSearch))}"
          ?visible="${true}"
          @click="${this.handleDeepSearchClick_}">
      </cr-composebox-tool-chip>
    ` : ''}
    ${this.activeTool_ === ComposeboxToolMode.kImageGen ? html`
      <cr-composebox-tool-chip
          id="nanoBananaChip"
          exportparts="tool-chip-label"
          icon="composebox:nanoBanana"
          label="${this.getToolChipLabel_(ComposeboxToolMode.kImageGen)}"
          remove-chip-aria-label="${
          this.i18n(
              'removeToolChipAriaLabel',
              this.getToolChipLabel_(ComposeboxToolMode.kImageGen))}"
          ?visible="${true}"
          @click="${this.handleImageGenClick_}">
      </cr-composebox-tool-chip>
    ` : ''}
    ${this.activeTool_ === ComposeboxToolMode.kCanvas ? html`
      <cr-composebox-tool-chip
          id="canvasChip"
          exportparts="tool-chip-label"
          icon="composebox:canvas"
          label="${this.getToolChipLabel_(ComposeboxToolMode.kCanvas)}"
          remove-chip-aria-label="${
          this.i18n(
              'removeToolChipAriaLabel',
              this.getToolChipLabel_(ComposeboxToolMode.kCanvas))}"
          ?visible="${true}"
          @click="${this.handleCanvasClick_}">
      </cr-composebox-tool-chip>
    ` : ''}
  `;

  // eslint-disable-next-line @webui-eslint/lit-element-template-structure
  const contextMenu = html`
      <div class="context-menu-container" id="contextMenuContainer"
          part="context-menu-and-tools"
          @mousedown="${this.onContextMenuContainerMouseDown_}"
          @click="${this.onContextMenuContainerClick_}">
        ${this.shouldHideEntrypointButton_ ? '' : html`
          ${this.showModelPicker ? html`
            <cr-composebox-contextual-entrypoint-button id="contextEntrypoint"
                part="composebox-entrypoint"
                exportparts="context-menu-entrypoint-icon"
                class="upload-button no-overlap"
                .tabSuggestions="${this.tabSuggestions}"
                .showMenuOnClick="${this.showMenuOnClick}"
                @open-image-upload="${this.openImageUpload_}"
                @open-file-upload="${this.openFileUpload_}"
                @add-tab-context="${this.addTabContext_}"
                @delete-tab-context="${this.onDeleteFile_}"
                @tool-click="${this.onToolClick_}"
                .hasImageFiles="${this.hasImageFiles()}"
                .disabledTabIds="${this.addedTabsIds_}"
                .fileNum="${this.files_.size}"
                .searchboxLayoutMode="${this.searchboxLayoutMode}"
                .inputState="${this.inputState}"
                ?upload-button-disabled="${this.uploadButtonDisabled_}"
                ?show-context-menu-description="${
                      this.shouldShowDescription_()}"
                glif-animation-state="${this.contextMenuGlifAnimationState}">
            </cr-composebox-contextual-entrypoint-button>
          ` : html`
            <cr-composebox-context-menu-entrypoint id="contextEntrypoint"
                part="composebox-entrypoint"
                exportparts="context-menu-entrypoint-icon"
                class="upload-button no-overlap"
                .tabSuggestions="${this.tabSuggestions}"
                .showMenuOnClick="${this.showMenuOnClick}"
                @open-image-upload="${this.openImageUpload_}"
                @open-file-upload="${this.openFileUpload_}"
                @add-tab-context="${this.addTabContext_}"
                @deep-search-click="${this.handleDeepSearchClick_}"
                @create-image-click="${this.handleImageGenClick_}"
                @delete-tab-context="${this.onDeleteFile_}"
                .inCreateImageMode="${
                  this.activeTool_ === ComposeboxToolMode.kImageGen}"
                .hasImageFiles="${this.hasImageFiles()}"
                .disabledTabIds="${this.addedTabsIds_}"
                .fileNum="${this.files_.size}"
                .searchboxLayoutMode="${this.searchboxLayoutMode}"
                ?upload-button-disabled="${this.uploadButtonDisabled_}"
                ?show-context-menu-description="${this.shouldShowDescription_()}"
                glif-animation-state="${this.contextMenuGlifAnimationState}">
            </cr-composebox-context-menu-entrypoint>
          `}
        `}
        ${this.searchboxLayoutMode === 'Compact' && this.showVoiceSearch ? html`
          <cr-icon-button id="voiceSearchButton" class="voice-icon"
              part="voice-icon" iron-icon="cr:mic"
              @click="${this.onVoiceSearchClick_}"
              title="${this.i18n('voiceSearchButtonLabel')}">
          </cr-icon-button>
        ` : ''}
        ${this.shouldShowToolChipsForTallMode_ ? toolChips : ''}
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

  return html`<!--_html_template_start_-->
  ${this.searchboxLayoutMode === 'Compact' && !this.isOmniboxInCompactMode_ ? contextMenu : ''}
    <div part="carousel-container">
    ${this.showFileCarousel_ ? html`
      <cr-composebox-file-carousel
        part="cr-composebox-file-carousel"
        exportparts="thumbnail, thumbnail-title"
        id="carousel"
        class="${this.carouselOnTop_ ? 'top' : ''}"
        .files="${Array.from(this.files_.values())}"
        @delete-file="${this.onDeleteFile_}">
      </cr-composebox-file-carousel> ` : ''}
    ${this.submitButtonShown && this.searchboxLayoutMode === 'Compact' ?
      html`<slot name="submit-button"></slot>` :
      ''}
  </div>
  ${this.searchboxLayoutMode === 'TallTopContext' ? contextMenu : ''}
  ${this.shouldShowDivider_ ? html`
    <div class="carousel-divider" part="carousel-divider"></div>
  ` : ''}
  <!-- Suggestions are slotted in from the parent component. -->
  <slot id="dropdownMatches"></slot>
  ${this.shouldShowToolChipsForCompactMode_ ? html`
    <div class="context-menu-container" id="toolChipsContainer"
        part="tool-chips-container">${toolChips}</div>
  ` : ''}
  ${this.searchboxLayoutMode === 'TallBottomContext' || this.searchboxLayoutMode === '' || this.isOmniboxInCompactMode_ ? html`
    ${this.contextMenuEnabled_ ? contextMenu : html`
      <div part="upload-container" id="uploadContainer" class="icon-fade">
          <cr-icon-button
              class="upload-button no-overlap"
              id="imageUploadButton"
              iron-icon="composebox:imageUpload"
              title="${this.i18n('composeboxImageUploadButtonTitle')}"
              .disabled="${this.uploadButtonDisabled_}"
              @click="${this.openImageUpload_}">
          </cr-icon-button>
          ${this.composeboxShowPdfUpload_ ? html`
          <cr-icon-button
              class="upload-button no-overlap"
              id="fileUploadButton"
              iron-icon="composebox:fileUpload"
              title="${this.i18n('composeboxPdfUploadButtonTitle')}"
              .disabled="${this.uploadButtonDisabled_}"
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
  ${this.shouldShowVoiceSearchAtBottom_() ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon" part="voice-icon"
        iron-icon="cr:mic" @click="${this.onVoiceSearchClick_}"
        title="${this.i18n('voiceSearchButtonLabel')}">
    </cr-icon-button>
   ` : ''}
  ${this.submitButtonShown && this.searchboxLayoutMode === 'TallBottomContext' ?
      html`<slot name="submit-button"></slot>` :
      ''}
<!--_html_template_end_-->`;
  // clang-format on
}
