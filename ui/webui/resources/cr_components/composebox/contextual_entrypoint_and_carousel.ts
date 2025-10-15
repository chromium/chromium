// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './context_menu_entrypoint.js';
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './contextual_entrypoint_and_carousel.css.js';
import {getHtml} from './contextual_entrypoint_and_carousel.html.js';
import {FileUploadErrorType, FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export interface ContextualEntrypointAndCarouselElement {
  $: {
    fileInput: HTMLInputElement,
    fileUploadButton: CrIconButtonElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: CrIconButtonElement,
  };
}

const FILE_VALIDATION_ERRORS_MAP = new Map<FileUploadErrorType, string>([
  [
    FileUploadErrorType.kImageProcessingError,
    'composeboxFileUploadImageProcessingError',
  ],
  [
    FileUploadErrorType.kUnknown,
    'composeboxFileUploadValidationFailed',
  ],
]);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
const enum ComposeboxFileValidationError {
  NONE = 0,
  TOO_MANY_FILES = 1,
  FILE_EMPTY = 2,
  FILE_SIZE_TOO_LARGE = 3,
  MAX_VALUE = FILE_SIZE_TOO_LARGE,
}


export class ContextualEntrypointAndCarouselElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'contextual-entrypoint-and-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showDropdown: {type: Boolean},
      compactMode: {type: Boolean},
      attachmentFileTypes_: {type: String},
      contextMenuEnabled_: {type: Boolean},
      files_: {type: Object},
      imageFileTypes_: {type: String},
      inputsDisabled_: {
        reflect: true,
        type: Boolean,
      },
      composeboxShowPdfUpload_: {
        reflect: true,
        type: Boolean,
      },
      showContextMenuDescription_: {type: Boolean},
      showFileCarousel_ : {
        reflect: true,
        type: Boolean,
      },
      inDeepSearchMode_ : {
        reflect: true,
        type: Boolean,
      },
      inCreateImageMode_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor showDropdown: boolean = false;
  accessor compactMode: boolean = false;
  protected accessor attachmentFileTypes_: string =
      loadTimeData.getString('composeboxAttachmentFileTypes');
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  protected accessor inputsDisabled_: boolean = false;
  protected accessor composeboxShowPdfUpload_: boolean =
      loadTimeData.getBoolean('composeboxShowPdfUpload');
  protected accessor showContextMenuDescription_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  protected accessor showFileCarousel_: boolean = false;
  protected accessor inDeepSearchMode_: boolean = false;
  protected accessor inCreateImageMode_: boolean = false;
  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('files_')) {
      this.inputsDisabled_ = this.files_.size >= this.maxFileCount_;
      this.showFileCarousel_ = this.files_.size > 0;
      this.fire('on-context-files-changed', {files: this.files_.size});
    }
  }

  updateFileStatus(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType) {
    let errorMessage = null;
    let file = this.files_.get(token);
    if (file) {
      if ([FileUploadStatus.kValidationFailed,
           FileUploadStatus.kUploadFailed,
           FileUploadStatus.kUploadExpired].includes(status)) {
        this.files_.delete(token);

        switch (status) {
          case FileUploadStatus.kValidationFailed:
            errorMessage = this.i18n(
                FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                'composeboxFileUploadValidationFailed');
            break;
          case FileUploadStatus.kUploadFailed:
            errorMessage = this.i18n('composeboxFileUploadFailed');
            break;
          case FileUploadStatus.kUploadExpired:
            errorMessage = this.i18n('composeboxFileUploadExpired');
            break;
          default:
            break;
        }
      } else {
        file = {...file, status: status};
        this.files_.set(token, file);
      }
      this.files_ = new Map([...this.files_]);
    }
    return {file, errorMessage};
  }

  resetContextFiles() {
    this.files_ = new Map();
  }

  hasImageFiles() {
    if (this.files_) {
      for (const file of this.files_.values()) {
        if (file.type.includes('image')) {
          return true;
        }
      }
    }
    return false;
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== e.detail.uuid));
    this.fire('delete-context', {uuid: e.detail.uuid});
  }

  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;

    // Multiple is set to false in the input so only one file is expected.
    if (!files || files.length === 0 ||
        this.files_.size >= this.maxFileCount_) {
      this.recordFileValidationMetric_(
          ComposeboxFileValidationError.TOO_MANY_FILES);
      return;
    }

    const filesToUpload: File[] = [];
    for (const file of files) {
      if (file.size === 0 || file.size > this.maxFileSize_) {
        const fileIsEmpty = file.size === 0;
        input.value = '';
        fileIsEmpty ? this.recordFileValidationMetric_(
                          ComposeboxFileValidationError.FILE_EMPTY) :
                      this.recordFileValidationMetric_(
                          ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE);
        this.fire('on-file-validation-error', {
            errorMessage: fileIsEmpty ?
                this.i18n('composeboxFileUploadInvalidEmptySize') :
                this.i18n('composeboxFileUploadInvalidTooLarge'),
        });
        return;
      }

      if (!file.type.includes('pdf') && !file.type.includes('image')) {
        return;
      }
      filesToUpload.push(file);
    }
    this.fire('add-file-context', {
      files: filesToUpload,
      isImage: input === this.$.imageInput,
      onContextAdded: (files: Map<UnguessableToken, ComposeboxFile>) => {
        this.files_ = new Map([...this.files_.entries(), ...files.entries()]);
        this.recordFileValidationMetric_(ComposeboxFileValidationError.NONE);
        input.value = '';
      },
    });
  }

  protected addTabContext_(
      e: CustomEvent<{id: number, title: string, url: Url}>) {
    e.stopPropagation();

    this.fire('add-tab-context', {
      id: e.detail.id,
      title: e.detail.title,
      url: e.detail.url,
      onContextAdded: (file: ComposeboxFile) => {
        this.files_ = new Map([...this.files_.entries(), [file.uuid, file]]);
      },
    });
  }

  protected openImageUpload_() {
    this.$.imageInput.click();
  }

  protected openFileUpload_() {
    this.$.fileInput.click();
  }

  protected onDeepSearchClick_() {
    this.showContextMenuDescription_ = !this.showContextMenuDescription_;
    this.inputsDisabled_ = !this.inputsDisabled_;
    this.inDeepSearchMode_ = !this.inDeepSearchMode_;
    this.fire(
        'set-deep-search-mode', {inDeepSearchMode: this.inDeepSearchMode_});
  }

  protected onCreateImageClick_(e: CustomEvent<{inCreateImageMode: boolean}>) {
    this.showContextMenuDescription_ = !this.showContextMenuDescription_;
    this.inCreateImageMode_ = !this.inCreateImageMode_;
    this.fire('set-create-image-mode',
      {inCreateImageMode: e.detail.inCreateImageMode});
  }

  private recordFileValidationMetric_(
      enumValue: ComposeboxFileValidationError) {
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure', enumValue,
        ComposeboxFileValidationError.MAX_VALUE + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-entrypoint-and-carousel':
        ContextualEntrypointAndCarouselElement;
  }
}

customElements.define(
    ContextualEntrypointAndCarouselElement.is,
    ContextualEntrypointAndCarouselElement);
