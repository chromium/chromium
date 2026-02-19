// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';
import {getHtml as getContextMenuHtml} from './contextual_entrypoint_and_carousel_context_menu.html.js';
import {getHtml as getToolChipsHtml} from './contextual_entrypoint_and_carousel_tool_chips.html.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.searchboxLayoutMode === 'Compact' && !this.isOmniboxInCompactMode_ ?
    getContextMenuHtml.bind(this)()
  : ''}
    <div part="carousel-container">
    ${this.showFileCarousel_ ? html`
      <cr-composebox-file-carousel
        part="cr-composebox-file-carousel"
        exportparts="thumbnail, thumbnail-title"
        id="carousel"
        class="${this.carouselOnTop_ ? 'top' : ''}"
        .files="${Array.from(this.files_.values())}"
        ?enable-scrolling="${this.enableCarouselScrolling}"
        @delete-file="${this.onDeleteFile_}">
      </cr-composebox-file-carousel> ` : ''}
    ${this.submitButtonShown && this.searchboxLayoutMode === 'Compact' ?
      html`<slot name="submit-button"></slot>` :
      ''}
  </div>
  ${this.searchboxLayoutMode === 'TallTopContext' ?
    getContextMenuHtml.bind(this)()
  : ''}
  ${this.shouldShowDivider_ ? html`
    <div class="carousel-divider" part="carousel-divider"></div>
  ` : ''}
  <!-- Suggestions are slotted in from the parent component. -->
  <slot id="dropdownMatches"></slot>
  ${this.shouldShowToolChipsForCompactMode_ ? html`
    <div class="context-menu-container" id="toolChipsContainer"
        part="tool-chips-container">
      ${getToolChipsHtml.bind(this)()}
    </div>
  ` : ''}
  ${this.searchboxLayoutMode === 'TallBottomContext' || this.searchboxLayoutMode === '' || this.isOmniboxInCompactMode_ ? html`
    ${this.contextMenuEnabled_ ? getContextMenuHtml.bind(this)() : html`
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
