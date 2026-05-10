// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFileThumbnailElement} from './file_thumbnail.js';

export function getHtml(this: ComposeboxFileThumbnailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="container">
      ${this.file.type === 'injectedinput' ? this.file.iconName ? html`
        <div id="injectedInputChip" class="chip">
          <div id="injectedInputIconThumbnail" class="thumbnail" part="thumbnail">
            ${this.isUploading_ ? html`
              <svg role="image" class="spinner" viewBox="0 0 100 100">
                <circle class="spinner-circle" cx="50" cy="50" r="40" />
              </svg>
            ` : html`
              <cr-icon icon="aim:${this.file.iconName}"
                  class="injected-input-icon"></cr-icon>
            `}
          </div>
          <p class="title" part="thumbnail-title" id="injectedInputTitle">
            ${this.file.name}
          </p>
          <div class="overlay">
            <div class="gradient-protection"></div>
            ${this.file.isDeletable ? html`<cr-icon-button
                id="removeInjectedInputIconButton"
                class="remove-button"
                iron-icon="cr:clear"
                title="${this.file.name}"
                aria-label="${this.getDeleteFileButtonTitle_()}"
                @click="${this.onRemoveButtonClick_}">
            </cr-icon-button>`: ''}
          </div>
          <div class="chip-overlay"></div>
        </div>
      ` : this.file.name ? html`
        <div id="injectedInputChip" class="chip">
          <div id="injectedInputImgThumbnail"
            class="thumbnail injected-input-img-thumbnail">
            <img class="injected-input-img"
              src="${this.file.objectUrl || this.file.dataUrl}"
              aria-label="${this.file.name}">
            ${this.isUploading_ ? html`
              <div id="spinnerOverlay">
                <svg role="image" class="spinner" viewBox="0 0 100 100">
                  <circle class="spinner-circle" cx="50" cy="50" r="40" />
                </svg>
              </div>
            ` : ''}
          </div>
          <p class="title" id="injectedInputTitle">${this.file.name}</p>
          <div class="overlay">
            <div class="gradient-protection"></div>
            ${this.file.isDeletable ? html`<cr-icon-button
                id="removeInjectedInputButton"
                class="remove-button"
                iron-icon="cr:clear"
                title="${this.file.name}"
                aria-label="${this.getDeleteFileButtonTitle_()}"
                @click="${this.onRemoveButtonClick_}">
            </cr-icon-button>`: ''}
          </div>
          <div class="chip-overlay"></div>
        </div>
      ` : html`
        <div id="injectedInputImgChip" class="img-chip">
          ${this.isUploading_ ? html`
            <svg role="image" class="spinner" viewBox="0 0 100 100">
              <circle class="spinner-circle" cx="50" cy="50" r="40" />
            </svg>
          ` : html`
            <img class="img-thumbnail"
              src="${this.file.objectUrl || this.file.dataUrl}"
              aria-label="${this.file.name}">
          `}
          ${this.file.isDeletable ? html`<cr-icon-button
              class="img-overlay"
              id="removeInjectedInputImgButton"
              iron-icon="cr:clear"
              title="${this.file.name}"
              aria-label="${this.getDeleteFileButtonTitle_()}"
              @click="${this.onRemoveButtonClick_}">
          </cr-icon-button>`: ''}
        </div>
      ` : this.file.url ? html`
        <div id="tabChip" class="chip" title="${this.file.name}">
          <div id="tabThumbnail" class="thumbnail">
            <cr-composebox-tab-favicon .url="${this.file.url}" .size="${24}">
            </cr-composebox-tab-favicon>
            ${this.isUploading_ ? html`
              <div id="spinnerOverlay">
                <svg role="image" class="spinner" viewBox="0 0 100 100">
                  <circle class="spinner-circle" cx="50" cy="50" r="40" />
                </svg>
              </div>
            ` : ''}
          </div>

          <div class="tabInfo">
            <div part="thumbnail-title" class="title">
              ${this.file.name}
            </div>
            <div class="url">${this.getFormattedUrl_()}</div>
          </div>
          <div class="overlay">
            <div class="gradient-protection"></div>
            ${this.file.isDeletable ? html`<cr-icon-button
              id="removeTabButton"
              class="remove-button"
              iron-icon="cr:clear"
              title="${this.file.name}"
              aria-label="${this.getDeleteFileButtonTitle_()}"
              @click="${this.onRemoveButtonClick_}">
              </cr-icon-button>`: ''}
          </div>
          <div class="chip-overlay"></div>
        </div>
      ` : (this.file.type.startsWith('image/') || this.file.objectUrl
            || this.file.dataUrl) ? html`
        <div id="imgChip" class="img-chip">
          ${this.isUploading_ ? html`
            <svg role="image" class="spinner" viewBox="0 0 100 100">
              <circle class="spinner-circle" cx="50" cy="50" r="40" />
            </svg>
          ` : html`
            ${this.file.thumbnailUrl ? html`
              <img is="cr-auto-img" class="img-thumbnail"
                auto-src="${this.file.thumbnailUrl}"
                aria-label="${this.file.name}">
            ` : html`
            <img class="img-thumbnail"
              src="${this.file.objectUrl || this.file.dataUrl}"
              aria-label="${this.file.name}">
            `}
          `}
          ${this.file.isDeletable ? html`<cr-icon-button
              class="img-overlay"
              id="removeImgButton"
              iron-icon="cr:clear"
              title="${this.file.name}"
              aria-label="${this.getDeleteFileButtonTitle_()}"
              @click="${this.onRemoveButtonClick_}">
          </cr-icon-button>`: ''}
        </div>` : html`
        <div id="documentChip" class="chip">
          <div id="documentThumbnail" class="thumbnail" part="thumbnail">
            ${this.isUploading_ ? html`
              <svg role="image" class="spinner" viewBox="0 0 100 100">
                <circle class="spinner-circle" cx="50" cy="50" r="40" />
              </svg>
            ` : (this.file.type === 'application/vnd.google-apps.document' ||
                 this.file.type === 'application/vnd.google-apps.spreadsheet' ||
                 this.file.type === 'application/vnd.google-apps.presentation') ? html`
              <img is="cr-auto-img" class="document-icon" draggable="false"
                  auto-src="https://drive-thirdparty.googleusercontent.com/32/type/${this.file.type}">
            ` : html`
              <cr-icon icon="${
                  this.shouldUsePdfIcon_() ?
                      'thumbnail:pdf' :
                      'thumbnail:document'}"
                  class="${
                  this.shouldUsePdfIcon_() ?
                      'pdf-icon' :
                      'document-icon'}"></cr-icon>
            `}
          </div>
          <p class="title"
              part="thumbnail-title" id="documentTitle">${this.file.name}</p>
          <div class="overlay">
            <div class="gradient-protection"></div>
            ${this.file.isDeletable ? html`<cr-icon-button
                id="removeDocumentButton"
                class="remove-button"
                iron-icon="cr:clear"
                title="${this.file.name}"
                aria-label="${this.getDeleteFileButtonTitle_()}"
                @click="${this.onRemoveButtonClick_}">
            </cr-icon-button>`: ''}
          </div>
          <div class="chip-overlay"></div>
        </div>
      `}
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
