// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './context_menu_entrypoint.js';
import './file_carousel.js';
import './recent_tab_chip.js';
import './icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile, ContextualUpload} from './common.js';
import {FileUploadErrorType, FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {ContextMenuEntrypointElement} from './context_menu_entrypoint.js';
import {getCss} from './contextual_entrypoint_and_carousel.css.js';
import {getHtml} from './contextual_entrypoint_and_carousel.html.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';
import type {RecentTabChipElement} from './recent_tab_chip.js';

// LINT.IfChange(ComposeboxMode)
export enum ComposeboxMode {
  DEFAULT = '',
  DEEP_SEARCH = 'deep-search',
  CREATE_IMAGE = 'create-image',
}
// LINT.ThenChange(chromium/components/omnibox/browser/searchbox.mojom:ComposeboxMode)

export interface ContextualEntrypointAndCarouselElement {
  $: {
    fileInput: HTMLInputElement,
    fileUploadButton: CrIconButtonElement,
    contextEntrypoint: ContextMenuEntrypointElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: CrIconButtonElement,
    recentTabChip: RecentTabChipElement,
    voiceSearchButton: CrIconButtonElement,
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
      // =========================================================================
      // Public properties
      // =========================================================================
      showDropdown: {type: Boolean},
      searchboxLayoutMode: {type: String},
      tabSuggestions: {type: Array},
      entrypointName: {type: String},
      parentFocused: {type: Boolean},
      showVoiceSearch: {
        reflect: true,
        type: Boolean,
      },

      // =========================================================================
      // Protected properties
      // =========================================================================
      attachmentFileTypes_: {type: String},
      contextMenuEnabled_: {type: Boolean},
      files_: {type: Object},
      addedTabsIds_: {type: Object},
      imageFileTypes_: {type: String},
      inputsDisabled_: {
        reflect: true,
        type: Boolean,
      },
      recentTabChipDisabled_: {
        reflect: true,
        type: Boolean,
      },
      composeboxShowPdfUpload_: {
        reflect: true,
        type: Boolean,
      },
      showContextMenuDescription_: {type: Boolean},
      showFileCarousel_: {
        reflect: true,
        type: Boolean,
      },
      showRecentTabChip_: {type: Boolean},
      inDeepSearchMode_: {
        reflect: true,
        type: Boolean,
      },
      inCreateImageMode_: {
        reflect: true,
        type: Boolean,
      },
      recentTabForChip_: {type: Object},
      recentTabInContext_: {type: Boolean},
      carouselOnTop_: {type: Boolean},
    };
  }

  accessor showDropdown: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor entrypointName: string = '';
  accessor tabSuggestions: TabInfo[] = [];
  accessor carouselOnTop_: boolean = false;
  accessor parentFocused: boolean = false;
  accessor showVoiceSearch: boolean = false;

  protected accessor attachmentFileTypes_: string =
      loadTimeData.getString('composeboxAttachmentFileTypes');
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor addedTabsIds_: Map<number, UnguessableToken> = new Map();
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  protected accessor inputsDisabled_: boolean = false;
  protected accessor recentTabChipDisabled_: boolean = false;
  protected accessor composeboxShowPdfUpload_: boolean =
      loadTimeData.getBoolean('composeboxShowPdfUpload');
  protected accessor showContextMenuDescription_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  protected accessor showRecentTabChip_: boolean =
      loadTimeData.getBoolean('composeboxShowRecentTabChip');
  protected accessor showFileCarousel_: boolean = false;
  protected accessor inDeepSearchMode_: boolean = false;
  protected accessor inCreateImageMode_: boolean = false;
  protected accessor recentTabForChip_: TabInfo|null = null;
  protected accessor recentTabInContext_: boolean = false;

  protected get inToolMode_(): boolean {
    return this.inDeepSearchMode_ || this.inCreateImageMode_;
  }

  protected get shouldShowRecentTabChip_(): boolean {
    return !!this.recentTabForChip_ && this.parentFocused &&
        this.showRecentTabChip_ && !this.recentTabInContext_ &&
        !this.inToolMode_;
  }

  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private createImageModeEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowCreateImageButton');

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('files_') ||
        changedPrivateProperties.has(`inCreateImageMode_`)) {
      // If only 1 image is uploaded and the create image tool is enabled, we
      // don't want to disable the context menu entrypoint because the user
      // should still be able to use the tool within the context menu.
      const isCreateImageToolAvailableWithImages =
          this.createImageModeEnabled_ &&
          this.hasImageFiles() && this.files_.size === 1;
      // `inputsDisabled_` decides whether or not the context menu entrypoint is
      // shown to the user. Only set `inputsDisabled_` to true if
      // 1. The max number of files is reached, and the create image tool button
      //    is not available.
      // 2. The user has an image uploaded and is in create image mode.
      this.inputsDisabled_ =
          (this.files_.size >= this.maxFileCount_ &&
           !isCreateImageToolAvailableWithImages) ||
          (this.hasImageFiles() && this.inCreateImageMode_);
      this.showFileCarousel_ = this.files_.size > 0;
      this.recentTabChipDisabled_ = this.files_.size >= this.maxFileCount_;
      this.fire('on-context-files-changed', {files: this.files_.size});
    }

    if (changedPrivateProperties.has('files_') ||
        changedProperties.has('tabSuggestions')) {
      this.recentTabInContext_ = this.computeRecentTabInContext_();
    }

    if (changedProperties.has('tabSuggestions')) {
      this.recentTabForChip_ =
          this.tabSuggestions.find(tab => tab.showInRecentTabChip) || null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // The position of the contextual entrypoint might change in the tall modes,
    // so the context menu needs to be repositioned accordingly.
    if (this.searchboxLayoutMode !== 'Compact' && this.contextMenuEnabled_)  {
      this.$.contextEntrypoint.repositionMenu();
    }
  }

  private computeRecentTabInContext_(): boolean {
    const recentTab = this.tabSuggestions?.[0];
    if (!recentTab) {
      return false;
    }

    return this.addedTabsIds_.has(recentTab.tabId);
  }

  addFiles(files: FileList|null) {
    this.processFiles_(files);
  }

  setContextFiles(files: ContextualUpload[]) {
    for (const file of files) {
      if ('tabId' in file) {
        this.addTabContext_(new CustomEvent('addTabContext', {
          detail: {
            id: file.tabId,
            title: file.title,
            url: file.url,
            delayUpload: file.delayUpload,
          },
        }));
      } else {
        this.addFileContext_([file.file]);
      }
    }
  }

  setInitialMode(mode: ComposeboxMode) {
    switch (mode) {
      case ComposeboxMode.DEEP_SEARCH:
        this.onDeepSearchClick_();
        break;
      case ComposeboxMode.CREATE_IMAGE:
        this.onCreateImageClick_();
        break;
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
        if (file.tabId) {
          this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
            ([id, _]) => id !== file!.tabId));
        }

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
    // Only keep files that are not deletable.
    const undeletableFiles =
        Array.from(this.files_.values()).filter(file => !file.isDeletable);

    if (undeletableFiles.length === this.files_.size) {
      return;
    }

    this.files_ = new Map(undeletableFiles.map(file => [file.uuid, file]));
    this.addedTabsIds_ = new Map(
        undeletableFiles.filter(file => file.tabId)
            .map(file => [file.tabId!, file.uuid]));
  }

  resetModes() {
    if (this.inDeepSearchMode_) {
      this.inDeepSearchMode_ = false;
      this.inputsDisabled_ = false;
      this.fire(
          'set-deep-search-mode', {inDeepSearchMode: this.inDeepSearchMode_});
      this.showContextMenuDescription_ = true;
    } else if (this.inCreateImageMode_) {
      this.inCreateImageMode_ = false;
      this.fire('set-create-image-mode', {
        inCreateImageMode: this.inCreateImageMode_,
        imagePresent: this.hasImageFiles(),
      });
      this.showContextMenuDescription_ = true;
    }
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

  hasDeletableFiles() {
    return Array.from(this.files_.values()).some(file => file.isDeletable);
  }

  onFileContextAdded(file: ComposeboxFile) {
    const newFiles = new Map(this.files_);
    newFiles.set(file.uuid, file);
    this.files_ = newFiles;
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }

    const file = this.files_.get(e.detail.uuid);
    if (file?.tabId) {
      this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
            ([id, _]) => id !== file.tabId));
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== e.detail.uuid));
    this.fire('delete-context', {uuid: e.detail.uuid});
  }

  protected processFiles_(files: FileList|null) {
    // Multiple is set to false in the input so only one file is expected.
    if (!files || files.length === 0) {
      return;
    }
    if ((this.files_.size + files.length) > this.maxFileCount_) {
      this.fire('on-file-validation-error', {
        errorMessage: this.i18n('maxFilesReachedError'),
      });
      this.recordFileValidationMetric_(
          ComposeboxFileValidationError.TOO_MANY_FILES);
      return;
    }

    const filesToUpload: File[] = [];
    for (const file of files) {
      if (file.size === 0 || file.size > this.maxFileSize_) {
        const fileIsEmpty = file.size === 0;
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
        this.fire('on-file-validation-error', {
          errorMessage:
              // TODO(crbug.com/454730356): replace with translatable string
              // that includes pdf and not just image.
              this.i18n('composeboxFileUploadImageProcessingError'),
        });
        return;
      }
      filesToUpload.push(file);
    }
    this.addFileContext_(filesToUpload);
  }

  protected onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;
    this.processFiles_(files);
    input.value = '';
  }

  protected addFileContext_(filesToUpload: File[]) {
    this.fire('add-file-context', {
      files: filesToUpload,
      onContextAdded: (files: Map<UnguessableToken, ComposeboxFile>) => {
        this.files_ = new Map([...this.files_.entries(), ...files.entries()]);
        this.recordFileValidationMetric_(ComposeboxFileValidationError.NONE);
      },
    });
  }

  protected addTabContext_(e: CustomEvent<{
      id: number, title: string, url: Url, delayUpload: boolean,
  }>) {
    e.stopPropagation();

    this.fire('add-tab-context', {
      id: e.detail.id,
      title: e.detail.title,
      url: e.detail.url,
      delayUpload: e.detail.delayUpload,
      onContextAdded: (file: ComposeboxFile) => {
        this.files_ = new Map([...this.files_.entries(), [file.uuid, file]]);
        this.addedTabsIds_ = new Map(
            [...this.addedTabsIds_.entries(), [e.detail.id, file.uuid]]);
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
    if (this.entrypointName !== 'Realbox') {
      this.showContextMenuDescription_ = !this.showContextMenuDescription_;
      this.inputsDisabled_ = !this.inputsDisabled_;
      this.inDeepSearchMode_ = !this.inDeepSearchMode_;
    }
    this.fire(
        'set-deep-search-mode', {inDeepSearchMode: this.inDeepSearchMode_});
  }

  protected onCreateImageClick_() {
    if (this.entrypointName !== 'Realbox') {
      this.showContextMenuDescription_ = !this.showContextMenuDescription_;
      this.inCreateImageMode_ = !this.inCreateImageMode_;
      if (this.hasImageFiles()) {
        this.inputsDisabled_ = !this.inputsDisabled_;
      }
    }
    this.fire('set-create-image-mode', {
      inCreateImageMode: this.inCreateImageMode_,
      imagePresent: this.hasImageFiles(),
    });
  }

  protected onVoiceSearchClick_() {
    this.fire('open-voice-search');
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
