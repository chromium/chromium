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
  ${this.file.objectUrl ? html`
    <div id="imgChip">
      ${this.file.status === FileUploadStatus.kUploadSuccessful ? html`
        <img class="img-thumbnail"
          src="${this.file.objectUrl}"
          aria-label="${this.file.name}">
      ` : html`
        <svg role="image" class="spinner" viewBox="0 0 100 100">
          <circle class="spinner-circle" cx="50" cy="50" r="40" />
        </svg>
      `}
      <cr-icon-button
          class="img-overlay"
          id="removeImgButton"
          iron-icon="cr:clear"
          title="${this.file.name}"
          aria-label="${this.deleteFileButtonTitle}"
          @click="${this.deleteFile_}">
      </cr-icon-button>
    </div>` : html`
    <div id="pdfChip">
      <div id="pdfThumbnail">
        ${this.file.status === FileUploadStatus.kUploadSuccessful ? html`
          <cr-icon icon="thumbnail:pdf" class="pdf-icon">
          </cr-icon>
        ` : html`
          <svg class="spinner" viewBox="0 0 100 100">
            <circle class="spinner-circle" cx="50" cy="50" r="40" />
          </svg>
        `}
        <div class="pdf-overlay">
          <cr-icon-button
              id="removePdfButton"
              iron-icon="cr:clear"
              title="${this.file.name}"
              aria-label="${this.deleteFileButtonTitle}"
              @click="${this.deleteFile_}">
          </cr-icon-button>
        </div>
      </div>
      <p class="pdf-title">${this.file.name}</p>
    </div>
  `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
