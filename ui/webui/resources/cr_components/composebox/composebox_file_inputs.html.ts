// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFileInputsElement} from './composebox_file_inputs.js';

export function getHtml(this: ComposeboxFileInputsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.disableFileInputs ? html`<slot></slot>` : html`
  <div id="fileUploadWrapper"
    @open-image-upload="${this.openImageUpload_}"
    @open-file-upload="${this.openFileUpload_}">
    <slot></slot>
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
`}
<!--_html_template_end_-->`;
  // clang-format off
}
