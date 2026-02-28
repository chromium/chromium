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
      attachmentFileTypes_: {type: Array},
      imageFileTypes_: {type: Array},
    };
  }

  accessor disableFileInputs: boolean = false;
  protected accessor attachmentFileTypes_: string[] =
      loadTimeData.getString('composeboxAttachmentFileTypes').split(',');
  protected accessor imageFileTypes_: string[] =
      loadTimeData.getString('composeboxImageFileTypes').split(',');

  protected openFileUpload_() {
    assert(this.$.fileInput);
    this.$.fileInput.click();
  }

  protected openImageUpload_() {
    assert(this.$.imageInput);
    this.$.imageInput.click();
  }


  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    this.fire('on-file-change', {files: input.files});
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
