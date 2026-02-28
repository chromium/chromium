// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './composebox_file_inputs.js';
import './composebox_lens_search.js';
import './composebox_tool_chip.js';
import './context_menu_entrypoint.js';
import './contextual_entrypoint_and_menu.js';
import './contextual_entrypoint_button.js';
import './composebox_dropdown.js';
import './composebox_voice_search.js';
import './error_scrim.js';
import './file_carousel.js';
import './file_thumbnail.js';
import './icons.html.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {GlowAnimationState, ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
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
import type {AutocompleteMatch, AutocompleteResult, FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, SelectedFileInfo, TabAttachment, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {FileUploadErrorType, InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ModelMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {FILE_VALIDATION_ERRORS_MAP} from './common.js';
import type {ComposeboxFile, ContextualUpload, TabUpload} from './common.js';
import {recordBoolean, recordContextAdditionMethod, recordEnumerationValue, recordUserAction, TabUploadOrigin} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from './composebox_file_inputs.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {FileUploadStatus, InputType, ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';
import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';
import type {ErrorScrimElement} from './error_scrim.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';

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
    carousel: ComposeboxFileCarouselElement,
    fileInputs: ComposeboxFileInputsElement,
    submitContainer: HTMLElement,
    submitOverlay: HTMLElement,
    matches: ComposeboxDropdownElement,
    errorScrim: ErrorScrimElement,
    voiceSearch: ComposeboxVoiceSearchElement,
  };
}

// LINT.IfChange(FileValidationError)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
const enum ComposeboxFileValidationError {
  NONE = 0,
  TOO_MANY_FILES = 1,
  FILE_EMPTY = 2,
  FILE_SIZE_TOO_LARGE = 3,
  MAX_VALUE = FILE_SIZE_TOO_LARGE,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:FileValidationError)

