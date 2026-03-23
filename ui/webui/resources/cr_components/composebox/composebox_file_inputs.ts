// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './composebox_file_inputs.html.js';

export interface ComposeboxFileInputsElement {
  $: {
    imageInput: HTMLInputElement,
    fileInput: HTMLInputElement,
  };
}

export class ComposeboxFileInputsElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-file-inputs';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disableFileInputs: {type: Boolean},
      attachmentFileTypes_: {type: String},
      imageFileTypes_: {type: String},
    };
  }

  accessor disableFileInputs: boolean = false;
  // Comma separated list of file types.
  protected accessor attachmentFileTypes_: string =
      loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled') ?
      '*/*' :
      loadTimeData.getString('composeboxAttachmentFileTypes');
  // Comma separated list of file types.
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');

  protected onOpenFileUpload_() {
    assert(this.$.fileInput);
    this.$.fileInput.click();
  }

  protected onOpenImageUpload_() {
    assert(this.$.imageInput);
    this.$.imageInput.click();
  }


  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    this.fire('file-change', {files: input.files});
    input.value = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-inputs': ComposeboxFileInputsElement;
  }
}

customElements.define(
    ComposeboxFileInputsElement.is, ComposeboxFileInputsElement);
