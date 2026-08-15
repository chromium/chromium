// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './composebox_tab_favicon.js';
import './icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getLoadTimeBoolean} from './common.js';
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
      isAndroid_: {
        type: Boolean,
        reflect: true,
        attribute: 'is-android',
      },
      isUploading_: {
        type: Boolean,
        reflect: true,
      },
      tabFaviconChipsToCoinsEnabled_: {type: Boolean},
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

  protected accessor tabFaviconChipsToCoinsEnabled_: boolean =
      loadTimeData.getBoolean('tabFaviconChipsToCoinsEnabled');

  protected accessor isAndroid_: boolean =
      getLoadTimeBoolean('isAndroid', false);

  protected accessor isUploading_: boolean = false;

  protected shouldUsePdfIcon_(): boolean {
    return !this.lensSendRawFileMediaTypesEnabled_ ||
        this.file.type === 'pdf' || this.file.type === 'application/pdf';
  }

  protected isVideo_(): boolean {
    return Boolean(
        this.file?.type &&
        (this.file.type.startsWith('video/') ||
         this.file.type.includes('video')));
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

  /**
   * Formats filenames longer than `MAX_DISPLAY_LENGTH` by inserting an ellipsis
   * (`...`) in the middle of the base filename so that short extensions
   * (e.g. `.pdf`, `.docx`) are preserved at the end of the string.
   *
   * For files without an extension or with long extensions (e.g. `.gitignore`),
   * the original filename is returned untouched to rely on CSS truncation.
   */
  protected getFormattedFileName_(): string {
    const MAX_DISPLAY_LENGTH = 18;
    const MAX_EXTENSION_LENGTH = 5;
    const ELLIPSIS = '...';

    const name = this.file?.name || '';
    if (name.length <= MAX_DISPLAY_LENGTH) {
      return name;
    }

    const dotIndex = name.lastIndexOf('.');
    if (dotIndex === -1 || dotIndex === 0) {
      return name;
    }

    const extension = name.slice(dotIndex);
    if (extension.length > MAX_EXTENSION_LENGTH) {
      return name;
    }

    const baseName = name.slice(0, dotIndex);
    const availableCharsForBase =
        MAX_DISPLAY_LENGTH - ELLIPSIS.length - extension.length;
    const startChars = Math.ceil(availableCharsForBase / 2);
    const endChars = Math.floor(availableCharsForBase / 2);
    return `${baseName.slice(0, startChars)}${ELLIPSIS}${
        baseName.slice(-endChars)}${extension}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-thumbnail': ComposeboxFileThumbnailElement;
  }
}

customElements.define(
    ComposeboxFileThumbnailElement.is, ComposeboxFileThumbnailElement);
