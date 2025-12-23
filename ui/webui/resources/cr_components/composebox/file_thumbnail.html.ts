// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {ComposeboxFileThumbnailElement} from './file_thumbnail.js';

export function getHtml(this: ComposeboxFileThumbnailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  ${this.file.url ? html`
    <div id="tabChip" class="chip">
      <div id="tabThumbnail" class="thumbnail">
        <cr-composebox-tab-favicon .url="${this.file.url.url}" .size="${24}">
        </cr-composebox-tab-favicon>
      </div>
      <div class="tabInfo">
        <div class="title">${this.file.name}</div>
        <div class="url">${this.formattedUrl_}</div>
      </div>
      <div class="overlay">
        <div class="gradient-protection"></div>
        ${this.file.isDeletable ? html`<cr-icon-button
          id="removeTabButton"
          class="remove-button"
          iron-icon="cr:clear"
          title="${this.file.name}"
          aria-label="${this.deleteFileButtonTitle_}"
          @click="${this.deleteFile_}">
          </cr-icon-button>`: ''}
      </div>
      <div class="chip-overlay"></div>
    </div>
  ` : (this.file.objectUrl || this.file.dataUrl) ? html`
    <div id="imgChip">
      ${this.file.status !== FileUploadStatus.kUploadSuccessful ? html`
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
          id="removeImgButton"
          iron-icon="cr:clear"
          title="${this.file.name}"
          aria-label="${this.deleteFileButtonTitle_}"
          @click="${this.deleteFile_}">
      </cr-icon-button>`: ''}
    </div>` : html`
    <div id="pdfChip" class="chip">
      <div id="pdfThumbnail" class="thumbnail" part="thumbnail">
        ${this.file.status === FileUploadStatus.kUploadSuccessful ? html`
          <cr-icon icon="thumbnail:pdf" class="pdf-icon">
          </cr-icon>
        ` : html`
          <svg class="spinner" viewBox="0 0 100 100">
            <circle class="spinner-circle" cx="50" cy="50" r="40" />
          </svg>
        `}
      </div>
      <p class="title" id="pdfTitle">${this.file.name}</p>
      <div class="overlay">
        <div class="gradient-protection"></div>
        ${this.file.isDeletable ? html`<cr-icon-button
            id="removePdfButton"
            class="remove-button"
            iron-icon="cr:clear"
            title="${this.file.name}"
            aria-label="${this.deleteFileButtonTitle_}"
            @click="${this.deleteFile_}">
        </cr-icon-button>`: ''}
      </div>
      <div class="chip-overlay"></div>
    </div>
  `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
