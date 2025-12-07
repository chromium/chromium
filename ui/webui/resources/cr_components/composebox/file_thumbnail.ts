// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './composebox_tab_favicon.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFile} from './common.js';
import {FileUploadStatus} from './composebox_query.mojom-webui.js';
import {getCss} from './file_thumbnail.css.js';
import {getHtml} from './file_thumbnail.html.js';

export interface ComposeboxFileThumbnailElement {
  $: {
    removeImgButton: HTMLElement,
    removePdfButton: HTMLElement,
    removeTabButton: HTMLElement,
  };
}

export class ComposeboxFileThumbnailElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-file-thumbnail';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      file: {type: Object},
    };
  }

  accessor file: ComposeboxFile = {
    name: '',
    type: '',
    objectUrl: null,
    dataUrl: null,
    uuid: '',
    status: FileUploadStatus.kNotUploaded,
    url: null,
    tabId: null,
    isDeletable: true,
  };

  protected deleteFile_() {
    // TODO(crbug.com/422559977): Send call to handler to delete file from
    // cache.
    this.fire('delete-file', {uuid: this.file.uuid, fromUserAction: true});
  }

  protected get deleteFileButtonTitle_(): string {
    return loadTimeData.getStringF('composeboxDeleteFileTitle', this.file.name);
  }

  protected get formattedUrl_(): string|null {
    if (!this.file?.url) {
      return null;
    }
    const link = new URL(this.file.url.url);
    return (link.host + link.pathname).replace(/\/$/, '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-thumbnail': ComposeboxFileThumbnailElement;
  }
}

customElements.define(
    ComposeboxFileThumbnailElement.is, ComposeboxFileThumbnailElement);
