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
  <div id="composebox" tabindex="-1" @keydown="${this.onKeydown_}"
      ?inert=${this.showErrorScrim_}>
    <div id="inputContainer">
      <ntp-composebox-file-carousel
        id="carousel"
        .files=${Array.from(this.files_.values())}
        @delete-file=${this.onDeleteFile_}>
      </ntp-composebox-file-carousel>
      <textarea autocomplete="off" id="input"
          type="search" spellcheck="false"
          placeholder="${this.inputPlaceholder_}"
          @keydown="${this.onInputKeydown_}"
          @input=${this.handleInput_}></textarea>
      <div id="uploadContainer">
        <cr-icon-button
            class="upload-icon no-overlap"
            id="imageUploadButton"
            iron-icon="composebox:imageUpload"
            title="$i18n{composeboxImageUploadButtonTitle}"
            .disabled="${this.inputsDisabled_}"
            @click="${this.openImageUpload_}">
        </cr-icon-button>
        <cr-icon-button
            class="upload-icon no-overlap"
            id="fileUploadButton"
            iron-icon="composebox:fileUpload"
            title="$i18n{composeboxPdfUploadButtonTitle}"
            .disabled="${this.inputsDisabled_}"
            @click="${this.openFileUpload_}">
        </cr-icon-button>
      </div>
    </div>
    <cr-icon-button
      class="action-icon icon-arrow-upward"
      id="submitIcon"
      title="$i18n{composeboxSubmitButtonTitle}"
      @click="${this.onSubmitClick_}"
      ?disabled="${!this.submitEnabled_}">
    </cr-icon-button>
    <cr-icon-button
        class="action-icon icon-clear"
        id="cancelIcon"
        title="${this.computeCancelButtonTitle_()}"
        @click="${this.onCancelClick_}">
    </cr-icon-button>
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
<!--_html_template_end_-->`;
  // clang-format on
}