// These values are sorted by precedence. The error with the highest value
// will be the one shown to the user if multiple errors apply.
enum ProcessFilesError {
  NONE = 0,
  INVALID_TYPE = 1,
  FILE_TOO_LARGE = 2,
  FILE_EMPTY = 3,
  MAX_FILES_EXCEEDED = 4,
  MAX_IMAGES_EXCEEDED = 5,
  MAX_PDFS_EXCEEDED = 6,
  FILE_UPLOAD_NOT_ALLOWED = 7,
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
      suggestionActivityEnabled: {type: Boolean},
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
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      activeToolMode_: {
        type: Number,
        reflect: true,
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
      tabSuggestions_: {type: Array},
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
      },
      errorMessage_: {type: String},
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
      showMenuOnClick: {type: Boolean},
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
      fileUploadsComplete: {
        type: Boolean,
        reflect: true,
      },
      canSubmitFilesAndInput_: {
        type: Boolean,
        reflect: true,
      },
      showModelPicker: {type: Boolean},
      inputState_: {type: Object},
      showModelPicker_: {
        type: Boolean,
        reflect: true,
      },
      hasAllowedInputs_: {
        type: Boolean,
        reflect: true,
      },
      enableCarouselScrolling: {type: Boolean},
      inputPlaceholderOverride: {type: String},
      files_: {type: Object},
      contextMenuEnabled_: {type: Boolean},
      addedTabsIds_: {type: Object},
      showContextMenuDescription_: {type: Boolean},
      uploadButtonDisabled_: {
        type: Boolean,
        reflect: true,
      },
      isOmniboxInCompactMode_: {
        type: Boolean,
        reflect: true,
      },
      isFollowupQuery: {type: Boolean},
      shouldShowGhostFiles: {type: Boolean},
    };
  }

  accessor isFollowupQuery: boolean = false;
  accessor inputPlaceholderOverride: string = '';
  accessor suggestionActivityEnabled: boolean = true;
  accessor disableCaretColorAnimation: boolean = false;
  accessor disableComposeboxAnimation: boolean = false;
  accessor enableCarouselScrolling: boolean = false;
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
  accessor showMenuOnClick: boolean = true;
  accessor entrypointName: string = '';
  accessor disableVoiceSearchAnimation: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  accessor lensButtonDisabled: boolean = false;
  // Set this to true in parent component if it is desired
  // to show files that are not in the file map when
  // file status is updated from backend. Ghost files will be
  // shown as image chip with spinner in file carousel.
  accessor shouldShowGhostFiles: boolean = false;

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
  protected accessor activeToolMode_: ComposeboxToolMode =
      ComposeboxToolMode.kUnspecified;
  protected accessor errorMessage_: string = '';
  protected accessor transcript_: string = '';
  protected accessor receivedSpeech_: boolean = false;
  protected accessor canSubmitFilesAndInput_: boolean = true;
  protected accessor inputState_: InputState|null = null;
  protected lastQueriedInput_: string = '';
  protected showVoiceSearchInSteadyComposebox_: boolean =
      loadTimeData.getBoolean('steadyComposeboxShowVoiceSearch');
  protected showVoiceSearchInExpandedComposebox_: boolean =
      loadTimeData.getBoolean('expandedComposeboxShowVoiceSearch');
  protected accessor showModelPicker_: boolean =
      loadTimeData.valueExists('contextualMenuUsePecApi') ?
      loadTimeData.getBoolean('contextualMenuUsePecApi') :
      false;
  protected accessor hasAllowedInputs_: boolean = false;
  protected accessor files_: Map<UnguessableToken, ComposeboxFile> = new Map();
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor addedTabsIds_: Map<number, UnguessableToken> = new Map();
  protected accessor uploadButtonDisabled_: boolean = false;
  protected contextMenuDescriptionEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  protected accessor showContextMenuDescription_: boolean =
      this.contextMenuDescriptionEnabled_;
  protected accessor isOmniboxInCompactMode_: boolean = false;
  protected dragAndDropHandler_: DragAndDropHandler;
  private showTypedSuggest_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggest');
  private showTypedSuggestWithContext_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggestWithContext');
  private showZps: boolean = loadTimeData.getBoolean('composeboxShowZps');
  // Determines whether to query zps when the composebox is rendered.
  private queryZpsOnLoad: boolean = loadTimeData.valueExists('queryZpsOnLoad') ?
      loadTimeData.valueExists('queryZpsOnLoad') :
      true;
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
  private pendingUploads_: Set<string> = new Set<string>([]);
  private contextMenuOpened_: boolean = false;
  private automaticActiveTab_: ComposeboxFile|null = null;
  private composeboxSource_: string =
      loadTimeData.getString('composeboxSource');
  private maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private maxFileSize_: number =
      loadTimeData.getInteger('composeboxFileMaxSize');
  private attachmentFileTypes_: string[] =
      loadTimeData.getString('composeboxAttachmentFileTypes').split(',');
  private imageFileTypes_: string[] =
      loadTimeData.getString('composeboxImageFileTypes').split(',');

  protected get inToolMode_(): boolean {
    return this.activeToolMode_ !== ComposeboxToolMode.kUnspecified;
  }

  protected get shouldShowDivider_(): boolean {
    // TODO(crbug.com/476175193): Remove `entrypointName` condition.
    if (this.entrypointName === 'Omnibox' &&
        this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel_) {
      return false;
    }

    return this.showDropdown_ &&
        (this.showFileCarousel_ ||
         this.searchboxLayoutMode === 'TallTopContext' ||
         this.shouldShowSubmitButton_);
  }

  protected get shouldShowSubmitButton_(): boolean {
    return this.searchboxNextEnabled && this.submitEnabled_;
  }

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
    this.dragAndDropHandler_ =
        new DragAndDropHandler(this, this.dragAndDropEnabled_);
  }

  override async connectedCallback() {
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
      this.searchboxCallbackRouter_.onInputStateChanged.addListener(
          this.onInputStateChanged_.bind(this)),
    ];

    this.eventTracker_.add(this.$.input, 'input', () => {
      this.submitEnabled_ = this.computeSubmitEnabled_();
    });
    this.focusInput();
    // For "next" searchboxes (Realbox Next, Omnibox Next, etc.), the zps
    // autocomplete query is triggered after the state has been initialized.
    if (this.queryZpsOnLoad && !this.searchboxNextEnabled) {
      this.queryAutocomplete_(/* clearMatches= */ false);
    }

    this.searchboxHandler_.notifySessionStarted();

    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-initialized', {
        initializeComposeboxState: this.initializeState_.bind(this),
      });
    } else {
      this.inputState_ = (await this.searchboxHandler_.getInputState()).state;
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
    if (changedProperties.has('entrypointName') ||
        changedProperties.has('searchboxLayoutMode')) {
      this.isOmniboxInCompactMode_ = this.entrypointName === 'Omnibox' &&
          this.searchboxLayoutMode === 'Compact';
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    // When the result initially gets set check if dropdown should show.
    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('files_') ||
        changedPrivateProperties.has('errorMessage_')) {
      this.showFileCarousel_ = this.files_.size > 0;
      this.showDropdown_ = this.computeShowDropdown_();
    }
    if (changedPrivateProperties.has('submitEnabled_') ||
        changedPrivateProperties.has('fileUploadsComplete')) {
      this.uploadButtonDisabled_ = !this.fileUploadsComplete;
      this.canSubmitFilesAndInput_ =
          this.submitEnabled_ && this.fileUploadsComplete;
    }

    if (changedPrivateProperties.has('inputState_')) {
      this.hasAllowedInputs_ = !!this.inputState_ &&
          (this.inputState_.allowedModels.length > 0 ||
           this.inputState_.allowedTools.length > 0 ||
           this.inputState_.allowedInputTypes.length > 0);
    }

    if (changedPrivateProperties.has('inputPlaceholderOverride')) {
      this.updateInputPlaceholder_();
    }
  }
  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedMatchIndex_')) {
      if (this.selectedMatch_) {
        // Update the input.
        const text = this.selectedMatch_.fillIntoEdit;
        this.input_ = text;
      } else if (!this.lastQueriedInput_) {
        // This is for cases when focus leaves the matches/input.
        // If there was already text in the input do not clear it.
        this.clearInput();
      } else {
        // For typed queries reset the input back to typed value when
        // focus leaves the match.
        this.input_ = this.lastQueriedInput_;
      }
    }

    if (changedPrivateProperties.has('selectedMatchIndex_') ||
        changedPrivateProperties.has('inputState_') ||
        changedPrivateProperties.has('isFollowupQuery') ||
        changedPrivateProperties.has('files_')) {
      this.submitEnabled_ = this.computeSubmitEnabled_();
      this.canSubmitFilesAndInput_ =
          this.submitEnabled_ && this.fileUploadsComplete;
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
    return this;
  }

  addDroppedFiles(files: FileList|null) {
    this.processFiles_(files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.DRAG_AND_DROP, this.composeboxSource_);
  }

  focusInput() {
    this.$.input.focus();
  }

  getText() {
    return this.input_;
  }

  queryAutocomplete(clearMatches: boolean) {
    this.queryAutocomplete_(clearMatches);
  }

  protected onQueryAutocomplete_(e: CustomEvent<{clearMatches: boolean}>) {
    this.queryAutocomplete_(e.detail.clearMatches);
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
    const previousTool = this.activeToolMode_;
    this.activeToolMode_ = ComposeboxToolMode.kUnspecified;
    this.uploadButtonDisabled_ = false;

    if (previousTool !== ComposeboxToolMode.kUnspecified) {
      this.showContextMenuDescription_ = this.contextMenuDescriptionEnabled_;
      this.handleToolModeUpdate_();
    }
  }

  setDefaultModel() {
    if (this.inputState_?.activeModel === ModelMode.kUnspecified) {
      this.searchboxHandler_.setActiveModelMode(
          this.inputState_?.allowedModels[0] ?? ModelMode.kUnspecified);
    }
  }

  resetToolsAndModels() {
    if (this.inputState_) {
      this.searchboxHandler_.setActiveToolMode(ComposeboxToolMode.kUnspecified);
      this.searchboxHandler_.setActiveModelMode(ModelMode.kUnspecified);
    }
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
    return this.automaticActiveTab_ !== null;
  }

  getAutomaticActiveTabChipElement(): HTMLElement|null {
    if (!this.automaticActiveTab_) {
      return null;
    }
    const carousel =
        this.shadowRoot?.querySelector<ComposeboxFileCarouselElement>(
            '#carousel');
    if (!carousel) {
      return null;
    }

    return carousel.getThumbnailElementByUuid(this.automaticActiveTab_.uuid);
  }

  protected async initializeState_(
      text: string = '', files: ContextualUpload[] = [],
      mode: ComposeboxToolMode = ComposeboxToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified,
      inputState: InputState|null = null) {
    if (text) {
      this.input_ = text;
      this.lastQueriedInput_ = text;
    }
    if (this.showZps && files.length === 0) {
      this.queryAutocomplete_(/* clearMatches= */ false);
    }
    this.inputState_ = inputState ?
        inputState :
        (await this.searchboxHandler_.getInputState()).state;
    if (files.length > 0) {
      const dataTransfer = new DataTransfer();
      for (const file of files) {
        if ('tabId' in file) {
          // If the composebox is being initialized with tab context from the
          // context menu, we want to keep the context menu open to allow for
          // multi-tab selection.
          const entrypointAndMenu =
              this.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
                  '#contextEntrypoint');
          if (entrypointAndMenu &&
              file.origin === TabUploadOrigin.CONTEXT_MENU) {
            entrypointAndMenu.openMenuForMultiSelection();
          }
          this.addTabContextHandleCallback_({
            tabId: file.tabId,
            title: file.title,
            url: file.url,
            delayUpload: file.delayUpload,
            replaceAutoActiveTabToken: false,
            origin: file.origin,
          } as TabUpload);
        } else {
          dataTransfer.items.add(file.file);
        }
      }
      this.processFiles_(dataTransfer.files);
    }
    if (mode !== ComposeboxToolMode.kUnspecified) {
      this.handleToolClick_(mode);
    }
    if (model !== ModelMode.kUnspecified) {
      this.searchboxHandler_.setActiveModelMode(model);
    } else if (this.inputState_ && this.inputState_.allowedModels.length > 0) {
      this.searchboxHandler_.setActiveModelMode(this.inputState_.allowedModels[0]!);
    }
    this.updateInputPlaceholder_();
  }

  protected addToPendingUploads_(token: UnguessableToken) {
    this.pendingUploads_.add(token);
    this.fileUploadsComplete = false;
  }

  protected computeCancelButtonTitle_() {
    return this.input_.trim().length > 0 || this.files_.size > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  protected onContextMenuContainerMouseDown_(e: FocusEvent) {
    // Special treatment for the "Tall" layout variants where not clicking on an
    // inner element should be treated as clicking on a non-focusable area.
    if (this.searchboxLayoutMode !== 'Compact' &&
        (e.target instanceof HTMLElement &&
         e.target.id === 'contextMenuContainer')) {
      e.preventDefault();
      e.stopPropagation();
    }
  }

  protected onContextMenuContainerClick_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();

    // Ignore non-primary button clicks.
    if (e.button !== 0) {
      return;
    }

    if (this.searchboxLayoutMode !== 'Compact') {
      this.focusInput();
    }
  }

  private computeShowDropdown_() {
    // Don't show dropdown if there's multiple files.
    if (this.files_.size > 1) {
      return false;
    }

    // Don't show dropdown if there's no results.
    if (!this.result_?.matches.length) {
      return false;
    }

    // Do not show dropdown if there's an error scrim.
    if (this.errorMessage_ !== '') {
      return false;
    }

    // Do not show dropdown if there's an image and contextual image suggestions
    // are disabled.
    if (!this.enableImageContextualSuggestions_ && this.hasImageFiles_()) {
      return false;
    }

    if (this.showTypedSuggest_ && this.lastQueriedInput_.trim()) {
      // If context is present, but not enabled, continue to avoid showing the
      // dropdown.
      if (!this.showTypedSuggestWithContext_ && this.files_.size > 0) {
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

  private hasValidQuery_() {
    // If there are files or an autocomplete match is selected, it's a valid
    // query.
    if (this.files_.size > 0 ||
        (this.selectedMatchIndex_ >= 0 && !!this.result_)) {
      return true;
    }

    if (this.input_.trim().length > 0) {
      return true;
    }

    // TODO(crbug.com/485648942): Update to drive Deep Search behavior from the
    // PEC API's ToolSubstateConfig.
    // Allow empty query for Deep Search follow-ups.
    if (this.inputState_?.activeTool === ComposeboxToolMode.kDeepSearch &&
        this.isFollowupQuery) {
      return true;
    }

    return false;
  }

  private computeSubmitEnabled_() {
    return this.hasValidQuery_();
  }

  protected shouldShowSuggestionActivityLink_() {
    const showActivityLink = this.result_ && this.showDropdown_ &&
        this.result_.matches.some((match) => match.isNoncannedAimSuggestion);
    this.fire('show-suggestion-activity-link', showActivityLink);
    return showActivityLink;
  }

  protected shouldShowSmartComposeInlineHint_() {
    return !!this.smartComposeInlineHint_;
  }

  protected shouldShowVoiceSearch_(): boolean {
    const isExpanded = this.showDropdown_ || this.files_.size > 0;
    return isExpanded ? this.showVoiceSearchInExpandedComposebox_ :
                        this.showVoiceSearchInSteadyComposebox_;
  }

  protected shouldShowVoiceSearchAnimation_(): boolean {
    return !this.disableVoiceSearchAnimation && this.shouldShowVoiceSearch_();
  }

  protected onTranscriptUpdate_(e: CustomEvent<string>) {
    // Update property that is sent to searchAnimatedGlow binding.
    this.transcript_ = e.detail;
  }

  protected onSpeechReceived_() {
    // Update property that is sent to searchAnimatedGlow binding.
    this.receivedSpeech_ = true;
  }

  deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean) {
    if (!uuidToDelete || !this.files_.has(uuidToDelete)) {
      return;
    }

    const file = this.files_.get(uuidToDelete);
    if (file?.tabId) {
      this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
          ([id, _]) => id !== file.tabId));
    }

    const fromAutoSuggestedChip =
        uuidToDelete === this.automaticActiveTab_?.uuid &&
        (fromUserAction === true);
    if (fromAutoSuggestedChip) {
      const metricName = 'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
          this.composeboxSource_;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.automaticActiveTab_ = null;
    }

    this.files_ = new Map([...this.files_.entries()].filter(
        ([uuid, _]) => uuid !== uuidToDelete));
    // If we're in create image mode, notify that image is gone.
    if (this.activeToolMode_ === ComposeboxToolMode.kImageGen) {
      this.handleToolModeUpdate_();
    }
    this.pendingUploads_.delete(uuidToDelete);
    this.fileUploadsComplete = this.pendingUploads_.size === 0;
    this.searchboxHandler_.deleteContext(uuidToDelete, fromAutoSuggestedChip);
    this.focusInput();
    this.queryAutocomplete_(/* clearMatches= */ true);
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource_);
  }

  // Start file upload flow from frontend. This contrasts with
  // `onFileContextAdded_`, which is for the file upload flow that is started
  // from the backend.
  private async addFileContext_(files: File[]) {
    const composeboxFiles: Map<UnguessableToken, ComposeboxFile> = new Map();
    for (const file of files) {
      const fileBuffer = await file.arrayBuffer();
      const bigBuffer:
          BigBuffer = {bytes: Array.from(new Uint8Array(fileBuffer))};
      let token: UnguessableToken;
      try {
        token = await this.searchboxHandler_.addFileContext(
            {
              fileName: file.name,
              imageDataUrl: null,
              mimeType: file.type,
              isDeletable: true,
              selectionTime: new Date(),
            },
            bigBuffer);
      } catch (e) {
        const err = e as FileUploadErrorType;
        if (FILE_VALIDATION_ERRORS_MAP.has(err)) {
          this.errorMessage_ = this.i18n(FILE_VALIDATION_ERRORS_MAP.get(err)!);
        }
        continue;
      }

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
      composeboxFiles.set(token, attachment);
      const announcer = getAnnouncerInstance();
      announcer.announce(this.i18n('composeboxFileUploadStartedText'));
    }
    this.files_ =
        new Map([...this.files_.entries(), ...composeboxFiles.entries()]);
    this.recordFileValidationMetric_(ComposeboxFileValidationError.NONE);
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

    this.onFileContextAdded_(attachment);
  }

  injectInput(title: string, thumbnail: string, fileToken: UnguessableToken) {
    const attachment: ComposeboxFile = {
      uuid: fileToken,
      name: title,
      dataUrl: thumbnail,
      objectUrl: thumbnail,
      type: 'injectedinput',
      status: FileUploadStatus.kUploadSuccessful,
      url: null,
      tabId: null,
      isDeletable: true,
    };

    this.onFileContextAdded_(attachment);
  }

  private updateAutoSuggestedTabContext_(tab: TabInfo|null) {
    const shouldDeleteAutomaticActiveTab = this.automaticActiveTab_ &&
        (!tab || this.automaticActiveTab_.tabId !== tab.tabId);
    if (shouldDeleteAutomaticActiveTab) {
      this.deleteFile(this.automaticActiveTab_!.uuid);
      this.automaticActiveTab_ = null;

      // TODO(crbug.com/482150500): Correctly query for url based suggestions
      // when delayed tab is present. Right now, while url-based suggestions are
      // not set-up, clear the autocomplete matches.
      this.queryAutocomplete_(/* clearMatches= */ true);
    }

    if (tab) {
      // Ignore the `TabInfo` update if there is a matching
      // `automaticActiveTab_`.
      if (this.automaticActiveTab_ &&
          tab.url === this.automaticActiveTab_.url &&
          tab.tabId === this.automaticActiveTab_.tabId) {
        return;
      }

      this.addTabContextHandleCallback_(
          {
            tabId: tab.tabId,
            title: tab.title,
            url: tab.url,
            delayUpload: /*delay_upload=*/ true,
            origin: TabUploadOrigin.AUTO_ACTIVE,
          } as TabUpload,
          /*replaceAutoActiveTabToken=*/ true);

      // Only query autocomplete if we're replacing the current chip or if we're
      // adding a new chip.
      this.queryAutocomplete_(/* clearMatches= */ true);
    }
  }

  private onInputStateChanged_(inputState: InputState) {
    this.inputState_ = inputState;
  }

  protected onDeleteFile_(
      e: CustomEvent<{uuid: UnguessableToken, fromUserAction?: boolean}>) {
    this.deleteFile(e.detail.uuid, e.detail.fromUserAction);
  }

  protected addTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    this.addTabContextHandleCallback_({
      tabId: e.detail.id,
      title: e.detail.title,
      url: e.detail.url,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    } as TabUpload);
  }

  private async addTabContextHandleCallback_(
      tabUpload: TabUpload, replaceAutoActiveTabToken: boolean = false) {
    try {
      const token = await this.searchboxHandler_.addTabContext(
          tabUpload.tabId, tabUpload.delayUpload);

      const attachment: ComposeboxFile = {
        uuid: token,
        name: tabUpload.title,
        dataUrl: null,
        objectUrl: null,
        type: 'tab',
        status: FileUploadStatus.kNotUploaded,
        url: tabUpload.url,
        tabId: tabUpload.tabId,
        isDeletable: true,
      };

      this.files_ = new Map(
          [...this.files_.entries(), [attachment.uuid, attachment]]);
      this.addedTabsIds_ = new Map(
          [...this.addedTabsIds_.entries(), [tabUpload.tabId, attachment.uuid]]);
      if (replaceAutoActiveTabToken) {
        this.automaticActiveTab_ =
            Object.assign(attachment, {uuid: attachment.uuid});
      }
      this.focusInput();

    } catch (e) {
      const err = e as FileUploadErrorType;
      if (FILE_VALIDATION_ERRORS_MAP.has(err)) {
        this.errorMessage_ = this.i18n(FILE_VALIDATION_ERRORS_MAP.get(err)!);
      }
      return;
    }
  }

  protected onPaste_(event: ClipboardEvent) {
    if (!this.dragAndDropEnabled_ || !event.clipboardData?.items) {
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
      this.processFiles_(fileList);
      recordContextAdditionMethod(
          ComposeboxContextAddedMethod.COPY_PASTE, this.composeboxSource_);
    }
  }

  protected async onContextMenuClosed_() {
    this.contextMenuOpened_ = false;

    await this.updateComplete;
    this.focusInput();
  }

  protected onContextMenuOpened_() {
    this.contextMenuOpened_ = true;
    this.refreshTabSuggestions_();
  }

  protected async refreshTabSuggestions_() {
    if (!this.contextMenuOpened_) {
      return;
    }
    const {tabs} = await this.searchboxHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
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
    this.transcript_ = '';
  }

  protected async onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    e.stopPropagation();
    this.voiceSearchEndCleanup_();
    // For contextual tasks composebox voice metrics.
    this.fire('composebox-voice-search-transcription-success');
    if (this.autoSubmitVoiceSearch) {
      this.fire(
          'voice-search-action', {value: VoiceSearchAction.QUERY_SUBMITTED});
      this.input_ = e.detail;
      this.searchboxHandler_.submitQuery(
          e.detail, /*mouse_button=*/ 0, /*alt_key=*/ false,
          /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false);
      this.submitCleanup_();
    } else {
      // If auto-submit is not enabled, update the input to the voice search
      // query, clear autocomplete matches, and recompute whether submission
      // should be enabled.
      this.input_ = e.detail;
      this.queryAutocomplete_(/* clearMatches= */ true);
      this.submitEnabled_ = this.computeSubmitEnabled_();
      await this.updateComplete;
      this.focusInput();
    }
  }

  protected openAimVoiceSearch_() {
    this.inVoiceSearchMode_ = true;
    this.animationState = GlowAnimationState.LISTENING;
    this.fire('voice-search-action', {value: VoiceSearchAction.ACTIVATE});
    // For contextual tasks composebox voice metrics.
    this.fire('composebox-voice-search-start');
    this.$.voiceSearch.start();
  }

  protected onVoiceSearchClose_(e: CustomEvent<boolean>) {
    // If closing was the user canceling voice search:
    if (e.detail) {
      // For contextual tasks composebox voice metrics.
      this.fire('composebox-voice-search-user-canceled');
    }
    this.voiceSearchEndCleanup_();
    this.receivedSpeech_ = false;
  }

  protected onVoiceSearchError_(e: CustomEvent<boolean>) {
    // For contextual tasks composebox voice metrics:
    if (e.detail) {
      // An error that canceled voice search.
      this.fire('composebox-voice-search-error-and-canceled');
    } else {
      // An error that did not cancel voice search.
      this.fire('composebox-voice-search-error');
    }
  }

  protected onCancelClick_() {
    if (this.hasContent_()) {
      this.resetModes();
      this.clearAllInputs(/* querySubmitted= */ false,
                          /* shouldBlockAutoSuggestedTabs= */ true);
      this.focusInput();
      this.queryAutocomplete_(/* clearMatches= */ true);
    } else {
      this.closeComposebox_();
    }
  }

  handleEscapeKeyLogic(): void {
    if (!this.composeboxCloseByEscape_ && this.hasContent_()) {
      this.resetModes();
      this.clearAllInputs(/* querySubmitted= */ false,
                          /* shouldBlockAutoSuggestedTabs= */ false);
      this.focusInput();
      this.queryAutocomplete_(/* clearMatches= */ true);
    } else {
      this.closeComposebox_();
    }
  }

  private hasContent_(): boolean {
    return this.activeToolMode_ !== ComposeboxToolMode.kUnspecified ||
        this.input_.trim().length > 0 || this.files_.size > 0;
  }

  protected onLensClick_() {
    if (this.lensButtonTriggersOverlay) {
      this.pageHandler_.handleLensButtonClick();
    } else {
      this.pageHandler_.handleFileUpload(/*is_image=*/ true);
    }
  }

  protected onLensIconMouseDown_(e: MouseEvent) {
    // Prevent the composebox from expanding due to being focused by capturing
    // the mousedown event. This is needed to allow the Lens icon to be
    // clicked when the composebox does not have focus without expanding the
    // composebox.
    e.preventDefault();
  }

  private updateInputPlaceholder_() {
    if (this.inputPlaceholderOverride) {
      this.inputPlaceholder_ = this.inputPlaceholderOverride;
      return;
    }

    if (this.inputState_) {
      if (this.activeToolMode_ !== ComposeboxToolMode.kUnspecified) {
        const config = this.inputState_.toolConfigs.find(
            c => c.tool === this.activeToolMode_);
        if (config?.hintText) {
          this.inputPlaceholder_ = config.hintText;
          return;
        }
      }

      if (this.inputState_.activeModel !== ModelMode.kUnspecified) {
        const config = this.inputState_.modelConfigs.find(
            c => c.model === this.inputState_!.activeModel);
        if (config?.hintText) {
          this.inputPlaceholder_ = config.hintText;
          return;
        }
      }

      if (this.inputState_.hintText) {
        this.inputPlaceholder_ = this.inputState_.hintText;
        return;
      }
    }

    if (this.activeToolMode_ === ComposeboxToolMode.kDeepSearch) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeDeepSearchPlaceholder');
    } else if (this.activeToolMode_ === ComposeboxToolMode.kImageGen) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeCreateImagePlaceholder');
    } else {
      this.inputPlaceholder_ =
          loadTimeData.getString('searchboxComposePlaceholder');
    }
  }

  get activeToolMode(): ComposeboxToolMode {
    return this.activeToolMode_;
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ComposeboxToolMode}>) {
    this.handleToolClick_(e.detail.toolMode);
  }

  protected handleDeepSearchClick_() {
    this.handleToolClick_(ComposeboxToolMode.kDeepSearch);
  }

  protected handleImageGenClick_() {
    this.handleToolClick_(ComposeboxToolMode.kImageGen);
  }

  protected handleCanvasClick_() {
    this.handleToolClick_(ComposeboxToolMode.kCanvas);
  }

  protected handleToolClick_(tool: ComposeboxToolMode) {
    if (this.contextMenuDescriptionEnabled_) {
      if (this.activeToolMode_ === tool) {
        this.showContextMenuDescription_ = true;
      } else {
        this.showContextMenuDescription_ =
            tool === ComposeboxToolMode.kUnspecified;
      }
    }

    if (this.activeToolMode_ === tool) {
      this.activeToolMode_ = ComposeboxToolMode.kUnspecified;
    } else {
      this.activeToolMode_ = tool;
    }

    this.handleToolModeUpdate_();
  }

  private handleToolModeUpdate_() {
    this.searchboxHandler_.setActiveToolMode(this.activeToolMode_);
    this.queryAutocomplete_(/* clearMatches= */ true);
    this.updateInputPlaceholder_();
    this.fire('active-tool-mode-changed', {value: this.activeToolMode_});
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.searchboxHandler_.setActiveModelMode(e.detail.model);
    this.updateInputPlaceholder_();
  }

  protected onErrorScrimDismissed_() {
    this.errorMessage_ = '';
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected handleInput_(e: Event) {
    const inputElement = e.target as HTMLInputElement;
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
      this.queryAutocomplete_(/* clearMatches= */ false);
    } else {
      this.queryAutocomplete_(/* clearMatches= */ this.input_ === '');
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
          this.queryAutocomplete_(/* clearMatches= */ true);
        }
        return;
      }
    }

    if (e.key === 'Enter' &&
        (this.shadowRoot.activeElement === this.$.matches || !e.shiftKey)) {
      e.preventDefault();
      if (this.canSubmitFilesAndInput_) {
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
      this.selectFirstMatch();
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
    if (this.lastQueriedInput_) {
      this.selectFirstMatch();
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
      this.queryAutocomplete_(/* clearMatches= */ true);
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
      this.selectFirstMatch();
    }
  }

  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input_ = context.input;
      }
      for (const attachment of context.attachments) {
        if (attachment.fileAttachment) {
          this.addFileFromAttachment_(attachment.fileAttachment);
        } else if (attachment.tabAttachment) {
          this.addTabFromAttachment_(attachment.tabAttachment);
        }
      }

      switch (context.toolMode) {
        case ToolMode.kDeepSearch:
          this.handleToolClick_(ComposeboxToolMode.kDeepSearch);
          break;
        case ToolMode.kCreateImage:
          this.handleToolClick_(ComposeboxToolMode.kImageGen);
          break;
        case ToolMode.kCanvas:
          this.handleToolClick_(ComposeboxToolMode.kCanvas);
          break;
        default:
      }
    }
    // Query for ZPS even if there's no context.
    if (this.showZps) {
      this.queryAutocomplete_(/* clearMatches= */ false);
    }
  }

  private closeComposebox_() {
    this.resetModes();
    this.searchboxHandler_.clearFiles(/*shouldBlockAutoSuggestedTabs=*/ false);
    this.resetToolsAndModels();
    this.fire('close-composebox', {composeboxText: this.input_});

    if (this.isCollapsible) {
      this.expanding_ = false;
      this.animationState = GlowAnimationState.NONE;
      this.$.input.blur();
    }
  }

  protected submitCleanup_() {
    // Update states after submitting:
    this.animationState = GlowAnimationState.SUBMITTING;

    // If the composebox is expandable or we should clear it, clear the input
    // after submitting the query.
    if (this.isCollapsible || this.clearAllInputsWhenSubmittingQuery_) {
      this.clearAllInputs(/* querySubmitted= */ true,
                          /* shouldBlockAutoSuggestedTabs= */ false);
    }

    if (this.isCollapsible) {
      this.$.input.blur();
    }

    this.fire('composebox-submit');
  }

  protected submitQuery_(e: KeyboardEvent|MouseEvent) {
    // If the submit button is disabled, do nothing.
    if (!this.canSubmitFilesAndInput_) {
      return;
    }

    // Sanity check the query.
    assert(this.hasValidQuery_(), 'Cannot submit query without a valid query.');

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

    this.submitCleanup_();
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

  getResultForTesting(): AutocompleteResult|null {
    return this.result_;
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
    /* Indicates when suggestion results have changed so that zero state
     * suggestion results in contextual tasks composebox can update accordingly.
     */
    this.fire('result-changed', result);

    const hasMatches = this.result_.matches.length > 0;
    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    // Zero suggest matches are not allowed to be default. Therefore, this
    // makes sure zero suggest results aren't focused when they are returned.
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      this.selectFirstMatch();
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
    this.smartComposeInlineHint_ = this.result_.smartComposeInlineHint?.trim() ?
        this.result_.smartComposeInlineHint :
        '';
  }

  private onContextualInputStatusChanged_(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType|null) {
    // If error message is updated, then the returned file is stale and removed
    // from carousel. File is removed from carousel on `kUploadReplaced` as
    // well despite no error message being returned (special case).
    // Else, `file` below is updated to its most recent state,
    // and `errorMessage` is null.
    const {file, errorMessage} =
        this.updateFileStatus_(token, status, errorType);
    if (errorMessage) {  // `file` value is definitely stale.
      this.errorMessage_ = errorMessage;
      this.pendingUploads_.delete(token);
      this.fileUploadsComplete = this.pendingUploads_.size === 0;
    } else if (file) {
      // Treat `kUploadReplaced` like an error upload state
      // (like `kUploadFailed`. `kValidationFailed`,
      // `kUploadExpired`), just without setting `errorMessage_`.
      // This means for `kUploadReplaced`, we do not fetch suggestions,
      // etc.
      if (file.status === FileUploadStatus.kUploadReplaced) {
        this.pendingUploads_.delete(file.uuid);
        this.fileUploadsComplete = this.pendingUploads_.size === 0;
        return;
      } else if (file.status === FileUploadStatus.kUploadSuccessful) {
        // At this point, due to the error message handling above (for
        // `kValidationFailed`, `kUploadExpired`, and `kUploadFailed`),
        // if kUploadSuccessful, the file upload is complete.
        // Else, the file upload is in progress.
        this.pendingUploads_.delete(file.uuid);
        this.fileUploadsComplete = this.pendingUploads_.size === 0;

        const announcer = getAnnouncerInstance();
        announcer.announce(this.i18n('composeboxFileUploadCompleteText'));
      } else if (
          file.status === FileUploadStatus.kProcessing ||
          file.status === FileUploadStatus.kProcessingSuggestSignalsReady) {
        // `NotUploaded`, `UploadStarted` come before and after `kProcessing`
        //  respectively, so we only need to add to `pendingUploads_` when in a
        //  type of processing state.
        this.addToPendingUploads_(file.uuid);
      }

      // Fetch contextual suggestions for processingSuggestSignalsReady
      // non-images:
      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          this.showZps && !file.type.includes('image')) {
        // Query autocomplete to get contextual suggestions for files.
        this.queryAutocomplete_(/* clearMatches= */ true);
      }
      // For image files:
      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          file.type.includes('image')) {
        // If we're in create image mode, update the aim tool mode.
        if (this.activeToolMode_ === ComposeboxToolMode.kImageGen) {
          this.handleToolModeUpdate_();
        } else if (this.enableImageContextualSuggestions_) {
          // Query autocomplete to get contextual suggestions for files.
          this.queryAutocomplete_(/* clearMatches= */ true);
        } else {
          this.showDropdown_ = false;
        }
      }

      // Query autocomplete to get contextual suggestions for tabs.
      if (status === FileUploadStatus.kProcessing &&
          file.type.includes('tab')) {
        this.queryAutocomplete_(/* clearMatches= */ true);
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
  private queryAutocomplete_(clearMatches: boolean) {
    if (clearMatches) {
      this.clearAutocompleteMatches();
    }
    this.lastQueriedInput_ = this.input_;
    this.haveReceivedAutcompleteResponse_ = false;
    this.searchboxHandler_.queryAutocomplete(this.input_, false);
  }

  clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean) {
    this.clearInput();
    // Let `querySubmit_` handle clearing files if the tool mode is a tool mode
    // that should be cleared after submitting. For all other general
    // clearing, clear input here.
    if (!querySubmitted) {
      this.resetModes();
    }
    const undeletableFiles =
        Array.from(this.files_.values()).filter(file => !file.isDeletable);
    if (undeletableFiles.length !== this.files_.size) {
      this.files_ = new Map(undeletableFiles.map(file => [file.uuid, file]));
      this.addedTabsIds_ = new Map(undeletableFiles.filter(file => file.tabId)
                                       .map(file => [file.tabId!, file.uuid]));
    }
    // Reset files in set to match remaining files in carousel.
    this.setPendingUploads([...this.files_.keys()]);
    this.smartComposeInlineHint_ = '';
    if (!querySubmitted) {
      // If the query was submitted, the searchbox handler will clear its own
      // uploaded file state when the query submission is handled.
      this.searchboxHandler_.clearFiles(shouldBlockAutoSuggestedTabs);
    }
    this.fileUploadsComplete = this.pendingUploads_.size === 0;
    if (this.inVoiceSearchMode_) {
      this.voiceSearchEndCleanup_();
    }
  }

  clearInput() {
    this.input_ = '';
    this.lastQueriedInput_ = '';
    this.$.matches.unselect();
  }

  getInputText(): string {
    return this.input_;
  }

  getNumOfFilesForTesting(): number {
    return this.files_.size;
  }

  private selectFirstMatch() {
    if (this.result_?.matches.length) {
      this.$.matches.selectFirst();
    }
  }

  protected shouldDisableFileInputs_() {
    return !this.contextMenuEnabled_ || !this.showMenuOnClick ||
        this.entrypointName === 'ContextualTasks';
  }

  protected shouldShowVoiceSearchAtBottom_(): boolean {
    return (this.searchboxLayoutMode === 'TallBottomContext' ||
            !this.searchboxLayoutMode) &&
        this.shouldShowVoiceSearch_();
  }

  protected hasImageFiles_() {
    return Array.from(this.files_.values()).some(
        file => file.type.includes('image'));
  }

  protected getToolChipLabel_(tool: ComposeboxToolMode): string {
    if (this.inputState_ && this.inputState_.toolConfigs) {
      const config = this.inputState_.toolConfigs.find(c => c.tool === tool);
      if (config && config.chipLabel) {
        return config.chipLabel;
      }
    }
    // Fallback to i18n strings
    switch (tool) {
      case ComposeboxToolMode.kDeepSearch:
        return this.i18n('deepSearch');
      case ComposeboxToolMode.kImageGen:
        return this.i18n('createImages');
      case ComposeboxToolMode.kCanvas:
        return this.i18n('canvas');
      default:
        return '';
    }
  }

  // This function is called when backend starts a file upload flow, whether
  // through `addFileFromAttachment_`, `addFileContextFromBrowser`, etc. This
  // contrasts with the workflows where the frontend starts a file upload flow
  // (`addFileContext_`).
  private onFileContextAdded_(file: ComposeboxFile) {
    const newFiles = new Map(this.files_);
    newFiles.set(file.uuid, file);
    this.files_ = newFiles;
    this.addToPendingUploads_(file.uuid);
  }

  private handleProcessFilesError_(error: ProcessFilesError) {
    if (error === ProcessFilesError.NONE) {
      return;
    }

    let metric = ComposeboxFileValidationError.NONE;

    switch (error) {
      case ProcessFilesError.MAX_FILES_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        this.errorMessage_ = this.i18n('maxFilesReachedError');
        break;
      case ProcessFilesError.MAX_IMAGES_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        this.errorMessage_ = this.i18n('maxImagesReachedError');
        break;
      case ProcessFilesError.MAX_PDFS_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        this.errorMessage_ = this.i18n('maxPdfsReachedError');
        break;
      case ProcessFilesError.FILE_EMPTY:
        metric = ComposeboxFileValidationError.FILE_EMPTY;
        this.errorMessage_ = this.i18n('composeboxFileUploadInvalidEmptySize');
        break;
      case ProcessFilesError.FILE_TOO_LARGE:
        metric = ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE;
        this.errorMessage_ = this.i18n('composeboxFileUploadInvalidTooLarge');
        break;
      case ProcessFilesError.INVALID_TYPE:
        this.errorMessage_ = this.i18n('composeFileTypesAllowedError');
        break;
      case ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED:
        this.errorMessage_ = this.i18n('composeboxFileUploadNotAllowed');
        break;
      default:
        break;
    }

    this.recordFileValidationMetric_(metric);
    this.closeMenu_();
  }

  private updateFileStatus_(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType|null) {
    let errorMessage = null;
    let file = this.files_.get(token);
    if (file) {
      if ([
            FileUploadStatus.kValidationFailed,
            FileUploadStatus.kUploadFailed,
            FileUploadStatus.kUploadExpired,
            FileUploadStatus.kUploadReplaced,
          ].includes(status)) {
        this.files_.delete(token);

        if (file.tabId) {
          this.addedTabsIds_ = new Map([...this.addedTabsIds_.entries()].filter(
              ([id, _]) => id !== file!.tabId));
        }
        switch (status) {
          case FileUploadStatus.kValidationFailed:
            if (errorType) {
              errorMessage = this.i18n(
                  FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                  'composeboxFileUploadValidationFailed');
            } else {
              errorMessage = this.i18n('composeboxFileUploadValidationFailed');
            }
            break;
          case FileUploadStatus.kUploadFailed:
            errorMessage = this.i18n('composeboxFileUploadFailed');
            break;
          case FileUploadStatus.kUploadExpired:
            errorMessage = this.i18n('composeboxFileUploadExpired');
            break;
          case FileUploadStatus.kUploadReplaced:
            // Update `composebox.ts` with the status since
            // this should not return an error message for this
            // 'non-uploaded' terminal file state, meaning
            // its file status is still needed for understanding state
            // when returned and back in the context of the function caller.
            file = {...file, status: status};
            break;
          default:
            break;
        }
        this.closeMenu_();
      } else {
        file = {...file, status: status};
        this.files_.set(token, file);
      }
      this.files_ = new Map([...this.files_]);
    } else {
      // File is unknown but its status is known. Show this if
      // ghost/unknown files in frontend are allowed to be in
      // carousel.
      if (this.shouldShowGhostFiles) {
        file = {
          uuid: token,
          name: '',
          objectUrl: null,
          dataUrl: null,
          type: '',
          // Override this since first upload status is this or processing.
          // Need this or processing in order to show tab spinner.
          status: FileUploadStatus.kUploadStarted,
          url: null,
          tabId: null,
          isDeletable: true,
        };
        // Update pending uploads in 'composebox.ts' to disable
        // submit button.
        this.onFileContextAdded_(file);
      }
    }
    return {file, errorMessage};
  }

  private processFiles_(files: FileList|null) {
    if (!files || files.length === 0) {
      return;
    }

    if (this.activeToolMode_ === ComposeboxToolMode.kDeepSearch) {
      this.handleProcessFilesError_(ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED);
      return;
    }

    const filesToUpload: File[] = [];
    let errorToDisplay = ProcessFilesError.NONE;

    const counts = new Map<InputType, number>();
    counts.set(InputType.kLensImage, 0);
    counts.set(InputType.kLensFile, 0);
    counts.set(InputType.kBrowserTab, 0);

    for (const file of this.files_.values()) {
      const type = this.getInputType_(file.type);
      counts.set(type, (counts.get(type) || 0) + 1);
    }

    let totalCount = this.files_.size;

    let maxTotal = this.maxFileCount_;
    if (this.inputState_ && this.inputState_.maxTotalInputs > 0) {
      maxTotal = this.inputState_.maxTotalInputs;
    }

    if (totalCount + files.length > maxTotal) {
      errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
    }

    for (const file of files) {
      const inputType = this.getInputType_(file.type);
      if (this.inputState_ &&
          this.activeToolMode_ !== ComposeboxToolMode.kUnspecified) {
        const disabledTypes = this.inputState_.disabledInputTypes || [];
        if (disabledTypes.includes(inputType)) {
          errorToDisplay =
              Math.max(errorToDisplay, ProcessFilesError.INVALID_TYPE);
          continue;
        }
      }

      if (file.size === 0 || file.size > this.maxFileSize_) {
        const sizeError = file.size === 0 ? ProcessFilesError.FILE_EMPTY :
                                            ProcessFilesError.FILE_TOO_LARGE;
        errorToDisplay = Math.max(errorToDisplay, sizeError);
        continue;
      }

      if (!this.isFileAllowed_(file.type)) {
        errorToDisplay =
            Math.max(errorToDisplay, ProcessFilesError.INVALID_TYPE);
        continue;
      }

      let maxType = maxTotal;
      if (this.inputState_ &&
          this.inputState_.maxInstances[inputType] !== undefined) {
        maxType = this.inputState_.maxInstances[inputType];
      }

      const currentTypeCount = counts.get(inputType) || 0;

      if (totalCount < maxTotal && currentTypeCount < maxType) {
        filesToUpload.push(file);
        totalCount++;
        counts.set(inputType, currentTypeCount + 1);
      } else {
        if (currentTypeCount >= maxType) {
          switch (inputType) {
            case InputType.kLensImage:
              errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_IMAGES_EXCEEDED);
              break;
            case InputType.kLensFile:
              errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_PDFS_EXCEEDED);
              break;
            default:
              errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
          }
        } else {
          errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
        }
      }
    }

    if (filesToUpload.length > 0) {
      this.addFileContext_(filesToUpload);
    }

    this.handleProcessFilesError_(errorToDisplay);
  }

  private isFileAllowed_(fileType: string): boolean {
    return this.isMimeTypeAllowed_(fileType, this.imageFileTypes_) ||
        this.isMimeTypeAllowed_(fileType, this.attachmentFileTypes_);
  }

  private isMimeTypeAllowed_(
      mimeType: string, allowedTypes: string[]): boolean {
    const lowerMimeType = mimeType.toLowerCase();
    return allowedTypes.some(type => {
      if (type.endsWith('/*')) {
        const prefix = type.slice(0, -1);
        return lowerMimeType.startsWith(prefix);
      }
      return lowerMimeType === type;
    });
  }

  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    if (!this.isFileAllowed_(fileAttachment.mimeType)) {
      this.handleProcessFilesError_(ProcessFilesError.INVALID_TYPE);
      return;
    }
    const pendingStatus = this.files_.get(fileAttachment.uuid)?.status;
    const composeboxFile: ComposeboxFile = {
      uuid: fileAttachment.uuid,
      name: fileAttachment.name,
      objectUrl: null,
      dataUrl: fileAttachment.imageDataUrl ?? null,
      type: fileAttachment.mimeType,
      status: pendingStatus ?? FileUploadStatus.kNotUploaded,
      url: null,
      tabId: null,
      isDeletable: true,
    };
    this.onFileContextAdded_(composeboxFile);
  }

  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContextHandleCallback_({
      tabId: tabAttachment.tabId,
      title: tabAttachment.title,
      url: tabAttachment.url,
      delayUpload: /*delay_upload=*/ false,
      origin: TabUploadOrigin.OTHER,
    } as TabUpload);
  }

  private getInputType_(type: string): InputType {
    if (type === 'tab') {
      return InputType.kBrowserTab;
    }
    if (type === 'image') {
      return InputType.kLensImage;
    }
    if (type === 'pdf') {
      return InputType.kLensFile;
    }

    if (this.imageFileTypes_.some(t => {
          if (t.endsWith('/*')) {
            const prefix = t.slice(0, -1);
            return type.startsWith(prefix);
          }
          return type === t;
        })) {
      return InputType.kLensImage;
    }

    return InputType.kLensFile;
  }

  private closeMenu_() {
    if (!this.showMenuOnClick) {
      return;
    }

    const entrypointAndMenu =
        this.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
            '#contextEntrypoint');
    if (entrypointAndMenu) {
      entrypointAndMenu.closeMenu();
    }
  }

  private recordFileValidationMetric_(
      enumValue: ComposeboxFileValidationError) {
    recordEnumerationValue(
        'ContextualSearch.File.WebUI.UploadAttemptFailure.' +
            this.composeboxSource_,
        enumValue, ComposeboxFileValidationError.MAX_VALUE + 1);
  }

  addFileContextForTesting(file: ComposeboxFile) {
    this.onFileContextAdded_(file);
  }

  onToolClickForTesting(toolMode: ComposeboxToolMode) {
    this.handleToolClick_(toolMode);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
