// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './composebox_tool_chip.js';
import './context_menu_entrypoint.js';
import './contextual_entrypoint_and_carousel.js';
import './composebox_dropdown.js';
import './composebox_voice_search.js';
import './error_scrim.js';
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, SelectedFileInfo, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile, ContextualUpload} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {FileUploadErrorType} from './composebox_query.mojom-webui.js';
import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';
import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';
import {ComposeboxMode} from './contextual_entrypoint_and_carousel.js';
import type {ErrorScrimElement} from './error_scrim.js';

export enum VoiceSearchAction {
  ACTIVATE = 0,
  QUERY_SUBMITTED = 1,
}

const DEBOUNCE_TIMEOUT_MS: number = 20;

function debounce(context: Object, func: () => void, delay: number) {
  let timeout: number;
  return function(...args: []) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(context, args), delay);
  };
}

export interface ComposeboxElement {
  $: {
    cancelIcon: CrIconButtonElement,
    input: HTMLInputElement,
    composebox: HTMLElement,
    submitContainer: HTMLElement,
    submitOverlay: HTMLElement,
    matches: ComposeboxDropdownElement,
    context: ContextualEntrypointAndCarouselElement,
    errorScrim: ErrorScrimElement,
    voiceSearch: ComposeboxVoiceSearchElement,
  };
}

