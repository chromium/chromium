// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './context_menu_entrypoint.js';
import './composebox_dropdown.js';
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {mojoString16ToString, stringToMojoString16} from '//resources/js/mojo_type_util.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageCallbackRouter, PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
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
    matches: ComposeboxDropdownElement,
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
      contextMenuEnabled_: {type: Boolean},
      files_: {type: Object},
      input_: {type: String},
      imageFileTypes_: {type: String},
      isCollapsible: {
        reflect: true,
        type: Boolean,
      },
      expanded_: {
        reflect: true,
        type: Boolean,
      },
      inputsDisabled_: {
        reflect: true,
        type: Boolean,
      },
      hideFileInputs_: {
        reflect: true,
        type: Boolean,
      },
      result_: {type: Object},
      submitEnabled_: {
        reflect: true,
        type: Boolean,
      },
      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <composebox-dropdown> API to change
       * selection.
       */
      selectedMatchIndex_: {type: Number},
      submitting_: {
        reflect: true,
        type: Boolean,
      },
      showDropdown_: {
        reflect: true,
        type: Boolean,
      },
      enableImageContextualSuggestions_: {
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
        reflect: true,
        type: String,
      },
      composeboxShowPdfUpload_: {
        reflect: true,
        type: Boolean,
      },
      smartComposeInlineHint_: {type: String},
    };
  }

  protected accessor attachmentFileTypes_: string =
      loadTimeData.getString('composeboxAttachmentFileTypes');
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor imageFileTypes_: string =
      loadTimeData.getString('composeboxImageFileTypes');
  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  protected accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanded_: boolean;
  protected accessor input_: string = '';
  protected accessor inputsDisabled_: boolean = false;
  protected accessor showDropdown_: boolean =
      loadTimeData.getBoolean('composeboxShowZps');
  protected accessor enableImageContextualSuggestions_: boolean =
      loadTimeData.getBoolean('composeboxShowImageSuggest');
  // When enabled, the file input buttons will not be rendered.
  protected accessor hideFileInputs_: boolean = false;
  protected accessor selectedMatchIndex_: number = -1;
  protected accessor submitting_: boolean = false;
  protected accessor submitEnabled_: boolean = false;
  protected accessor showErrorScrim_: boolean = false;
  protected accessor errorMessage_: string = '';
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor smartComposeInlineHint_: string = '';
  protected accessor inputPlaceholder_: string =
      loadTimeData.getString('searchboxComposePlaceholder');
  protected accessor composeboxShowPdfUpload_: boolean =
      loadTimeData.getBoolean('composeboxShowPdfUpload');
  private showTypedSuggest_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggest');
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

  private selectedMatch_: AutocompleteMatch|null;
  private lastQueriedInput_: string = '';

  constructor() {
    super();
    this.callbackRouter_ = ComposeboxProxyImpl.getInstance().callbackRouter;
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set the initial expanded state based on the inputted property.
    this.expanded_ = !this.isCollapsible;

    this.listenerIds = [
      this.callbackRouter_.onContextualInputStatusChanged.addListener(
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
                  default:
                    break;
                }
                this.showErrorScrim_ = true;
              } else {
                file = {...file, status: status};
                this.files_.set(token, file);

                if (status === FileUploadStatus.kProcessing && this.showZps &&
                    (this.enableImageContextualSuggestions_ ||
                     !file.type.includes('image'))) {
                  // Query autocomplete to get contextual suggestions for files.
                  this.clearAutocompleteMatches_();
                  this.lastQueriedInput_ = this.$.input.value;
                  this.searchboxHandler_.queryAutocomplete(
                      stringToMojoString16(this.$.input.value), false);
                }
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

    this.pageHandler_.notifySessionStarted();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.pageHandler_.notifySessionAbandoned();

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

    // When the result initially gets set check if dropdown should show.
    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('result_')) {
      this.showDropdown_ = this.computeShowDropdown_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('showErrorScrim_') &&
        this.showErrorScrim_) {
      const announcer = getAnnouncerInstance();
      announcer.announce(this.errorMessage_);
      const dismissErrorButton =
          this.shadowRoot.querySelector<HTMLElement>('#dismissErrorButton');
      if (dismissErrorButton) {
        dismissErrorButton.focus();
      }
    }
    if (changedPrivateProperties.has('selectedMatchIndex_')) {
      if (this.selectedMatch_) {
        // Update the input.
        const text = mojoString16ToString(this.selectedMatch_.fillIntoEdit);
        assert(text);
        this.$.input.value = text;
        this.input_ = text;
        this.submitEnabled_ = true;
      } else {
        this.$.input.value = '';
        this.input_ = '';
        this.submitEnabled_ = false;
      }
    }
    if (changedPrivateProperties.has('smartComposeInlineHint_')) {
      if (this.smartComposeInlineHint_) {
        this.adjustInputForSmartCompose();
      } else {
        // Unset the height override so input can expand through typing.
        this.$.input.style.height =
            'calc-size(fit-content, min(size + 4px, 190px))';
      }
    }
  }

  getText() {
    return this.$.input.value;
  }

  resetText() {
    this.$.input.value = '';
  }

  getSmartComposeForTesting() {
    return this.smartComposeInlineHint_;
  }

  protected computeCancelButtonTitle_() {
    return this.input_.trim().length > 0 || this.files_.size > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  private computeShowDropdown_() {
    // Don't show dropdown if there's no results.
    if (!this.result_?.matches?.length) {
      return false;
    }

    if (this.showTypedSuggest_ && this.input_.trim()) {
      return true;
    }

    // lastQueriedInput_ is used here since the input_ changes based on
    // the selected match. If typed suggest is not enabled and input_ is used,
    // the dropdown will hide if the user keys down over zps matches.
    return this.showZps && !this.lastQueriedInput_;
  }

  protected shouldShowSuggestionActivityLink_() {
    if (!this.result_ || !this.showDropdown_) {
      return false;
    }
    return this.result_.matches.some((match) => match.isNoncannedAimSuggestion);
  }

  protected shouldShowSmartComposeInlineHint_() {
    return !!this.smartComposeInlineHint_;
  }

  protected onDeleteFile_(e: CustomEvent) {
    if (!e.detail.uuid || !this.files_.has(e.detail.uuid)) {
      return;
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== e.detail.uuid));
    this.pageHandler_.deleteContext(e.detail.uuid);
    this.$.input.focus();
    this.clearAutocompleteMatches_();
    this.searchboxHandler_.queryAutocomplete(
        stringToMojoString16(this.$.input.value), false);
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
        const {token} = await this.pageHandler_.addFileContext(
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
          url: null,
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

  protected async addTabContext_(
      e: CustomEvent<{id: number, title: string, url: Url}>) {
    const {token} = await this.pageHandler_.addTabContext(e.detail.id);

    const attachment: ComposeboxFile = {
      uuid: token,
      name: e.detail.title,
      objectUrl: null,
      type: 'tab',
      status: FileUploadStatus.kNotUploaded,
      url: e.detail.url,
    };
    this.files_ = new Map([...this.files_.entries(), [token, attachment]]);
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
      this.lastQueriedInput_ = this.input_;
      this.files_ = new Map();
      this.smartComposeInlineHint_ = '';
      this.submitEnabled_ = false;
      this.pageHandler_.clearFiles();
      this.$.input.focus();
      this.$.matches.unselect();
      this.clearAutocompleteMatches_();
      this.searchboxHandler_.queryAutocomplete(
          stringToMojoString16(this.$.input.value), false);
    } else {
      this.closeComposebox_();
    }
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected handleInput_(e: Event) {
    const inputElement = e.target as HTMLInputElement;
    this.input_ = inputElement.value;
    this.lastQueriedInput_ = this.input_;
    // This is done to stop any in progress providers before requerying
    // for on-focus (zero-suggest) inputs. The searchbox doesn't allow
    // zero-suggest requests to be made while the ACController is not
    // done.
    if (this.lastQueriedInput_ === '') {
      this.clearAutocompleteMatches_();
    }
    this.searchboxHandler_.queryAutocomplete(
        stringToMojoString16(this.$.input.value), false);
  }

  protected onKeydown_(e: KeyboardEvent) {
    const KEYDOWN_HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
      'Tab',
    ];

    if (!KEYDOWN_HANDLED_KEYS.includes(e.key)) {
      return;
    }

    if (this.shadowRoot.activeElement === this.$.input) {
      if ((e.key === 'ArrowDown' || e.key === 'ArrowUp')) {
        return;
      }

      if (e.key === 'Tab') {
        // If focus leaves the input, unselect the first match.
        if (e.shiftKey && this.selectedMatchIndex_ === 0) {
          this.$.matches.unselect();
        } else if (this.smartComposeInlineHint_) {
          this.input_ = this.input_ + this.smartComposeInlineHint_;
          this.$.input.value = this.input_;
          this.smartComposeInlineHint_ = '';
          e.preventDefault();
        }
        return;
      }
    }

    if (e.key === 'Tab') {
      // If focus goes past the last match, unselect the last match.
      if (this.result_ && this.result_.matches) {
        if (this.selectedMatchIndex_ === this.result_.matches.length - 1) {
          this.$.matches.unselect();
        }
      }
      return;
    }

    if (e.key === 'Enter' && this.submitEnabled_) {
      if (this.shadowRoot.activeElement === this.$.matches || !e.shiftKey) {
        e.preventDefault();
        this.submitQuery_(e);
      }
    }

    if (e.key === 'Escape' && this.composeboxCloseByEscape_) {
      this.closeComposebox_();
      e.preventDefault();
    }

    // Do not handle the following keys if there are no matches available.
    if (!this.result_ || this.result_.matches.length === 0) {
      return;
    }

    // Do not handle the following keys if there are key modifiers.
    if (hasKeyModifiers(e)) {
      return;
    }

    if (e.key === 'ArrowDown') {
      this.$.matches.selectNext();
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      this.$.matches.selectFirst();
    } else if (e.key === 'PageDown') {
      this.$.matches.selectLast();
    }
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }
  }

  protected handleComposeboxFocusIn_() {
    this.expanded_ = true;
    this.submitting_ = false;
    this.pageHandler_.focusChanged(true);
  }

  protected handleComposeboxFocusOut_() {
    // If the input is blurred and the composebox is expandable, collapse it.
    // Else, keep the composebox expanded.
    this.expanded_ = !this.isCollapsible;
    this.pageHandler_.focusChanged(false);
  }

  protected handleScroll_() {
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    const input = this.shadowRoot.querySelector<HTMLElement>('#input');
    smartCompose!.scrollTop = input!.scrollTop;
  }

  private closeComposebox_() {
    this.fire('close-composebox', {composeboxText: this.$.input.value});

    if (this.isCollapsible) {
      this.expanded_ = false;
      this.$.input.blur();
    }
  }

  protected submitQuery_(e: KeyboardEvent|MouseEvent) {
    // Users are allowed to submit queries that consist of only files with no
    // input. `selectedMatchIndex_` will be >= 0 when there is non-empty input
    // since the verbatim match is present.
    assert(
        (this.selectedMatchIndex_ >= 0 && this.result_) ||
        this.files_.size > 0);

    // If there is a match that is selected, open that match, else follow the
    // non-autocomplete submission flow. The non-autocomplete submission flow
    // will not have omnibox metrics recorded for it.
    if (this.selectedMatchIndex_ >= 0) {
      const match = this.result_!.matches[this.selectedMatchIndex_];
      assert(match);
      this.searchboxHandler_.openAutocompleteMatch(
          this.selectedMatchIndex_, match.destinationUrl,
          /* are_matches_showing */ true, (e as MouseEvent).button || 0,
          e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
    } else {
      this.pageHandler_.submitQuery(
          this.$.input.value.trim(), (e as MouseEvent).button || 0, e.altKey,
          e.ctrlKey, e.metaKey, e.shiftKey);
    }

    this.submitting_ = true;

    // If the composebox is expandable, collapse it and clear the input after
    // submitting.
    if (this.isCollapsible) {
      this.resetText();
      this.$.input.blur();
      this.submitEnabled_ = false;
    }
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail.index);
  }

  protected onMatchClick_() {
    this.clearAutocompleteMatches_();
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex_ = e.detail.value;
    this.selectedMatch_ =
        this.result_?.matches[this.selectedMatchIndex_] || null;
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  private clearAutocompleteMatches_() {
    this.showDropdown_ = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.searchboxHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = '';
  }

  private recordFileValidationMetric_(
      enumValue: ComposeboxFileValidationError) {
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure', enumValue,
        ComposeboxFileValidationError.MAX_VALUE + 1);
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !==
            mojoString16ToString(result.input)) {
      return;
    }
    // TODO(crbug.com/434748455): Display suggestions below composebox.
    this.result_ = result;
    const hasMatches = this.result_?.matches?.length > 0;
    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    // Zero suggest matches are not allowed to be default. Therefore, this
    // makes sure zero suggest results aren't focused when they are returned.
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      this.$.matches.selectFirst();
    } else {
      this.$.matches.unselect();
    }

    // Populate the smart compose suggestion.
    this.smartComposeInlineHint_ = this.result_.smartComposeInlineHint ?
        mojoString16ToString(this.result_.smartComposeInlineHint) :
        '';
  }

  private adjustInputForSmartCompose() {
    // Checks the scroll height of the input + smart complete hint (ghost div)
    // and updates the height of the actual input to be that height so the
    // ghost text does not overflow.
    const input = this.shadowRoot.querySelector<HTMLInputElement>('#input');
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');

    const ghostHeight = smartCompose!.scrollHeight;
    const maxHeight = 190;
    input!.style.height = `${Math.min(ghostHeight, maxHeight)}px`;

    // If the height of the input + smart complete hint is greater than the max
    // height, scroll the smart compose as the input will already scroll. Note
    // there is an issue at the break point since the input will not have
    // scrolled yet as it does not have enough content. The smart compose will
    // display the ghost text below the input and it will be cut off. However,
    // the current response only works for queries below the max height.
    if (ghostHeight > maxHeight) {
      smartCompose!.scrollTop = input!.scrollTop;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
