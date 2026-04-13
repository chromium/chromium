// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './composebox_file_inputs.js';
import './composebox_lens_search.js';
import './composebox_tool_chip.js';
import './contextual_entrypoint_and_menu.js';
import './contextual_entrypoint_button.js';
import './composebox_dropdown.js';
import './composebox_voice_search.js';
import './error_scrim.js';
import './file_carousel.js';
import './file_thumbnail.js';
import './icons.html.js';
import './composebox_input.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, SelectedFileInfo, TabAttachment, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ComposeboxFile, FILE_VALIDATION_ERRORS_MAP, recordBoolean, recordContextAdditionMethod, recordEnumerationValue, recordUserAction, TabUploadOrigin} from './common.js';
import type {ComposeboxState, TabUpload} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from './composebox_file_inputs.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ComposeboxEmbedderMixin} from './composebox_mixin.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {ContextUploadStatus, InputType, ToolMode} from './composebox_query.mojom-webui.js';
import type {ContextUploadErrorType, InputState} from './composebox_query.mojom-webui.js';
import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';
import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';
import type {ErrorScrimElement} from './error_scrim.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';
import {WindowProxy} from './window_proxy.js';

export enum VoiceSearchAction {
  ACTIVATE = 0,
  QUERY_SUBMITTED = 1,
}

export function isTerminalState(status: ContextUploadStatus): boolean {
  switch (status) {
    case ContextUploadStatus.kUploadSuccessful:
    case ContextUploadStatus.kUploadFailed:
    case ContextUploadStatus.kValidationFailed:
    case ContextUploadStatus.kUploadExpired:
    case ContextUploadStatus.kUploadReplaced:
      return true;
    case ContextUploadStatus.kNotUploaded:
    case ContextUploadStatus.kProcessing:
    case ContextUploadStatus.kUploadStarted:
    case ContextUploadStatus.kProcessingSuggestSignalsReady:
      return false;
    default:
      assertNotReachedCase(status, 'Unknown enum value');
  }
}

export enum SubmitButtonIconType {
  FORWARD = 'forward',
  UPWARD = 'upward',
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
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    carousel: ComposeboxFileCarouselElement,
    fileInputs: ComposeboxFileInputsElement,
    matches: ComposeboxDropdownElement,
    errorScrim: ErrorScrimElement,
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


export class ComposeboxElement extends ComposeboxEmbedderMixin
(I18nMixinLit(CrLitElement)) implements DragAndDropHost {
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
      state: {type: Object},
      showLensButton: {type: Boolean},
      suggestionActivityEnabled: {type: Boolean},
      lensButtonTriggersOverlay: {type: Boolean},
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
      inToolMode_: {
        type: Boolean,
        reflect: true,
      },
      isCanvasQuerySubmitted: {
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
      submitButtonIconType: {
        type: String,
      },
      tabSuggestions_: {type: Array},
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
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
      showMenuOnClick: {type: Boolean},
      entrypointName: {type: String, reflect: true},
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
      contextMenuEnabled_: {type: Boolean},
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
      enableFileHint: {type: Boolean},
      dropdownNeeded: {type: Boolean},
    };
  }

  accessor state: ComposeboxState|null = null;
  accessor enableFileHint: boolean = false;
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
  accessor lensButtonDisabled: boolean = false;
  // If linked to `showDropdown`, then matches will not be propagated upwards to
  // parent component. This is purely to hide cr composebox's dropdown, while
  // `showDropdown` seems to be affecting the presence of matches in `result_`.
  accessor dropdownNeeded: boolean = true;

  // Set this to true in parent component if it is desired
  // to show files that are not in the file map when
  // file status is updated from backend. Ghost files will be
  // shown as image chip with spinner in file carousel.
  accessor shouldShowGhostFiles: boolean = false;
  accessor submitButtonIconType: SubmitButtonIconType =
      SubmitButtonIconType.UPWARD;
  protected isRtl_: boolean = document.documentElement.dir === 'rtl';
  protected accessor tabSuggestions_: TabInfo[] = [];

  protected composeboxNoFlickerSuggestionsFix_: boolean =
      loadTimeData.getBoolean('composeboxNoFlickerSuggestionsFix');
  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  protected accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanding_: boolean = false;
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
  protected accessor showFileCarousel_: boolean = false;
  protected accessor transcript_: string = '';
  protected accessor receivedSpeech_: boolean = false;
  protected accessor canSubmitFilesAndInput_: boolean = true;
  protected lastQueriedInput_: string = '';
  protected showVoiceSearchInSteadyComposebox_: boolean =
      loadTimeData.getBoolean('steadyComposeboxShowVoiceSearch');
  protected showVoiceSearchInExpandedComposebox_: boolean =
      loadTimeData.getBoolean('expandedComposeboxShowVoiceSearch');
  // TODO(crbug.com/493988206): Rename to usePecApi_ and update all references.
  protected accessor showModelPicker_: boolean =
      loadTimeData.valueExists('contextualMenuUsePecApi') ?
      loadTimeData.getBoolean('contextualMenuUsePecApi') :
      false;
  protected accessor hasAllowedInputs_: boolean = false;
  protected accessor contextMenuEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor uploadButtonDisabled_: boolean = false;
  protected contextMenuDescriptionEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  protected accessor showContextMenuDescription_: boolean =
      this.contextMenuDescriptionEnabled_;
  protected accessor isOmniboxInCompactMode_: boolean = false;
  protected accessor inVoiceSearchMode_: boolean = false;
  protected accessor inToolMode_: boolean = false;
  accessor isCanvasQuerySubmitted: boolean = false;
  // Synchronous immediate guard used to deduplicate processing
  // autochips being added, not fully processed chips.
  protected pendingAutomaticActiveTabUrl_: string = '';

