// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './composebox_tab_favicon.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFile} from './common.js';
import {ContextUploadStatus, InputType} from './composebox_query.mojom-webui.js';
import {getCss} from './file_thumbnail.css.js';
import {getHtml} from './file_thumbnail.html.js';

export interface ComposeboxFileThumbnailElement {
  $: {
    removeImgButton: HTMLElement,
    removeDocumentButton: HTMLElement,
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
      isUploading_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor file: ComposeboxFile = {
    name: '',
    type: '',
    inputType: InputType.kLensFile,
    objectUrl: null,
    dataUrl: null,
    uuid: '',
    status: ContextUploadStatus.kNotUploaded,
    url: null,
    tabId: null,
    isDeletable: true,
    iconName: null,
    supportsUnimodal: true,
    thumbnailUrl: null,
  };

  getIsUploadingForTesting(): boolean {
    return this.isUploading_;
  }

  protected lensSendRawFileMediaTypesEnabled_: boolean =
      loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled');

  protected accessor isUploading_: boolean = false;

  protected shouldUsePdfIcon_(): boolean {
    return !this.lensSendRawFileMediaTypesEnabled_ ||
        this.file.type === 'pdf' || this.file.type === 'application/pdf';
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('file')) {
      this.isUploading_ =
          this.file.status === ContextUploadStatus.kProcessing ||
          this.file.status ===
              ContextUploadStatus.kProcessingSuggestSignalsReady ||
          this.file.status === ContextUploadStatus.kUploadStarted;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    requestAnimationFrame(() => {
      this.classList.add('entering');
      const animations = this.getAnimations();
      if (animations.length > 0) {
        Promise.allSettled(animations.map(a => a.finished)).then(() => {
          this.classList.remove('entering');
        });
      } else {
        this.classList.remove('entering');
      }
    });
  }

  protected onRemoveButtonClick_() {
    if (this.classList.contains('exiting')) {
      return;
    }

    this.classList.add('exiting');
    const animations = this.getAnimations();

    if (animations.length === 0) {
      this.classList.remove('exiting');
      this.fire('delete-file', {uuid: this.file.uuid, fromUserAction: true});
    } else {
      Promise.allSettled(animations.map(a => a.finished)).then(() => {
        this.classList.remove('exiting');
        this.fire('delete-file', {uuid: this.file.uuid, fromUserAction: true});
      });
    }
  }

  protected getDeleteFileButtonTitle_(): string {
    return loadTimeData.getStringF('composeboxDeleteFileTitle', this.file.name);
  }

  protected getFormattedUrl_(): string|null {
    if (!this.file?.url) {
      return null;
    }
    const link = new URL(this.file.url);
    const host = link.host.replace(/^www\./, '');
    return (host + link.pathname).replace(/\/$/, '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-thumbnail': ComposeboxFileThumbnailElement;
  }
}

customElements.define(
    ComposeboxFileThumbnailElement.is, ComposeboxFileThumbnailElement);