export class ComposeboxElement extends I18nMixinLit
(CrLitElement) implements DragAndDropHost {
  static get is() {
    return 'cr-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showLensButton: {type: Boolean},
      lensButtonTriggersOverlay: {type: Boolean},
      input_: {type: String},
      isCollapsible: {
        reflect: true,
        type: Boolean,
      },
      expanding_: {
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
       * Do not modify this. Use <cr-composebox-dropdown> API to change
       * selection.
       */
      selectedMatchIndex_: {type: Number},
      showDropdown_: {
        reflect: true,
        type: Boolean,
      },
      showSubmit_: {
        reflect: true,
        type: Boolean,
      },
      animationState: {
        reflect: true,
        type: String,
      },
      enableImageContextualSuggestions_: {
        reflect: true,
        type: Boolean,
      },
      inputPlaceholder_: {
        reflect: true,
        type: String,
      },
      smartComposeEnabled_: {
        reflect: true,
        type: Boolean,
      },
      smartComposeInlineHint_: {type: String},
      showFileCarousel_: {
        reflect: true,
        type: Boolean,
      },
      inDeepSearchMode_: {
        reflect: true,
        type: Boolean,
      },
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      inCreateImageMode_: {
        reflect: true,
        type: Boolean,
      },
      /**
       * Feature flag for New Tab Page Realbox Next.
       */
      ntpRealboxNextEnabled: {
        type: Boolean,
        // NOTE: Do not reflect this property. For shared UI, use
        // `searchboxNextEnabled`. For NTP-specific styling, use CSS from the
        // embedding component (e.g., `new_tab_page/app.css`).
      },
      /**
       * Generic flag indicating a "Next" searchbox (Realbox Next, Omnibox Next,
       * etc.). Used for all styling and behavior shared across 'Next' searchbox
       * implementations.
       */
      searchboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },
      tabSuggestions: {type: Array},
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
      },
      errorScrimVisible_: {type: Boolean},
      contextFilesSize_: {
        type: Number,
        reflect: true,
      },
      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },
      carouselOnTop_: {
        type: Boolean,
      },
      inVoiceSearchMode_: {
        type: Boolean,
        reflect: true,
      },
      entrypointName: {type: String},
      transcript_: {type: String},
      receivedSpeech_: {type: Boolean},
      maxSuggestions: {type: Number},
      disableVoiceSearchAnimation: {type: Boolean},
      disableCaretColorAnimation: {
        type: Boolean,
        reflect: true,
      },
      disableComposeboxAnimation: {type: Boolean},
      fileUploadsComplete: {type: Boolean},
      inComposebox: {type: Boolean},
    };
  }

  accessor disableCaretColorAnimation: boolean = false;
  accessor disableComposeboxAnimation: boolean = false;
  accessor inComposebox: boolean = false;
  accessor lensButtonTriggersOverlay: boolean = false;
  accessor fileUploadsComplete: boolean = true;
  accessor maxSuggestions: number|null = null;
  accessor showLensButton: boolean = true;
  accessor ntpRealboxNextEnabled: boolean = false;
  accessor searchboxNextEnabled: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor carouselOnTop_: boolean = false;
  accessor isDraggingFile: boolean = false;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor entrypointName: string = '';
  accessor disableVoiceSearchAnimation: boolean = false;
  accessor tabSuggestions: TabInfo[] = [];
  accessor lensButtonDisabled: boolean = false;
  protected composeboxNoFlickerSuggestionsFix_: boolean =
      loadTimeData.getBoolean('composeboxNoFlickerSuggestionsFix');
  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  protected accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanding_: boolean = false;
  protected accessor input_: string = '';
  protected accessor showDropdown_: boolean =
      loadTimeData.getBoolean('composeboxShowZps');
  protected accessor showSubmit_: boolean =
      loadTimeData.getBoolean('composeboxShowSubmit');
  protected accessor enableImageContextualSuggestions_: boolean =
      loadTimeData.getBoolean('composeboxShowImageSuggest');
  // When enabled, the file input buttons will not be rendered.
  protected accessor selectedMatchIndex_: number = -1;
  protected accessor submitEnabled_: boolean = false;
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor smartComposeInlineHint_: string = '';
  protected accessor smartComposeEnabled_: boolean =
      loadTimeData.getBoolean('composeboxSmartComposeEnabled');
  protected accessor inputPlaceholder_: string =
      loadTimeData.getString('searchboxComposePlaceholder');
  protected accessor showFileCarousel_: boolean = false;
  protected accessor inCreateImageMode_: boolean = false;
  protected accessor inDeepSearchMode_: boolean = false;
  protected accessor errorScrimVisible_: boolean = false;
  protected accessor contextFilesSize_: number = 0;
  protected accessor transcript_: string = '';
  protected accessor receivedSpeech_: boolean = false;
  protected lastQueriedInput_: string = '';
  protected showVoiceSearchInSteadyComposebox_: boolean =
      loadTimeData.getBoolean('steadyComposeboxShowVoiceSearch');
  protected showVoiceSearchInExpandedComposebox_: boolean =
      loadTimeData.getBoolean('expandedComposeboxShowVoiceSearch');
  protected dragAndDropHandler_: DragAndDropHandler;
  private showTypedSuggest_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggest');
  private showTypedSuggestWithContext_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggestWithContext');
  private showZps: boolean = loadTimeData.getBoolean('composeboxShowZps');
  private browserProxy: ComposeboxProxyImpl = ComposeboxProxyImpl.getInstance();
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private searchboxListenerIds: number[] = [];
  private resizeObservers_: ResizeObserver[] = [];
  private composeboxCloseByEscape_: boolean =
      loadTimeData.getBoolean('composeboxCloseByEscape');
  private dragAndDropEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
  private clearAllInputsWhenSubmittingQuery_: boolean =
      loadTimeData.valueExists('clearAllInputsWhenSubmittingQuery') ?
      loadTimeData.getBoolean('clearAllInputsWhenSubmittingQuery') :
      false;
  private autoSubmitVoiceSearch: boolean =
      loadTimeData.valueExists('autoSubmitVoiceSearchQuery') ?
      loadTimeData.getBoolean('autoSubmitVoiceSearchQuery') :
      true;
  protected accessor inVoiceSearchMode_: boolean = false;
  private selectedMatch_: AutocompleteMatch|null = null;
  // Whether the composebox is actively waiting for an autocomplete response. If
  // this is false, that means at least one response has been received (even if
  // the response was empty or had an error).
  private haveReceivedAutcompleteResponse_: boolean = false;
  private isVoiceInput_: boolean = false;
  private pendingUploads_: Set<string> = new Set<string>([]);

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
    this.dragAndDropHandler_ =
        new DragAndDropHandler(this, this.dragAndDropEnabled_);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set the initial expanded state based on the inputted property.
    this.expanding_ = !this.isCollapsible;
    this.animationState = this.isCollapsible ? GlowAnimationState.NONE :
                                               GlowAnimationState.EXPANDING;

    this.searchboxListenerIds = [
      this.searchboxCallbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged_.bind(this)),
      this.searchboxCallbackRouter_.onContextualInputStatusChanged.addListener(
          this.onContextualInputStatusChanged_.bind(this)),
      this.searchboxCallbackRouter_.onTabStripChanged.addListener(
          this.refreshTabSuggestions_.bind(this)),
      this.searchboxCallbackRouter_.addFileContext.addListener(
          this.addFileContextFromBrowser_.bind(this)),
      this.searchboxCallbackRouter_.updateAutoSuggestedTabContext.addListener(
          this.updateAutoSuggestedTabContext_.bind(this)),
    ];

    this.eventTracker_.add(this.$.input, 'input', () => {
      this.submitEnabled_ = this.computeSubmitEnabled_();
    });
    this.eventTracker_.add(
        this.$.context, 'on-context-files-changed',
        (e: CustomEvent<{files: number}>) => {
          this.contextFilesSize_ = e.detail.files;
          this.showFileCarousel_ = this.contextFilesSize_ > 0;
          this.submitEnabled_ = this.computeSubmitEnabled_();
        });
    this.eventTracker_.add(
        this.$.context, 'carousel-resize',
        (e: CustomEvent<{height: number}>) => {
          this.fire('composebox-resize', {carouselHeight: e.detail.height});
        });
    this.focusInput();
    // For "next" searchboxes (Realbox Next, Omnibox Next, etc.), the zps
    // autocomplete query is triggered after the state has been initialized.
    if (this.showZps && !this.searchboxNextEnabled) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }

    this.searchboxHandler_.notifySessionStarted();
    this.refreshTabSuggestions_();

    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-initialized', {
        initializeComposeboxState: this.initializeState_.bind(this),
      });
    }

    this.setupResizeObservers_();
  }

  private setupResizeObservers_() {
    const composeboxResizeObserver = new ResizeObserver(debounce(this, () => {
      this.fire('composebox-resize', {height: this.offsetHeight});
    }, DEBOUNCE_TIMEOUT_MS));
    this.resizeObservers_.push(composeboxResizeObserver);
    composeboxResizeObserver.observe(this);

    const composeboxDropdownResizeObserver =
        new ResizeObserver(debounce(this, () => {
          this.fire(
              'composebox-resize',
              {dropdownHeight: this.$.matches.offsetHeight});
        }, DEBOUNCE_TIMEOUT_MS));
    this.resizeObservers_.push(composeboxDropdownResizeObserver);
    composeboxDropdownResizeObserver.observe(this.$.matches);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.searchboxHandler_.notifySessionAbandoned();

    this.searchboxListenerIds.forEach(
        id => assert(
            this.browserProxy.searchboxCallbackRouter.removeListener(id)));
    this.searchboxListenerIds = [];

    this.eventTracker_.removeAll();

    for (const observer of this.resizeObservers_) {
      observer.disconnect();
    }
    this.resizeObservers_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    // When the result initially gets set check if dropdown should show.
    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('contextFilesSize_') ||
        changedPrivateProperties.has('errorScrimVisible_')) {
      this.showDropdown_ = this.computeShowDropdown_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedMatchIndex_')) {
      if (this.selectedMatch_) {
        // If the selected match is the default match (typing) the input will
        // already have been set by handleInput.
        if (!(this.selectedMatchIndex_ === 0 &&
              this.selectedMatch_.allowedToBeDefaultMatch)) {
          // Update the input.
          const text = this.selectedMatch_.fillIntoEdit;
          assert(text);
          this.input_ = text;
          this.submitEnabled_ = true;
        }
      } else if (!this.lastQueriedInput_) {
        // This is for cases when focus leaves the matches/input.
        // If there was already text in the input do not clear it.
        this.clearInput();
        this.submitEnabled_ = this.contextFilesSize_ > 0;
      } else {
        // For typed queries reset the input back to typed value when
        // focus leaves the match.
        this.input_ = this.lastQueriedInput_;
      }
    }
    if (changedPrivateProperties.has('smartComposeInlineHint_')) {
      if (this.smartComposeInlineHint_) {
        this.adjustInputForSmartCompose();
        // TODO(crbug.com/452619068): Investigate why screenreader is
        // inconsistent.
        const announcer = getAnnouncerInstance();
        announcer.announce(
            this.smartComposeInlineHint_ + ', ' +
            this.i18n('composeboxSmartComposeTitle'));
      } else {
        // Unset the height override so input can expand through typing.
        this.$.input.style.height = 'unset';
      }
    }
  }

  /* Used by drag/drop host interface so the
  drag and drop handler can access addDroppedFiles(). */
  getDropTarget() {
    return this.$.context;
  }

  focusInput() {
    this.$.input.focus();
  }

  getText() {
    return this.input_;
  }

  playGlowAnimation() {
    // If |animationState_| were still EXPANDING, this function would have no
    // effect because nothing changes in CSS and therefore animations wouldn't
    // be re-trigered. Resetting it to NONE forces the animation related styles
    // to reset before switching to EXPANDING.
    this.animationState = GlowAnimationState.NONE;
    // Wait for the style change for NONE to commit. This ensures the browser
    // detects a state change when we switch to EXPANDING.

    // If the composebox is not submittable or it is already expanded, do not
    // trigger the animation.
    if (this.expanding_ && !this.submitEnabled_) {
      requestAnimationFrame(() => {
        this.animationState = GlowAnimationState.EXPANDING;
      });
    }
  }

  setText(text: string) {
    this.input_ = text;
  }

  resetModes() {
    this.$.context.resetModes();
  }

  closeDropdown() {
    this.clearAutocompleteMatches();
  }

  getSmartComposeForTesting() {
    return this.smartComposeInlineHint_;
  }

  getMatchesElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  getHasAutomaticActiveTabChipToken() {
    return this.$.context.hasAutomaticActiveTabChipToken();
  }

  getAutomaticActiveTabChipElement(): HTMLElement|null {
    return this.$.context.getAutomaticActiveTabChipElement();
  }

  protected initializeState_(
      text: string = '', files: ContextualUpload[] = [],
      mode: ComposeboxMode = ComposeboxMode.DEFAULT) {
    if (text) {
      this.input_ = text;
      this.lastQueriedInput_ = text;
    }
    if (this.showZps && files.length === 0) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }
    if (files.length > 0) {
      this.$.context.setContextFiles(files);
    }
    if (mode !== ComposeboxMode.DEFAULT) {
      this.$.context.setInitialMode(mode);
    }
  }

  protected computeCancelButtonTitle_() {
    return this.input_.trim().length > 0 || this.contextFilesSize_ > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  private computeShowDropdown_() {
    // Don't show dropdown if there's multiple files.
    if (this.contextFilesSize_ > 1) {
      return false;
    }

    // Don't show dropdown if there's no results.
    if (!this.result_?.matches.length) {
      return false;
    }

    // Do not show dropdown if there's an error scrim.
    if (this.errorScrimVisible_) {
      return false;
    }

    if (this.showTypedSuggest_ && this.lastQueriedInput_.trim()) {
      // If context is present, but not enabled, continue to avoid showing the
      // dropdown.
      if (!this.showTypedSuggestWithContext_ && this.contextFilesSize_ > 0) {
        return false;
      }
      // Do not show the dropdown for multiline input or if only the verbatim
      // match is present (we always expect a verbatim
      // match for typed suggest, so we ensure the length of the matches is >1).
      if (this.$.input.scrollHeight <= 48 && this.result_?.matches.length > 1) {
        return true;
      }
    }

    // lastQueriedInput_ is used here since the input_ changes based on
    // the selected match. If typed suggest is not enabled and input_ is used,
    // the dropdown will hide if the user keys down over zps matches.
    return this.showZps && !this.lastQueriedInput_;
  }

  private computeSubmitEnabled_() {
    return this.input_.trim().length > 0 ||
        (this.contextFilesSize_ > 0 && this.$.context.hasDeletableFiles());
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

  protected shouldShowVoiceSearch_(): boolean {
    const isExpanded = this.showDropdown_ || this.contextFilesSize_ > 0;
    return isExpanded ? this.showVoiceSearchInExpandedComposebox_ :
                        this.showVoiceSearchInSteadyComposebox_;
  }

  protected shouldShowVoiceSearchAnimation_(): boolean {
    return !this.disableVoiceSearchAnimation && this.shouldShowVoiceSearch_();
  }

  protected onFileValidationError_(e: CustomEvent<{errorMessage: string}>) {
    this.$.errorScrim.setErrorMessage(e.detail.errorMessage);
  }

  protected onTranscriptUpdate_(e: CustomEvent<string>) {
    // Update property that is sent to searchAnimatedGlow binding.
    this.transcript_ = e.detail;
  }

  protected onSpeechReceived_() {
    // Update property that is sent to searchAnimatedGlow binding.
    this.receivedSpeech_ = true;
  }

  protected async deleteContext_(
      e: CustomEvent<
          {uuid: UnguessableToken, fromAutoSuggestedChip?: boolean}>) {
    // If we're in create image mode, notify that image is gone.
    if (this.inCreateImageMode_) {
      await this.setCreateImageMode_({
        detail: {
          inCreateImageMode: true,
          imagePresent: this.$.context.hasImageFiles(),
        },
      } as CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>);
    }
    this.pendingUploads_.delete(e.detail.uuid);
    this.fileUploadsComplete = this.pendingUploads_.size === 0;
    this.searchboxHandler_.deleteContext(
        e.detail.uuid, e.detail.fromAutoSuggestedChip || false);
    this.focusInput();
    this.queryAutocomplete(/* clearMatches= */ true);
  }

  protected async addFileContext_(e: CustomEvent<{
    files: File[],
    onContextAdded: (files: Map<UnguessableToken, ComposeboxFile>) => void,
  }>) {
    const composeboxFiles: Map<UnguessableToken, ComposeboxFile> = new Map();
    for (const file of e.detail.files) {
      const fileBuffer = await file.arrayBuffer();
      const bigBuffer:
          BigBuffer = {bytes: Array.from(new Uint8Array(fileBuffer))};
      const {token} = await this.searchboxHandler_.addFileContext(
          {
            fileName: file.name,
            imageDataUrl: null,
            mimeType: file.type,
            isDeletable: true,
            selectionTime: new Date(),
          },
          bigBuffer);

      const attachment: ComposeboxFile = {
        uuid: token,
        name: file.name,
        dataUrl: null,
        objectUrl: file.type.includes('image') ? URL.createObjectURL(file) :
                                                 null,
        type: file.type,
        status: FileUploadStatus.kNotUploaded,
        url: null,
        tabId: null,
        isDeletable: true,
      };
      this.pendingUploads_.add(token);
      composeboxFiles.set(token, attachment);
      const announcer = getAnnouncerInstance();
      announcer.announce(this.i18n('composeboxFileUploadStartedText'));
    }
    this.fileUploadsComplete = false;
    e.detail.onContextAdded(composeboxFiles);
    this.focusInput();
  }

  protected addFileContextFromBrowser_(
      uuid: UnguessableToken, fileInfo: SelectedFileInfo) {
    const attachment: ComposeboxFile = {
      uuid: uuid,
      name: fileInfo.fileName,
      dataUrl: fileInfo.imageDataUrl ?? null,
      objectUrl: null,
      type: fileInfo.imageDataUrl ? 'image' : 'pdf',
      status: fileInfo.imageDataUrl ? FileUploadStatus.kUploadSuccessful :
                                      FileUploadStatus.kNotUploaded,
      url: null,
      tabId: null,
      isDeletable: fileInfo.isDeletable,
    };

    this.$.context.onFileContextAdded(attachment);
  }

  private updateAutoSuggestedTabContext_(tab: TabInfo|null) {
    this.$.context.updateAutoActiveTabContext(tab);
  }

  protected async addTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    onContextAdded: (file: ComposeboxFile) => void,
  }>) {
    const {token} = await this.searchboxHandler_.addTabContext(
        e.detail.id, e.detail.delayUpload);
    if (!token) {
      return;
    }

    const attachment: ComposeboxFile = {
      uuid: token,
      name: e.detail.title,
      dataUrl: null,
      objectUrl: null,
      type: 'tab',
      status: FileUploadStatus.kNotUploaded,
      url: e.detail.url,
      tabId: e.detail.id,
      isDeletable: true,
    };
    e.detail.onContextAdded(attachment);
    this.focusInput();
  }

  protected onPaste_(event: ClipboardEvent) {
    if (!event.clipboardData?.items) {
      return;
    }

    const dataTransfer = new DataTransfer();

    for (const item of event.clipboardData.items) {
      if (item.kind === 'file') {
        const file = item.getAsFile();
        if (file) {
          dataTransfer.items.add(file);
        }
      }
    }

    const fileList: FileList = dataTransfer.files;

    if (fileList.length > 0) {
      event.preventDefault();
      this.$.context.addPastedFiles(fileList);
    }
  }

  protected async refreshTabSuggestions_() {
    const {tabs} = await this.searchboxHandler_.getRecentTabs();
    this.tabSuggestions = [...tabs];
  }

  protected async getTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.searchboxHandler_.getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected voiceSearchEndCleanup_() {
    this.inVoiceSearchMode_ = false;
    this.animationState = GlowAnimationState.NONE;
  }

  protected async onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    e.stopPropagation();
    this.voiceSearchEndCleanup_();
    if (this.autoSubmitVoiceSearch) {
      this.fire(
          'voice-search-action', {value: VoiceSearchAction.QUERY_SUBMITTED});
      this.searchboxHandler_.submitQuery(
          e.detail, /*mouse_button=*/ 0, /*alt_key=*/ false,
          /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false);
    } else {
      // If auto-submit is not enabled, update the input to the voice search
      // query, clear autocomplete matches, and recompute whether submission
      // should be enabled.
      this.input_ = e.detail;
      this.clearAutocompleteMatches();
      this.submitEnabled_ = this.computeSubmitEnabled_();
      this.isVoiceInput_ = true;
      await this.updateComplete;
      this.focusInput();
    }
  }

  protected openAimVoiceSearch_() {
    this.inVoiceSearchMode_ = true;
    this.animationState = GlowAnimationState.LISTENING;
    this.fire('voice-search-action', {value: VoiceSearchAction.ACTIVATE});
    this.$.voiceSearch.start();
  }

  protected onVoiceSearchClose_() {
    this.voiceSearchEndCleanup_();
    this.receivedSpeech_ = false;
  }

  protected onCancelClick_() {
    if (this.hasContent_()) {
      this.resetModes();
      this.clearAllInputs(/* querySubmitted= */ false);
      this.focusInput();
      this.queryAutocomplete(/* clearMatches= */ true);
    } else {
      this.closeComposebox_();
    }
  }

  handleEscapeKeyLogic(): void {
    if (!this.composeboxCloseByEscape_ && this.hasContent_()) {
      this.resetModes();
      this.clearAllInputs(/* querySubmitted= */ false);
      this.focusInput();
      this.queryAutocomplete(/* clearMatches= */ true);
    } else {
      this.closeComposebox_();
    }
  }

  private hasContent_(): boolean {
    return this.inDeepSearchMode_ || this.inCreateImageMode_ ||
        this.input_.trim().length > 0 || this.contextFilesSize_ > 0;
  }

  protected onLensClick_() {
    if (this.lensButtonTriggersOverlay) {
      this.pageHandler_.handleLensButtonClick();
    } else {
      this.pageHandler_.handleFileUpload(/*is_image=*/ true);
    }
  }

  protected onOpenFileDialog_(e: CustomEvent<{isImage: boolean}>) {
    this.pageHandler_.handleFileUpload(e.detail.isImage);
  }

  protected onLensIconMouseDown_(e: MouseEvent) {
    // Prevent the composebox from expanding due to being focused by capturing
    // the mousedown event. This is needed to allow the Lens icon to be
    // clicked when the composebox does not have focus without expanding the
    // composebox.
    e.preventDefault();
  }

  private updateInputPlaceholder_() {
    if (this.inDeepSearchMode_) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeDeepSearchPlaceholder');
    } else if (this.inCreateImageMode_) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeCreateImagePlaceholder');
    } else {
      this.inputPlaceholder_ =
          loadTimeData.getString('searchboxComposePlaceholder');
    }
  }

  protected async setDeepSearchMode_(
      e: CustomEvent<{inDeepSearchMode: boolean}>) {
    this.inDeepSearchMode_ = e.detail.inDeepSearchMode;
    this.pageHandler_.setDeepSearchMode(e.detail.inDeepSearchMode);
    this.queryAutocomplete(/* clearMatches= */ true);
    this.updateInputPlaceholder_();

    await this.updateComplete;
    this.focusInput();
  }

  protected async setCreateImageMode_(
      e: CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>) {
    this.inCreateImageMode_ = e.detail.inCreateImageMode;
    this.pageHandler_.setCreateImageMode(
        e.detail.inCreateImageMode, e.detail.imagePresent);
    this.queryAutocomplete(/* clearMatches= */ true);
    this.updateInputPlaceholder_();

    await this.updateComplete;
    this.focusInput();
  }

  protected onErrorScrimVisibilityChanged_(
      e: CustomEvent<{showErrorScrim: boolean}>) {
    this.errorScrimVisible_ = e.detail.showErrorScrim;
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected handleInput_(e: Event) {
    const inputElement = e.target as HTMLInputElement;
    if (inputElement.value === '') {
      this.isVoiceInput_ = false;
    }
    this.input_ = inputElement.value;
    // `clearMatches` is true if input is empty stop any in progress providers
    // before requerying for on-focus (zero-suggest) inputs. The searchbox
    // doesn't allow zero-suggest requests to be made while the ACController
    // is not done.
    if (this.composeboxNoFlickerSuggestionsFix_) {
      // If the composebox no flickering fix is enabled, stop the ACController
      // from querying for suggestions when the input is empty, but don't clear
      // the matches so the dropdown doesn't close.
      if (this.input_ === '') {
        this.searchboxHandler_.stopAutocomplete(/*clearResult=*/ true);
      }
      this.queryAutocomplete(/* clearMatches= */ false);
    } else {
      this.queryAutocomplete(/* clearMatches= */ this.input_ === '');
    }
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
      if ((e.key === 'ArrowDown' || e.key === 'ArrowUp') &&
          !this.showDropdown_) {
        return;
      }

      if (e.key === 'Tab') {
        // If focus leaves the input, unselect the first match.
        if (e.shiftKey) {
          this.$.matches.unselect();
        } else if (this.smartComposeEnabled_ && this.smartComposeInlineHint_) {
          this.input_ = this.input_ + this.smartComposeInlineHint_;
          this.smartComposeInlineHint_ = '';
          e.preventDefault();
          this.queryAutocomplete(/* clearMatches= */ true);
        }
        return;
      }
    }

    if (e.key === 'Enter' && this.submitEnabled_ && this.fileUploadsComplete) {
      if (this.shadowRoot.activeElement === this.$.matches || !e.shiftKey) {
        e.preventDefault();
        this.submitQuery_(e);
      }
    }

    if (e.key === 'Escape') {
      this.handleEscapeKeyLogic();
      e.stopPropagation();
      e.preventDefault();
      return;
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
    } else if (e.key === 'Tab') {
      // If focus goes past the last match, unselect the last match.
      if (this.selectedMatchIndex_ === this.result_.matches.length - 1) {
        if (this.selectedMatch_!.supportsDeletion) {
          const focusedMatchElem =
              this.shadowRoot.activeElement?.shadowRoot?.activeElement;
          const focusedButtonElem = focusedMatchElem?.shadowRoot?.activeElement;
          if (focusedButtonElem?.id === 'remove') {
            this.$.matches.unselect();
          }
        } else {
          this.$.matches.unselect();
        }
      }
      return;
    }
    this.smartComposeInlineHint_ = '';
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }
  }

  protected handleInputFocusIn_() {
    // if there's a last queried input, it's guaranteed that at least
    // the verbatim match will exist.
    if (this.lastQueriedInput_ && this.result_?.matches.length) {
      this.$.matches.selectFirst();
    }
    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-input-focus-changed', {value: true});
    }
  }

  protected handleInputFocusOut_() {
    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-input-focus-changed', {value: false});
    }
  }

  protected handleComposeboxFocusIn_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }

    // If the composebox was focused out, collapsed and now focused in,
    // requery autocomplete to get fresh contextual suggestions.
    if (this.isCollapsible) {
      this.queryAutocomplete(/* clearMatches= */ true);
    }

    this.expanding_ = true;
    this.pageHandler_.focusChanged(true);
    this.fire('composebox-focus-in');
  }

  protected handleComposeboxFocusOut_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    // If the the composebox is collapsible and empty, collapse it.
    // Else, keep the composebox expanded.
    this.expanding_ = this.isCollapsible ? this.submitEnabled_ : true;
    this.pageHandler_.focusChanged(false);
    this.fire('composebox-focus-out');
  }

  protected handleScroll_() {
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    if (!smartCompose) {
      return;
    }
    smartCompose.scrollTop = this.$.input.scrollTop;
  }

  protected handleSubmitFocusIn_() {
    // Matches should always be greater than 0 due to verbatim match.
    if (this.input_ && !this.selectedMatch_) {
      this.$.matches.selectFirst();
    }
  }

  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input_ = context.input;
      }
      this.$.context.addSearchContext(context);
    }
    // Query for ZPS even if there's no context.
    if (this.showZps) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }
  }

  private closeComposebox_() {
    this.resetModes();
    this.fire('close-composebox', {composeboxText: this.input_});

    if (this.isCollapsible) {
      this.expanding_ = false;
      this.animationState = GlowAnimationState.NONE;
      this.$.input.blur();
    }
  }

  protected submitQuery_(e: KeyboardEvent|MouseEvent) {
    // If the submit button is disabled, do nothing.
    if (!this.submitEnabled_) {
      return;
    }

    // Users are allowed to submit queries that consist of only files with no
    // input. `selectedMatchIndex_` will be >= 0 when there is non-empty input
    // since the verbatim match is present.
    assert(
        (this.selectedMatchIndex_ >= 0 && this.result_) ||
            this.contextFilesSize_ > 0 || this.isVoiceInput_,
        'Cannot submit query with no autocomplete matches and no files in ' +
            'context.');

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
      this.searchboxHandler_.submitQuery(
          this.input_.trim(), (e as MouseEvent).button || 0, e.altKey,
          e.ctrlKey, e.metaKey, e.shiftKey);
    }

    this.isVoiceInput_ = false;
    this.animationState = GlowAnimationState.SUBMITTING;

    // If the composebox is expandable, collapse it and clear the input after
    // submitting.
    if (this.isCollapsible || this.clearAllInputsWhenSubmittingQuery_) {
      this.clearAllInputs(/* querySubmitted= */ true);
    }

    if (this.isCollapsible) {
      this.submitEnabled_ = this.computeSubmitEnabled_();
      assert(!this.submitEnabled_);
      this.$.input.blur();
    }
    this.fire('composebox-submit');
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail.index);
  }

  protected onMatchClick_() {
    this.clearAutocompleteMatches();
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex_ = e.detail.value;
    this.selectedMatch_ =
        this.result_?.matches[this.selectedMatchIndex_] || null;
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  clearAutocompleteMatches() {
    this.showDropdown_ = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.searchboxHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = '';
  }

  getRemainingFilesToUpload(): Set<string> {
    return this.pendingUploads_;
  }

  setPendingUploads(files: string[]) {
    this.pendingUploads_ = new Set(files);
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !== result.input) {
      return;
    }

    // TODO(crbug.com/460888279): This is a temporary, merge safe fix. Ideally,
    // the ACController is not sending multiple responses for a single query,
    // especially when the matches is empty. Remove this logic once a long term
    // fix is found.
    if (this.composeboxNoFlickerSuggestionsFix_ && this.showTypedSuggest_ &&
        !this.haveReceivedAutcompleteResponse_) {
      // The first autcomplete response for ZPS contains no matches, since
      // composebox doesn't support ZPS from local providers (ex. history
      // suggestion). Similarly, since composebox doesn't support local
      // providers, typed suggest first response returns a single verbatim
      // match, which doesn't show in the dropdown. To prevent closing the
      // dropdown before the actual response from the suggest server is
      // received, add the previous non-verbatim matches to this first response.
      if (this.result_ && this.result_.matches.length > 0 &&
          result.matches.length <= 1) {
        result.matches.push(...this.result_.matches.filter(
            match => match.type !== 'search-what-you-typed'));
      }
      this.haveReceivedAutcompleteResponse_ = true;
    }
    this.haveReceivedAutcompleteResponse_ = true;
    this.result_ = result;
    const hasMatches = this.result_.matches.length > 0;
    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    // Zero suggest matches are not allowed to be default. Therefore, this
    // makes sure zero suggest results aren't focused when they are returned.
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      this.$.matches.selectFirst();
    } else if (
        this.input_.trim() && hasMatches && this.selectedMatchIndex_ >= 0 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input. Don't restore when the
      // user deletes all their input and autocomplete is queried or else the
      // empty input will change to the value of the first result.
      this.$.matches.selectIndex(this.selectedMatchIndex_);

      // Set the selected match since the `selectedMatchIndex_` does not change
      // (and therefore `selectedMatch_` does not get updated since
      // `onSelectedMatchIndexChanged_` is not called).
      this.selectedMatch_ = this.result_.matches[this.selectedMatchIndex_]!;
      this.input_ = this.selectedMatch_.fillIntoEdit;
    } else {
      this.$.matches.unselect();
    }

    // Populate the smart compose suggestion.
    this.smartComposeInlineHint_ = this.result_.smartComposeInlineHint ?
        this.result_.smartComposeInlineHint :
        '';
  }

  private async onContextualInputStatusChanged_(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType) {
    const {file, errorMessage} =
        this.$.context.updateFileStatus(token, status, errorType);
    if (errorMessage) {
      if (!this.$.errorScrim.isErrorScrimShowing()) {
        this.$.errorScrim.setErrorMessage(errorMessage);
      }
    } else if (file) {
      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          this.showZps && !file.type.includes('image')) {
        // Query autocomplete to get contextual suggestions for files.
        this.queryAutocomplete(/* clearMatches= */ true);
      }
      if (file.status === FileUploadStatus.kProcessing) {
        this.pendingUploads_.add(file.uuid);
      }
      const isFinished = file?.status === FileUploadStatus.kValidationFailed ||
          file.status === FileUploadStatus.kUploadSuccessful ||
          file.status === FileUploadStatus.kUploadExpired ||
          file.status === FileUploadStatus.kUploadFailed;
      if (isFinished) {
        this.pendingUploads_.delete(file.uuid);
        this.fileUploadsComplete = this.pendingUploads_.size === 0;
      }
      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          file.type.includes('image')) {
        // If we're in create image mode, update the aim tool mode.
        if (this.inCreateImageMode_) {
          await this.setCreateImageMode_(
              {
                detail: {
                  inCreateImageMode: true,
                  imagePresent: true,
                },
              } as
              CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>);
        } else if (this.enableImageContextualSuggestions_) {
          // Query autocomplete to get contextual suggestions for files.
          this.queryAutocomplete(/* clearMatches= */ true);
        } else {
          this.showDropdown_ = false;
        }
      }

      // Query autocomplete to get contextual suggestions for tabs.
      if (status === FileUploadStatus.kProcessing &&
          file.type.includes('tab')) {
        this.queryAutocomplete(/* clearMatches= */ true);
      }

      if (status === FileUploadStatus.kUploadSuccessful) {
        const announcer = getAnnouncerInstance();
        announcer.announce(this.i18n('composeboxFileUploadCompleteText'));
      }
    }
  }

  private adjustInputForSmartCompose() {
    // Checks the scroll height of the input + smart complete hint (ghost div)
    // and updates the height of the actual input to be that height so the
    // ghost text does not overflow.
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');

    const ghostHeight = smartCompose!.scrollHeight;
    const maxHeight = 190;
    this.$.input.style.height = `${Math.min(ghostHeight, maxHeight)}px`;
    // If smart compose goes to two lines. The tab chip will be cut off as it
    // has a height of 28px. Add 4px to show the whole tab chip.
    if (ghostHeight > 48) {
      this.$.input.style.minHeight = `68px`;
      smartCompose!.style.minHeight = `68px`;
    }

    // If the height of the input + smart complete hint is greater than the max
    // height, scroll the smart compose as the input will already scroll. Note
    // there is an issue at the break point since the input will not have
    // scrolled yet as it does not have enough content. The smart compose will
    // display the ghost text below the input and it will be cut off. However,
    // the current response only works for queries below the max height.
    if (ghostHeight > maxHeight) {
      smartCompose!.scrollTop = this.$.input.scrollTop;
    }
  }

  // `queryAutocomplete` updates the `lastQueriedInput_` and makes an
  // autocomplete call through the handler. It also optionally clears existing
  // matches.
  private queryAutocomplete(clearMatches: boolean) {
    if (this.isVoiceInput_) {
      return;
    }
    if (clearMatches) {
      this.clearAutocompleteMatches();
    }
    this.lastQueriedInput_ = this.input_;
    this.haveReceivedAutcompleteResponse_ = false;
    this.searchboxHandler_.queryAutocomplete(this.input_, false);
  }

  clearAllInputs(querySubmitted: boolean) {
    this.clearInput();
    const remainingFiles = this.$.context.resetContextFiles();
    // Reset files in set to match remaining files in carousel.
    this.setPendingUploads(remainingFiles);
    this.contextFilesSize_ = 0;
    this.smartComposeInlineHint_ = '';
    if (!querySubmitted) {
      // If the query was submitted, the searchbox handler will clear its own
      // uploaded file state when the query submission is handled.
      this.searchboxHandler_.clearFiles();
    }
    this.submitEnabled_ = this.computeSubmitEnabled_();
    assert(!this.submitEnabled_);
    this.fileUploadsComplete = this.pendingUploads_.size === 0;
  }

  clearInput() {
    this.input_ = '';
    this.isVoiceInput_ = false;
  }

  getInputText(): string {
    return this.input_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