  // Retains the latest version of the pending automatic active tab's title.
  protected pendingAutomaticActiveTabTitle_: string = '';
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
  private lensSendRawFileMediaTypesEnabled_: boolean =
      loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled');

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
         this.shouldShowSubmitButton_ || this.inToolMode_);
  }

  protected get shouldShowSubmitButton_(): boolean {
    return this.searchboxNextEnabled && this.submitEnabled_;
  }

  protected submitButtonIconClass_(): string {
    switch (this.submitButtonIconType) {
      case SubmitButtonIconType.FORWARD:
        return 'icon-arrow-forward';
      case SubmitButtonIconType.UPWARD:
        return 'icon-arrow-upward';
      default:
        assertNotReachedCase(this.submitButtonIconType);
    }
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

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
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

    this.eventTracker_.add(this.getInputElement().inputElement, 'input', () => {
      this.submitEnabled_ = this.computeSubmitEnabled_();
    });

    this.focusInput();
    // For "next" searchboxes (Realbox Next, Omnibox Next, etc.), the zps
    // autocomplete query is triggered after the state has been initialized.
    if (this.queryZpsOnLoad && !this.searchboxNextEnabled) {
      this.queryAutocomplete_(/* clearMatches= */ false);
    }

    this.searchboxHandler_.notifySessionStarted();

    const inputState = await this.searchboxHandler_.getInputState();
      if (inputState) {
        this.inputState = inputState.state;
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
    if (changedPrivateProperties.has('input') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('files') ||
        changedPrivateProperties.has('errorMessage')) {
      this.showFileCarousel_ = this.files.size > 0;
      this.showDropdown_ = this.computeShowDropdown_();
    }
    if (changedPrivateProperties.has('submitEnabled_') ||
        changedPrivateProperties.has('fileUploadsComplete')) {
      this.uploadButtonDisabled_ = !this.fileUploadsComplete;
      this.canSubmitFilesAndInput_ =
          this.submitEnabled_ && this.fileUploadsComplete;
    }

    if (changedPrivateProperties.has('inputState') && this.inputState) {
      this.hasAllowedInputs_ =
          (this.inputState.allowedModels.length > 0 ||
           this.inputState.allowedTools.length > 0 ||
           this.inputState.allowedInputTypes.length > 0);
      this.inToolMode_ = this.inputState.activeTool !== ToolMode.kUnspecified;
      this.dispatchEvent(new CustomEvent('input-state-changed', {
        detail: {inputState: this.inputState},
      }));
    }

    if (changedPrivateProperties.has('inputPlaceholderOverride') ||
        changedPrivateProperties.has('files') ||
        changedPrivateProperties.has('enableFileHint') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('inputState.activeTool')) {
      this.updateInputPlaceholder_();
    }
  }
  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state') && this.state) {
      this.updateState_(this.state);
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedMatchIndex_')) {
      if (this.selectedMatch_) {
        // Update the input.
        const text = this.selectedMatch_.fillIntoEdit;
        this.input = text;
      } else if (!this.lastQueriedInput_) {
        // This is for cases when focus leaves the matches/input.
        // If there was already text in the input do not clear it.
        this.clearInput();
      } else {
        // For typed queries reset the input back to typed value when
        // focus leaves the match.
        this.input = this.lastQueriedInput_;
      }
    }

    if (changedPrivateProperties.has('selectedMatchIndex_') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('isFollowupQuery') ||
        changedPrivateProperties.has('files')) {
      this.submitEnabled_ = this.computeSubmitEnabled_();
      this.canSubmitFilesAndInput_ =
          this.submitEnabled_ && this.fileUploadsComplete;
    }
    if (changedPrivateProperties.has('smartComposeInlineHint_')) {
      if (this.smartComposeInlineHint_) {
        // TODO(crbug.com/452619068): Investigate why screenreader is
        // inconsistent.
        const announcer = getAnnouncerInstance();
        announcer.announce(
            this.smartComposeInlineHint_ + ', ' +
            this.i18n('composeboxSmartComposeTitle'));
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
    this.getInputElement().inputElement.focus();
  }

  getText() {
    return this.input;
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
    this.input = text;
  }

  resetModes() {
    const previousTool = this.inputState?.activeTool;
    this.uploadButtonDisabled_ = false;

    if (previousTool !== ToolMode.kUnspecified) {
      this.showContextMenuDescription_ = this.contextMenuDescriptionEnabled_;
      this.handleToolModeUpdate_(ToolMode.kUnspecified);
    }
  }

  setDefaultModel() {
    if (this.inputState?.activeModel &&
        (this.inputState.activeModel as ModelMode) !== ModelMode.kUnspecified) {
      this.searchboxHandler_.setActiveModelMode(this.inputState.activeModel);
    } else if (
        this.inputState?.allowedModels &&
        this.inputState.allowedModels.length > 0) {
      this.searchboxHandler_.setActiveModelMode(
          this.inputState.allowedModels[0]!);
    }
  }

  resetToolsAndModels() {
    if (this.inputState) {
      this.searchboxHandler_.setActiveToolMode(ToolMode.kUnspecified);
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

  hasFiles(): boolean {
    return this.files.size > 0;
  }

  isExpanded(): boolean {
    return this.expanding_;
  }

  getSelectedMatchIndexForTesting() {
    return this.selectedMatchIndex_;
  }

  protected async updateState_(state: ComposeboxState) {
    const text = state.text || '';
    const files = state.files || [];
    const mode = state.mode ?? ToolMode.kUnspecified;
    let model = state.model ?? ModelMode.kUnspecified;

    if (text) {
      this.input = text;
      this.lastQueriedInput_ = text;
    }
    if (this.showZps && files.length === 0) {
      this.queryAutocomplete_(/* clearMatches= */ false);
    }
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
    if (mode !== ToolMode.kUnspecified) {
      this.handleToolClick_(mode);
    }

    if (!!this.inputState && model === ModelMode.kUnspecified &&
        this.inputState.allowedModels.length > 0) {
      model = this.inputState.allowedModels[0]!;
    }
    this.searchboxHandler_.setActiveModelMode(model);
    this.updateInputPlaceholder_();

    await this.updateComplete;
  }

  protected addToPendingUploads_(token: UnguessableToken) {
    this.pendingUploads_.add(token);
    this.fileUploadsComplete = false;
  }

  protected computeCancelButtonTitle_() {
    return this.input.trim().length > 0 || this.files.size > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  protected onContextMenuContainerMousedown_(e: FocusEvent) {
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
    if (this.files.size > 1) {
      return false;
    }

    // Don't show dropdown if there's no results.
    if (!this.result_?.matches.length) {
      return false;
    }

    // Do not show dropdown if there's an error scrim.
    if (this.errorMessage !== '') {
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
      if (!this.showTypedSuggestWithContext_ && this.files.size > 0) {
        return false;
      }
      // Do not show the dropdown for multiline input or if only the verbatim
      // match is present (we always expect a verbatim
      // match for typed suggest, so we ensure the length of the matches is >1).
      if (this.getInputElement().inputElement.scrollHeight <= 48 &&
          this.result_?.matches.length > 1) {
        return true;
      }
    }

    // lastQueriedInput_ is used here since the input changes based on
    // the selected match. If typed suggest is not enabled and input is used,
    // the dropdown will hide if the user keys down over zps matches.
    return this.showZps && !this.lastQueriedInput_;
  }

  private hasValidQuery_() {
    // If there is at least one file that supports unimodal search, query is
    // valid.
    if (this.files.values().find(
            (file: ComposeboxFile) => file.supportsUnimodal)) {
      return true;
    }

    // If an autocomplete match is selected, it's a valid query.
    if (this.selectedMatchIndex_ >= 0 && !!this.result_) {
      return true;
    }

    if (this.input.trim().length > 0) {
      return true;
    }

    // TODO(crbug.com/485648942): Update to drive Deep Search behavior from the
    //   PEC API's ToolSubstateConfig.
    // Allow empty query for Deep Search follow-ups.
    if (this.inputState?.activeTool === ToolMode.kDeepSearch &&
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

  protected shouldShowVoiceSearch_(): boolean {
    const isExpanded = this.showDropdown_ || this.files.size > 0;
    const isFeatureEnabled = isExpanded ?
        this.showVoiceSearchInExpandedComposebox_ :
        this.showVoiceSearchInSteadyComposebox_;
    return isFeatureEnabled &&
        WindowProxy.getInstance().hasWebkitSpeechRecognition();
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
    if (!uuidToDelete || !this.files.has(uuidToDelete)) {
      return;
    }

    const file = this.files.get(uuidToDelete);
    if (file?.tabId) {
      this.addedTabsIds = new Map([...this.addedTabsIds.entries()].filter(
          ([id, _]) => id !== file.tabId));
    }

    const fromAutoSuggestedChip =
        uuidToDelete === this.automaticActiveTab_?.uuid &&
        (fromUserAction === true);
    if (fromAutoSuggestedChip) {
      // TODO(crbug.com/492797638): Consider folding this into the
      // `InputStateDeletion` metric.
      const metricName = 'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
          this.composeboxSource_;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.automaticActiveTab_ = null;
    }

    if (fromUserAction === true) {
      const isTab = !!file?.tabId;
      const type = isTab ? 'Tab' : 'File';
      const metricName = `ContextualSearch.UserAction.InputStateDeletion.${
          type}.${this.composeboxSource_}`;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
    }

    this.files = new Map(
        [...this.files.entries()].filter(([uuid, _]) => uuid !== uuidToDelete));
    this.pendingUploads_.delete(uuidToDelete);
    this.fileUploadsComplete = this.pendingUploads_.size === 0;
    this.searchboxHandler_.deleteContext(uuidToDelete, fromAutoSuggestedChip);
    this.focusInput();
    // We should not be querying autocomplete in the presence of a tab
    // with delayed upload until URL suggestions are implemented.
    // `deleteContext_` gets called before the active tab chip token is cleared,
    // therefore, check if we're removing this chip to see if the delayed tab
    // is getting removed.
    if (fromAutoSuggestedChip || !this.getHasAutomaticActiveTabChipToken()) {
      this.queryAutocomplete_(/* clearMatches= */ true);
    } else {
      // TODO(crbug.com/482150500): Have URL-suggestions for tabs with delayed
      // uploads.
      this.clearAutocompleteMatches();
    }
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
        const err = e as ContextUploadErrorType;
        if (FILE_VALIDATION_ERRORS_MAP.has(err)) {
          this.errorMessage = this.i18n(FILE_VALIDATION_ERRORS_MAP.get(err)!);
        }
        continue;
      }

      const attachment = ComposeboxFile.createFromFile(
          token, file, ContextUploadStatus.kNotUploaded, {
            dataUrl: null,
            objectUrl: file.type.includes('image') ? URL.createObjectURL(file) :
                                                     null,
            iconName: null,
            supportsUnimodal: true,
          });
      composeboxFiles.set(token, attachment);
      const announcer = getAnnouncerInstance();
      announcer.announce(this.i18n('composeboxFileUploadStartedText'));
    }
    this.files =
        new Map([...this.files.entries(), ...composeboxFiles.entries()]);
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
      type: fileInfo.mimeType || (fileInfo.imageDataUrl ? 'image' : ''),
      inputType: fileInfo.imageDataUrl ? InputType.kLensImage :
                                         InputType.kLensFile,
      status: fileInfo.imageDataUrl ? ContextUploadStatus.kUploadSuccessful :
                                      ContextUploadStatus.kNotUploaded,
      url: null,
      tabId: null,
      isDeletable: fileInfo.isDeletable,
      iconName: null,
      supportsUnimodal: true,
    };

    this.onFileContextAdded_(attachment);
  }

  injectInput(
      title: string, thumbnail: string, fileToken: UnguessableToken,
      supportsUnimodal: boolean, iconName?: string) {
    const attachment = ComposeboxFile.createFromInjectedInput(
        fileToken, thumbnail, title, iconName ?? null);
    attachment.supportsUnimodal = supportsUnimodal;

    this.onFileContextAdded_(attachment);
  }

  private updateAutoSuggestedTabContext_(tab: TabInfo|null) {
    const shouldDeleteAutomaticActiveTab = this.automaticActiveTab_ &&
        (!tab || this.automaticActiveTab_.url !== tab.url);
    if (shouldDeleteAutomaticActiveTab) {
      this.deleteFile(this.automaticActiveTab_!.uuid);
      this.automaticActiveTab_ = null;

      // TODO(crbug.com/482150500): Correctly query for url based suggestions
      // when delayed tab is present. Right now, while url-based suggestions are
      // not set-up, clear the autocomplete matches.
      if (!tab) {
        this.queryAutocomplete_(/* clearMatches= */ true);
      }
      return;
    }

    if (tab) {
      // Ignore the `TabInfo` update if there is a matching
      // `automaticActiveTab_`.
      if (this.automaticActiveTab_ &&
          tab.url === this.automaticActiveTab_.url &&
          tab.tabId === this.automaticActiveTab_.tabId) {
        return;
      }

      // If an autochip is currently being uploaded but carousel attachment has
      // not been created yet, allow updates to its title. Absence of this
      // url means that there is no currently no auto active tab uploading.
      // If the url is the same, this is an update for the same tab so just
      // allow updates to the uploading tab's title from this update,
      // but do not upload it again.
      if (this.pendingAutomaticActiveTabUrl_ === tab.url) {
        this.pendingAutomaticActiveTabTitle_ = tab.title;
        return;
      }
      // Otherwise, prepare to replace the auto chip:
      this.pendingAutomaticActiveTabUrl_ = tab.url;
      this.pendingAutomaticActiveTabTitle_ = tab.title;


      // Do not reset above pending states in this async callback since
      // later requests make any older async callback updates irrelevant.
      // Add the `TabInfo` as `ComposeboxFile` in carousel.
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
      this.clearAutocompleteMatches();
    }
  }

  private onInputStateChanged_(inputState: InputState) {
    this.inputState = inputState;

    const allowedTypes = this.inputState.allowedInputTypes;
    this.files.forEach((file, uuid) => {
      if (!allowedTypes.includes(file.inputType)) {
        this.deleteFile(uuid);
      }
    });
  }

  protected onDeleteFile_(
      e: CustomEvent<{uuid: UnguessableToken, fromUserAction?: boolean}>) {
    this.deleteFile(e.detail.uuid, e.detail.fromUserAction);
  }

  protected onDeleteTabContext_(
      e: CustomEvent<{uuid: UnguessableToken, fromUserAction?: boolean}>) {
    this.deleteFile(e.detail.uuid, e.detail.fromUserAction);
  }

  protected onAddTabContext_(e: CustomEvent<{
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
      if (!token) {
        return;
      }
      // Adding a tab is asynchronous. For auto active tabs, a title update
      // might be received after the upload process has been started. In order
      // to prevent adding duplicate chips from this update, simply update the
      // title of the initial upload instead based on whatever the latest
      // title update received is.
      const attachment = ComposeboxFile.createFromTab(
          token, tabUpload.tabId,
          replaceAutoActiveTabToken ? this.pendingAutomaticActiveTabTitle_ :
                                      tabUpload.title,
          tabUpload.url, {supportsUnimodal: true});

      this.files =
          new Map([...this.files.entries(), [attachment.uuid, attachment]]);
      this.addedTabsIds = new Map(
          [...this.addedTabsIds.entries(), [tabUpload.tabId, attachment.uuid]]);

      // Do not reset pending active tab to avoid overwriting
      // synchronous "pending statuses" that are queued (since this function
      // is asynchronous and can run much later).
      if (replaceAutoActiveTabToken) {
        this.automaticActiveTab_ =
            Object.assign(attachment, {uuid: attachment.uuid});
      }

      this.focusInput();

    } catch (e) {
      const err = e as ContextUploadErrorType;
      if (FILE_VALIDATION_ERRORS_MAP.has(err)) {
        this.errorMessage = this.i18n(FILE_VALIDATION_ERRORS_MAP.get(err)!);
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

  protected onSubmitContainerClick_(e: MouseEvent) {
    this.submitQuery_(e);
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

  protected async onGetTabPreview_(e: CustomEvent<{
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
    // TODO(crbug.com/466412331): Don't only fire this for composebox, this
    // should be recorded for all.
    this.fire('composebox-voice-search-transcription-success');
    if (this.autoSubmitVoiceSearch) {
      // TODO(crbug.com/466412331): Remove, only recorded for the NTP.
      this.fire(
          'voice-search-action', {value: VoiceSearchAction.QUERY_SUBMITTED});
      this.input = e.detail;
      const metricName = `ContextualSearch.UserAction.SubmitVoiceQuery.${
          this.composeboxSource_}`;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.searchboxHandler_.submitQuery(
          e.detail, /*mouse_button=*/ 0, /*alt_key=*/ false,
          /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false);
      this.submitCleanup_();
    } else {
      // If auto-submit is not enabled, update the input to the voice search
      // query, clear autocomplete matches, and recompute whether submission
      // should be enabled.
      this.input = e.detail;
      this.queryAutocomplete_(/* clearMatches= */ true);
      this.submitEnabled_ = this.computeSubmitEnabled_();
      await this.updateComplete;
      this.focusInput();
    }
  }

  protected onVoiceSearchButtonClick_() {
    this.inVoiceSearchMode_ = true;
    this.animationState = GlowAnimationState.LISTENING;
    this.fire('voice-search-action', {value: VoiceSearchAction.ACTIVATE});
    // For contextual tasks composebox voice metrics.
    this.fire('composebox-voice-search-start');
    this.shadowRoot
        .querySelector<ComposeboxVoiceSearchElement>(
            'cr-composebox-voice-search')!.start();
  }

  protected onVoiceSearchCancel_(e: CustomEvent<boolean>) {
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

      if (!this.disableCaretColorAnimation) {
        this.getInputElement().resetCaret();
      }
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
    return this.inputState?.activeTool !== ToolMode.kUnspecified ||
        this.input.trim().length > 0 || this.files.size > 0;
  }

  protected onLensClick_() {
    if (this.lensButtonTriggersOverlay) {
      this.pageHandler_.handleLensButtonClick();
    } else {
      this.pageHandler_.handleFileUpload(/*is_image=*/ true);
    }
  }

  protected onLensIconMousedown_(e: MouseEvent) {
    // Prevent the composebox from expanding due to being focused by capturing
    // the mousedown event. This is needed to allow the Lens icon to be
    // clicked when the composebox does not have focus without expanding the
    // composebox.
    e.preventDefault();
  }

  private updateInputPlaceholder_() {
    if (this.inputPlaceholderOverride) {
      this.inputPlaceholder = this.inputPlaceholderOverride;
      return;
    }

    // The file hint should only be shown when there is context that was
    // deliberately added by the user (i.e. not the automatic active tab).
    const isOnlyAutoTab = this.files.size === 1 && !!this.automaticActiveTab_;
    const shouldUseFileHint = this.enableFileHint && this.hasFiles() &&
        !isOnlyAutoTab && this.inputState?.activeTool === ToolMode.kUnspecified;
    if (shouldUseFileHint) {
      if (this.files.size > 1) {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThese');
        return;
      }
      const file = this.files.values().next().value!;
      if (file.type === 'tab') {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThisTab');
        return;
      } else if (file.type.includes('image')) {
        this.inputPlaceholder =
            this.i18n('composeboxHintTextAskAboutThisImage');
        return;
      } else if (file.type === 'pdf' || file.type === 'application/pdf') {
        this.inputPlaceholder = this.i18n('composeboxHintTextAskAboutThisDoc');
        return;
      }
    }

    if (this.inputState) {
      if (this.inputState.activeTool !== ToolMode.kUnspecified) {
        const config = this.inputState.toolConfigs.find(
            c => c.tool === this.inputState!.activeTool);
        if (config?.hintText) {
          this.inputPlaceholder = config.hintText;
          return;
        }
      }

      if (this.inputState.activeModel !== ModelMode.kUnspecified) {
        const config = this.inputState.modelConfigs.find(
            c => c.model === this.inputState!.activeModel);
        if (config?.hintText) {
          this.inputPlaceholder = config.hintText;
          return;
        }
      }

      if (this.inputState.hintText) {
        this.inputPlaceholder = this.inputState.hintText;
        return;
      }
    }

    if (this.inputState?.activeTool === ToolMode.kDeepSearch) {
      this.inputPlaceholder =
          loadTimeData.getString('composeDeepSearchPlaceholder');
    } else if (this.inputState?.activeTool === ToolMode.kImageGen) {
      this.inputPlaceholder =
          loadTimeData.getString('composeCreateImagePlaceholder');
    } else {
      this.inputPlaceholder =
          loadTimeData.getString('searchboxComposePlaceholder');
    }
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.handleToolClick_(e.detail.toolMode);
  }

  protected handleToolClick_(tool: ToolMode) {
    const isTogglingOff = this.inputState?.activeTool === tool;

    if (this.contextMenuDescriptionEnabled_) {
      this.showContextMenuDescription_ =
          isTogglingOff || tool === ToolMode.kUnspecified;
    }

    const newToolMode = isTogglingOff ? ToolMode.kUnspecified : tool;

    if (isTogglingOff) {
      const metricName = `ContextualSearch.UserAction.InputStateDeletion.Tool.${
          this.composeboxSource_}`;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
    } else {
      this.searchboxHandler_.recordToolSelectionAction(newToolMode);
    }
    this.handleToolModeUpdate_(newToolMode);
  }

  private handleToolModeUpdate_(newTool: ToolMode) {
    this.searchboxHandler_.setActiveToolMode(newTool);
    this.queryAutocomplete_(/* clearMatches= */ true);
    this.updateInputPlaceholder_();
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.searchboxHandler_.recordModelSelectionAction(e.detail.model);
    this.searchboxHandler_.setActiveModelMode(e.detail.model);
    this.updateInputPlaceholder_();
  }

  protected onDismissErrorScrim_() {
    this.errorMessage = '';
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected onInputInput_(_e: CustomEvent<Event>) {
    this.input = this.getInputElement().input;

    // `clearMatches` is true if input is empty stop any in progress providers
    // before requerying for on-focus (zero-suggest) inputs. The searchbox
    // doesn't allow zero-suggest requests to be made while the ACController
    // is not done.
    if (this.composeboxNoFlickerSuggestionsFix_) {
      // If the composebox no flickering fix is enabled, stop the ACController
      // from querying for suggestions when the input is empty, but don't clear
      // the matches so the dropdown doesn't close.
      if (this.input === '') {
        this.searchboxHandler_.stopAutocomplete(/*clearResult=*/ true);
      }
      this.queryAutocomplete_(/* clearMatches= */ false);
    } else {
      this.queryAutocomplete_(/* clearMatches= */ this.input === '');
    }
  }

  private isFocusInInput_(): boolean {
    return this.shadowRoot.activeElement === this.getInputElement();
  }

  private hasMatches_(): boolean {
    return !!(this.result_ && this.result_.matches.length > 0);
  }

  private finalizeMatchSelection_(e: KeyboardEvent) {
    this.smartComposeInlineHint_ = '';
    e.preventDefault();
    if (this.shadowRoot.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }
  }

  private handleArrowKey_(e: KeyboardEvent) {
    if (!this.dropdownNeeded) {
      return;
    }
    if (this.isFocusInInput_() && !this.showDropdown_) {
      return;
    }
    if (!this.hasMatches_() || hasKeyModifiers(e)) {
      return;
    }

    if (e.key === 'ArrowDown') {
      this.$.matches.selectNext();
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
    }
    this.finalizeMatchSelection_(e);
  }

  private handleTab_(e: KeyboardEvent) {
    if (this.isFocusInInput_()) {
      // If focus leaves the input, unselect the first match.
      if (e.shiftKey) {
        this.$.matches.unselect();
      } else if (this.smartComposeEnabled_ && this.smartComposeInlineHint_) {
        this.input = this.input + this.smartComposeInlineHint_;
        this.smartComposeInlineHint_ = '';
        e.preventDefault();
        this.queryAutocomplete_(/* clearMatches= */ true);
      }
      return;
    }

    if (this.hasMatches_() && this.dropdownNeeded && !hasKeyModifiers(e)) {
      // If focus goes past the last match, unselect the last match.
      if (this.selectedMatchIndex_ === this.result_!.matches.length - 1) {
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
    }
  }

  private handleEnter_(e: KeyboardEvent) {
    if (this.shadowRoot.activeElement === this.$.matches || !e.shiftKey) {
      e.preventDefault();
      if (this.canSubmitFilesAndInput_) {
        this.submitQuery_(e);
      }
    }
  }

  private handleEscape_(e: KeyboardEvent) {
    this.handleEscapeKeyLogic();
    e.stopPropagation();
    e.preventDefault();
  }

  private handlePageNavigation_(e: KeyboardEvent) {
    if (!this.hasMatches_() || !this.dropdownNeeded || hasKeyModifiers(e)) {
      return;
    }

    if (e.key === 'PageUp') {
      this.selectFirstMatch();
    } else {
      this.$.matches.selectLast();
    }
    this.finalizeMatchSelection_(e);
  }

  protected onKeydown_(e: KeyboardEvent) {
    const HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
      'Tab',
    ];
    if (!HANDLED_KEYS.includes(e.key)) {
      return;
    }

    const handlers: Record<string, (e: KeyboardEvent) => void> = {
      'ArrowDown': (e) => this.handleArrowKey_(e),
      'ArrowUp': (e) => this.handleArrowKey_(e),
      'Enter': (e) => this.handleEnter_(e),
      'Escape': (e) => this.handleEscape_(e),
      'Tab': (e) => this.handleTab_(e),
      'PageUp': (e) => this.handlePageNavigation_(e),
      'PageDown': (e) => this.handlePageNavigation_(e),
    };

    handlers[e.key]!(e);
  }

  protected onInputFocusin_() {
    // if there's a last queried input, it's guaranteed that at least
    // the verbatim match will exist.
    if (this.lastQueriedInput_) {
      this.selectFirstMatch();
    }
  }

  protected onComposeboxFocusin_(e: FocusEvent) {
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

  protected onComposeboxFocusout_(e: FocusEvent) {
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

  protected onSubmitContainerFocusin_() {
    // Matches should always be greater than 0 due to verbatim match.
    if (this.input && !this.selectedMatch_) {
      this.selectFirstMatch();
    }
  }

  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input = context.input;
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
          this.handleToolModeUpdate_(ToolMode.kDeepSearch);
          break;
        case ToolMode.kImageGen:
          this.handleToolModeUpdate_(ToolMode.kImageGen);
          break;
        case ToolMode.kCanvas:
          this.handleToolModeUpdate_(ToolMode.kCanvas);
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
    this.fire('close-composebox', {composeboxText: this.input});

    if (this.isCollapsible) {
      this.expanding_ = false;
      this.animationState = GlowAnimationState.NONE;
      this.getInputElement().inputElement.blur();
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

    this.fire('composebox-submit');
  }

  protected submitQuery_(e: KeyboardEvent|MouseEvent) {
    // If we're unable to submit (e.g., still uploading files) or the query
    // synchronously evaluates to invalid (e.g. state hasn't updated in Lit
    // due to synchronous eventing), do nothing.
    if (!this.canSubmitFilesAndInput_ || !this.hasValidQuery_()) {
      return;
    }

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
          this.input.trim(), (e as MouseEvent).button || 0, e.altKey, e.ctrlKey,
          e.metaKey, e.shiftKey);
    }

    this.submitCleanup_();
    // We only close the composebox when opening in a new tab because doing
    // so in the current tab causes a visual jitter where the composebox
    // closes before the new results page finishes loading.
    if (e.ctrlKey || e.metaKey || e.shiftKey) {
      this.closeComposebox_();
    }
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail.index);
  }

  protected onMatchClick_(e: CustomEvent<{
    ctrlKey: boolean,
    metaKey: boolean,
    shiftKey: boolean,
  }>) {
    this.clearAutocompleteMatches();
    // We only close the composebox when opening in a new tab because doing
    // so in the current tab causes a visual jitter where the composebox
    // closes before the new results page finishes loading.
    if (e && e.detail &&
        (e.detail.ctrlKey || e.detail.metaKey || e.detail.shiftKey)) {
      this.closeComposebox_();
    }
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex_ = e.detail.value;
    this.selectedMatch_ =
        this.result_?.matches[this.selectedMatchIndex_] || null;
  }

  getFilesForTesting(): ComposeboxFile[] {
    return [...this.files.values()];
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
        this.input.trim() && hasMatches && this.selectedMatchIndex_ >= 0 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input. Don't restore when the
      // user deletes all their input and autocomplete is queried or else the
      // empty input will change to the value of the first result.
      this.$.matches.selectIndex(this.selectedMatchIndex_);

      // Set the selected match since the `selectedMatchIndex_` does not change
      // (and therefore `selectedMatch_` does not get updated since
      // `onSelectedMatchIndexChanged_` is not called).
      this.selectedMatch_ = this.result_.matches[this.selectedMatchIndex_]!;
      this.input = this.selectedMatch_.fillIntoEdit;
    } else {
      this.$.matches.unselect();
    }

    // Populate the smart compose suggestion.
    this.smartComposeInlineHint_ = this.result_.smartComposeInlineHint?.trim() ?
        this.result_.smartComposeInlineHint :
        '';
  }

  private onContextualInputStatusChanged_(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|null) {
    // If error message is updated, then the returned file is stale and removed
    // from carousel. File is removed from carousel on `kUploadReplaced` as
    // well despite no error message being returned (special case).
    // Else, `file` below is updated to its most recent state,
    // and `errorMessage` is null.
    const {file, errorMessage} =
        this.updateFileStatus_(token, status, errorType);
    if (errorMessage) {  // `file` value is definitely stale.
      this.errorMessage = errorMessage;
      this.pendingUploads_.delete(token);
      this.fileUploadsComplete = this.pendingUploads_.size === 0;
    } else if (file) {
      // Treat `kUploadReplaced` like an error upload state
      // (like `kUploadFailed`. `kValidationFailed`,
      // `kUploadExpired`), just without setting `errorMessage`.
      // This means for `kUploadReplaced`, we do not fetch suggestions,
      // etc.
      if (file.status === ContextUploadStatus.kUploadReplaced) {
        this.pendingUploads_.delete(file.uuid);
        this.fileUploadsComplete = this.pendingUploads_.size === 0;
        return;
      } else if (file.status === ContextUploadStatus.kUploadSuccessful) {
        // At this point, due to the error message handling above (for
        // `kValidationFailed`, `kUploadExpired`, and `kUploadFailed`),
        // if kUploadSuccessful, the file upload is complete.
        // Else, the file upload is in progress.
        this.pendingUploads_.delete(file.uuid);
        this.fileUploadsComplete = this.pendingUploads_.size === 0;

        const announcer = getAnnouncerInstance();
        announcer.announce(this.i18n('composeboxFileUploadCompleteText'));
      } else if (
          file.status === ContextUploadStatus.kProcessing ||
          file.status === ContextUploadStatus.kProcessingSuggestSignalsReady) {
        // `NotUploaded`, `UploadStarted` come before and after `kProcessing`
        //  respectively, so we only need to add to `pendingUploads_` when in a
        //  type of processing state.
        this.addToPendingUploads_(file.uuid);
      }

      // Fetch contextual suggestions for processingSuggestSignalsReady
      // non-images:
      if (status === ContextUploadStatus.kProcessingSuggestSignalsReady &&
          this.showZps && !file.type.includes('image')) {
        // Query autocomplete to get contextual suggestions for files.
        this.queryAutocomplete_(/* clearMatches= */ true);
      }
      // For image files:
      if (status === ContextUploadStatus.kProcessingSuggestSignalsReady &&
          file.type.includes('image')) {
        if (this.enableImageContextualSuggestions_) {
          // Query autocomplete to get contextual suggestions for files.
          this.queryAutocomplete_(/* clearMatches= */ true);
        } else {
          this.showDropdown_ = false;
        }
      }

      // Query autocomplete to get contextual suggestions for tabs.
      if (status === ContextUploadStatus.kProcessing &&
          file.type.includes('tab')) {
        this.queryAutocomplete_(/* clearMatches= */ true);
      }
    }
  }

  // `queryAutocomplete` updates the `lastQueriedInput_` and makes an
  // autocomplete call through the handler. It also optionally clears existing
  // matches.
  private queryAutocomplete_(clearMatches: boolean) {
    if (clearMatches) {
      this.clearAutocompleteMatches();
    }
    this.lastQueriedInput_ = this.input;
    this.haveReceivedAutcompleteResponse_ = false;
    this.searchboxHandler_.queryAutocomplete(this.input, false);
  }

  clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean) {
    this.clearInput();
    this.automaticActiveTab_ = null;
    this.pendingAutomaticActiveTabUrl_ = '';
    this.pendingAutomaticActiveTabTitle_ = '';
    // Let `querySubmit_` handle clearing files if the tool mode is a tool mode
    // that should be cleared after submitting. For all other general
    // clearing, clear input here.
    if (!querySubmitted) {
      this.resetModes();
    }
    const undeletableFiles =
        Array.from(this.files.values()).filter(file => !file.isDeletable);
    if (undeletableFiles.length !== this.files.size) {
      this.files = new Map(undeletableFiles.map(file => [file.uuid, file]));
      this.addedTabsIds = new Map(undeletableFiles.filter(file => file.tabId)
                                      .map(file => [file.tabId!, file.uuid]));
    }
    // Reset files in set to match remaining files in carousel.
    this.setPendingUploads([...this.files.keys()]);
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
    this.input = '';
    this.lastQueriedInput_ = '';
    this.$.matches.unselect();
  }

  getInputText(): string {
    return this.input;
  }

  getNumOfFilesForTesting(): number {
    return this.files.size;
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
    return Array.from(this.files.values())
        .some(file => file.type.includes('image'));
  }

  // This function is called when backend starts a file upload flow, whether
  // through `addFileFromAttachment_`, `addFileContextFromBrowser`, etc. This
  // contrasts with the workflows where the frontend starts a file upload flow
  // (`addFileContext_`).
  private onFileContextAdded_(file: ComposeboxFile) {
    const newFiles = new Map(this.files);
    newFiles.set(file.uuid, file);
    this.files = newFiles;
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
        this.errorMessage = this.i18n('maxFilesReachedError');
        break;
      case ProcessFilesError.MAX_IMAGES_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        this.errorMessage = this.i18n('maxImagesReachedError');
        break;
      case ProcessFilesError.MAX_PDFS_EXCEEDED:
        metric = ComposeboxFileValidationError.TOO_MANY_FILES;
        this.errorMessage = this.i18n('maxPdfsReachedError');
        break;
      case ProcessFilesError.FILE_EMPTY:
        metric = ComposeboxFileValidationError.FILE_EMPTY;
        this.errorMessage = this.i18n('composeboxFileUploadInvalidEmptySize');
        break;
      case ProcessFilesError.FILE_TOO_LARGE:
        metric = ComposeboxFileValidationError.FILE_SIZE_TOO_LARGE;
        this.errorMessage = this.i18n('composeboxFileUploadInvalidTooLarge');
        break;
      case ProcessFilesError.INVALID_TYPE:
        this.errorMessage = this.i18n('composeFileTypesAllowedError');
        break;
      case ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED:
        this.errorMessage = this.i18n('composeboxFileUploadNotAllowed');
        break;
      default:
        break;
    }

    this.recordFileValidationMetric_(metric);
    this.closeMenu_();
  }

  private updateFileStatus_(
      token: UnguessableToken, status: ContextUploadStatus,
      errorType: ContextUploadErrorType|null) {
    let errorMessage = null;
    let file = this.files.get(token);
    if (file) {
      if (isTerminalState(status) &&
          status !== ContextUploadStatus.kUploadSuccessful) {
        this.files.delete(token);

        if (file.tabId) {
          this.addedTabsIds = new Map([...this.addedTabsIds.entries()].filter(
              ([id, _]) => id !== file!.tabId));
        }
        switch (status) {
          case ContextUploadStatus.kValidationFailed:
            if (errorType) {
              errorMessage = this.i18n(
                  FILE_VALIDATION_ERRORS_MAP.get(errorType) ??
                  'composeboxFileUploadValidationFailed');
            } else {
              errorMessage = this.i18n('composeboxFileUploadValidationFailed');
            }
            break;
          case ContextUploadStatus.kUploadFailed:
            errorMessage = this.i18n('composeboxFileUploadFailed');
            break;
          case ContextUploadStatus.kUploadExpired:
            errorMessage = this.i18n('composeboxFileUploadExpired');
            break;
          case ContextUploadStatus.kUploadReplaced:
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
        this.files.set(token, file);
      }
      this.files = new Map([...this.files]);
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
          inputType: InputType.kLensFile,
          // Override this since first upload status is this or processing.
          // Need this or processing in order to show tab spinner.
          status: ContextUploadStatus.kUploadStarted,
          url: null,
          tabId: null,
          isDeletable: true,
          iconName: null,
          supportsUnimodal: true,
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
    if (this.inputState?.activeTool === ToolMode.kDeepSearch) {
      this.handleProcessFilesError_(ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED);
      return;
    }

    const filesToUpload: File[] = [];
    let errorToDisplay = ProcessFilesError.NONE;

    const counts = new Map<InputType, number>();
    counts.set(InputType.kLensImage, 0);
    counts.set(InputType.kLensFile, 0);
    counts.set(InputType.kBrowserTab, 0);

    for (const file of this.files.values()) {
      const type = this.getInputType_(file.type);
      counts.set(type, (counts.get(type) || 0) + 1);
    }

    let totalCount = this.files.size;

    let maxTotal = this.maxFileCount_;
    if (this.inputState && this.inputState.maxTotalInputs > 0) {
      maxTotal = this.inputState.maxTotalInputs;
    }

    if (totalCount + files.length > maxTotal) {
      errorToDisplay = Math.max(errorToDisplay, ProcessFilesError.MAX_FILES_EXCEEDED);
    }

    for (const file of files) {
      const inputType = this.getInputType_(file.type);
      if (this.inputState?.activeTool !== ToolMode.kUnspecified) {
        const disabledTypes = this.inputState?.disabledInputTypes || [];
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
      if (this.inputState &&
          this.inputState.maxInputsByType[inputType] !== undefined) {
        maxType = this.inputState.maxInputsByType[inputType];
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
    if (this.lensSendRawFileMediaTypesEnabled_) {
      return true;
    }
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
    const pendingStatus = this.files.get(fileAttachment.uuid)?.status;
    const composeboxFile = ComposeboxFile.createFromFile(
        fileAttachment.uuid as unknown as UnguessableToken,
        {name: fileAttachment.name, type: fileAttachment.mimeType},
        pendingStatus ?? ContextUploadStatus.kNotUploaded,
        {dataUrl: fileAttachment.imageDataUrl ?? null, supportsUnimodal: true});
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

    // Arbitrary file types are treated as Lens files.
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

  setAutomaticActiveTabForTesting(file: ComposeboxFile) {
    this.automaticActiveTab_ = file;
  }

  updateAutoSuggestedTabContextForTesting(tab: TabInfo|null) {
    this.updateAutoSuggestedTabContext_(tab);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
