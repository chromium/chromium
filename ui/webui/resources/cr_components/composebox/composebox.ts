// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {stringToMojoString16} from '//resources/js/mojo_type_util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageCallbackRouter, PageHandlerRemote} from './composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {FileUploadErrorType, FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export interface ComposeboxElement {
  $: {
    cancelIcon: CrIconButtonElement,
    fileInput: HTMLInputElement,
    fileUploadButton: CrIconButtonElement,
    carousel: ComposeboxFileCarouselElement,
    imageInput: HTMLInputElement,
    imageUploadButton: CrIconButtonElement,
    input: HTMLInputElement,
    composebox: HTMLElement,
    submitIcon: CrIconButtonElement,
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


export class ComposeboxElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'ntp-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      attachmentFileTypes_: {type: String},
      files_: {type: Object},
      input_: {type: String},
      imageFileTypes_: {type: String},
      inputsDisabled_: {
        reflect: true,
        type: Boolean,
      },
      result_: {type: Object},
      submitEnabled_: {
        reflect: true,
        type: Boolean,
      },
      submitting_: {
        reflect: true,
        type: Boolean,
      },
      showErrorScrim_: {
        reflect: true,
        type: Boolean,
      },
      errorMessage_: {
        type: String,
      },
      inputPlaceholder_: {
        type: String,
        reflect: true,
      },
    };
  }

  protected accessor attachmentFileTypes_: string =
      loadTimeData.getString('composeboxAttachmentFileTypes');
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  protected accessor input_: string = '';
  protected accessor inputsDisabled_: boolean = false;
  protected accessor submitEnabled_: boolean = false;
  protected accessor submitting_: boolean = false;
  protected accessor showErrorScrim_: boolean = false;
  protected accessor errorMessage_: string = '';
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor inputPlaceholder_: string =
      loadTimeData.getString('searchboxComposePlaceholder');
  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private showZps: boolean = loadTimeData.getBoolean('composeboxShowZps');
  private browserProxy: ComposeboxProxyImpl = ComposeboxProxyImpl.getInstance();
  private callbackRouter_: PageCallbackRouter;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private listenerIds: number[] = [];
  private searchboxListenerIds: number[] = [];
  private composeboxCloseByEscape_: boolean =
      loadTimeData.getBoolean('composeboxCloseByEscape');

  constructor() {
    super();
    this.callbackRouter_ = ComposeboxProxyImpl.getInstance().callbackRouter;
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
    this.pageHandler_.notifySessionStarted();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds = [
      this.callbackRouter_.onFileUploadStatusChanged.addListener(
          (token: UnguessableToken, status: FileUploadStatus,
           errorType: FileUploadErrorType) => {
            let file = this.files_.get(token);
            if (file) {
              if ([
                    FileUploadStatus.kValidationFailed,
                    FileUploadStatus.kUploadFailed,
                    FileUploadStatus.kUploadExpired,
                  ].includes(status)) {
                this.files_.delete(token);

                switch (status) {
                  case FileUploadStatus.kValidationFailed:
                    this.errorMessage_ = this.i18n(
                        FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                        'composeboxFileUploadValidationFailed');
                    break;
                  case FileUploadStatus.kUploadFailed:
                    this.errorMessage_ =
                        this.i18n('composeboxFileUploadFailed');
                    break;
                  case FileUploadStatus.kUploadExpired:
                    this.errorMessage_ =
                        this.i18n('composeboxFileUploadExpired');
                    break;
                }
                this.showErrorScrim_ = true;
              } else {
                file = {...file, status: status};
                this.files_.set(token, file);

                if (status === FileUploadStatus.kUploadSuccessful) {
                  const announcer = getAnnouncerInstance();
                  announcer.announce(
                      this.i18n('composeboxFileUploadCompleteText'));
                }
              }
              this.files_ = new Map([...this.files_]);
            }
          }),
    ];

    this.searchboxListenerIds = [
      this.searchboxCallbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged_.bind(this)),
    ];


    this.eventTracker_.add(this.$.input, 'input', () => {
      this.submitEnabled_ = this.$.input.value.trim().length > 0;
    });
    this.$.input.focus();
    if (this.showZps) {
      this.searchboxHandler_.queryAutocomplete(
          stringToMojoString16(this.$.input.value), false);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.searchboxListenerIds.forEach(
        id => assert(
            this.browserProxy.searchboxCallbackRouter.removeListener(id)));
    this.listenerIds = [];
    this.searchboxListenerIds = [];

    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('files_')) {
      this.inputsDisabled_ = this.files_.size >= this.maxFileCount_;
      this.submitEnabled_ = this.submitEnabled_ || this.files_.size > 0;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    if ((changedProperties as Map<PropertyKey, unknown>)
            .has('showErrorScrim_') &&
        this.showErrorScrim_) {
      const announcer = getAnnouncerInstance();
      announcer.announce(this.errorMessage_);
      const dismissErrorButton =
          this.shadowRoot.querySelector<HTMLElement>('#dismissErrorButton');
      if (dismissErrorButton) {
        dismissErrorButton.focus();
      }
    }
  }

  getText() {
    return this.$.input.value;
  }

  resetText() {
    this.$.input.value = '';
  }

  protected computeCancelButtonTitle_() {
    return this.input_.trim().length > 0 || this.files_.size > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== e.detail.uuid));
    this.pageHandler_.deleteFile(e.detail.uuid);
    this.$.input.focus();
  }

  protected onDismissErrorButtonClick_() {
    this.errorMessage_ = '';
    this.showErrorScrim_ = false;
  }

  protected async onFileChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const files = input.files;
    if (!files || files.length === 0 ||
        this.files_.size >= this.maxFileCount_) {
      this.recordFileValidationMetric_(
          ComposeboxFileValidationError.TOO_MANY_FILES);
      return;
    }

    for (const file of files) {
      if (file.size === 0 || file.size > this.maxFileSize_) {
        this.showErrorScrim_ = true;
        const fileIsEmpty = file.size === 0;
        this.errorMessage_ = fileIsEmpty ?
            this.i18n('composeboxFileUploadInvalidEmptySize') :
            this.i18n('composeboxFileUploadInvalidTooLarge');
        input.value = '';
        fileIsEmpty ? this.recordFileValidationMetric_(
                          ComposeboxFileValidationError.FILE_EMPTY) :
                      this.recordFileValidationMetric_(
                          ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE);
        return;
      } else {
        const fileBuffer = await file.arrayBuffer();
        if (!file.type.includes('pdf') && !file.type.includes('image')) {
          return;
        }

        const bigBuffer:
            BigBuffer = {bytes: Array.from(new Uint8Array(fileBuffer))};
        const {token} = await this.pageHandler_.addFile(
            {
              fileName: file.name,
              mimeType: file.type,
              selectionTime: new Date(),
            },
            bigBuffer);

        const attachment: ComposeboxFile = {
          uuid: token,
          name: file.name,
          objectUrl: input === this.$.imageInput ? URL.createObjectURL(file) :
                                                   null,
          type: file.type,
          status: FileUploadStatus.kNotUploaded,
        };
        this.files_ = new Map([...this.files_.entries(), [token, attachment]]);

        const announcer = getAnnouncerInstance();
        announcer.announce(this.i18n('composeboxFileUploadStartedText'));
        this.recordFileValidationMetric_(ComposeboxFileValidationError.NONE);
      }
    }
    // Clear the file input.
    input.value = '';
    this.$.input.focus();
  }

  protected openImageUpload_() {
    this.$.imageInput.click();
  }

  protected openFileUpload_() {
    this.$.fileInput.click();
  }

  protected onCancelClick_() {
    if (this.$.input.value.trim().length > 0 || this.files_.size > 0) {
      this.$.input.value = '';
      this.input_ = '';
      this.files_ = new Map();
      this.submitEnabled_ = false;
      this.pageHandler_.clearFiles();
      this.$.input.focus();
    } else {
      this.closeComposebox_();
    }
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected handleInput_(e: Event) {
    const inputElement = e.target as HTMLInputElement;
    this.input_ = inputElement.value;
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey && this.submitEnabled_) {
      e.preventDefault();
      this.onSubmitClick_(e);
    }
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' && this.composeboxCloseByEscape_) {
      this.closeComposebox_();
    }
  }

  private closeComposebox_() {
    this.fire('close-composebox', {composeboxText: this.$.input.value});
  }

  protected onSubmitClick_(e: KeyboardEvent|MouseEvent) {
    this.pageHandler_.submitQuery(
        this.$.input.value.trim(), (e as MouseEvent).button || 0, e.altKey,
        e.ctrlKey, e.metaKey, e.shiftKey);
    this.submitting_ = true;
  }

  private recordFileValidationMetric_(
      enumValue: ComposeboxFileValidationError) {
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure', enumValue,
        ComposeboxFileValidationError.MAX_VALUE + 1);
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    // TODO(crbug.com/434748455): Display suggestions below composebox.
    this.result_ = result;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
