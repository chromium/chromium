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
import './composebox_submit.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {SearchAnimatedGlowElement} from '//resources/cr_components/search/animated_glow.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ComposeboxFile, GlifAnimationState, mapUploadErrorToProcessFilesError, ProcessFilesError, recordBoolean, recordContextAdditionMethod, recordUserAction, TabUploadOrigin} from './common.js';
import type {TabUpload} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from './composebox_file_inputs.js';
import type {ComposeboxInputElement} from './composebox_input.js';
import {ComposeboxEmbedderMixin, SubmitButtonIconType, VoiceSearchAction} from './composebox_mixin.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import type {ContextUploadErrorType} from './composebox_query.mojom-webui.js';
import {ContextUploadStatus, ToolMode} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';
import type {ErrorScrimElement} from './error_scrim.js';
import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export {SubmitButtonIconType, VoiceSearchAction};

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
    animatedSearchElement: SearchAnimatedGlowElement,
  };
}

// LINT.IfChange
export class ComposeboxElement extends ComposeboxEmbedderMixin
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
      isCollapsible: {
        reflect: true,
        type: Boolean,
      },
      expanding_: {
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
      lensButtonDisabled: {
        reflect: true,
        type: Boolean,
      },
      carouselOnTop_: {
        type: Boolean,
      },
      entrypointName: {type: String, reflect: true},
      disableComposeboxAnimation: {type: Boolean},
      // Embedders can opt out of public composebox resize events when they do
      // not use them.
      observeResize: {type: Boolean},
      enableCarouselScrolling: {type: Boolean},
      isOmniboxInCompactMode_: {
        type: Boolean,
        reflect: true,
      },
      isFollowupQuery: {type: Boolean},
      energyEffectEnabled: {
        type: Boolean,
        reflect: true,
      },
      energyEffectAnimationEnabled: {
        type: Boolean,
        reflect: true,
      },
      isZeroState: {
        type: Boolean,
        reflect: true,
      },
      disableFallbackGlifAnimation: {type: Boolean},
      enableFileHint: {type: Boolean},
      // TODO(crbug.com/486707842): Move to the Contextual Tasks embedder.
      glifAnimationState: {type: String, reflect: true},
      isSidePanel: {type: Boolean},
      inputPlaceholderOverride: {type: String},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      // Must be property so can pass it down to children.
      searchboxCallbackRouter_: {type: Object},
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor energyEffectEnabled: boolean = false;
  accessor isSidePanel: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor isZeroState: boolean = false;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor disableFallbackGlifAnimation: boolean = false;
  accessor isFollowupQuery: boolean = false;
  accessor enableFileHint: boolean = false;
  accessor inputPlaceholderOverride: string = '';
  accessor suggestionActivityEnabled: boolean = true;
  accessor disableComposeboxAnimation: boolean = false;
  accessor observeResize: boolean = true;
  accessor enableCarouselScrolling: boolean = false;
  accessor lensButtonTriggersOverlay: boolean = false;
  accessor showLensButton: boolean = true;
  accessor ntpRealboxNextEnabled: boolean = false;
  accessor carouselOnTop_: boolean = false;
  accessor entrypointName: string = '';
  accessor lensButtonDisabled: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  protected isRtl_: boolean = document.documentElement.dir === 'rtl';

  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanding_: boolean = false;
  protected accessor isOmniboxInCompactMode_: boolean = false;
  protected accessor searchboxCallbackRouter_: SearchboxPageCallbackRouter;

  // Synchronous immediate guard used to deduplicate processing
  // autochips being added, not fully processed chips.
  protected pendingAutomaticActiveTabUrl_: string = '';

  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');

  // Retains the latest version of the pending automatic active tab's title.
  protected pendingAutomaticActiveTabTitle_: string = '';
  protected dragAndDropHandler_: DragAndDropHandler;
  private automaticActiveTab_: ComposeboxFile|null = null;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private resizeObservers_: ResizeObserver[] = [];

  override shouldShowDivider(): boolean {
    // TODO(crbug.com/476175193): Remove `entrypointName` condition.
    if (this.entrypointName === 'Omnibox' &&
        this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    // TODO(crbug.com/486707841): Move to the NTP embedder.
    // Retain the divider for NTP when only tab favicons are present.
    // Omnibox is excluded as it does not need this divider.
    const hasNonTabFiles = Array.from(this.files.values()).some(f => !f.url);
    if (this.entrypointName !== 'Omnibox' && this.hasTabs() &&
        !hasNonTabFiles) {
      return this.showDropdown;
    }
    return super.shouldShowDivider();
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getContextEntrypointElement(): ContextualEntrypointButtonElement|
      ContextualEntrypointAndMenuElement|null {
    return this.shadowRoot?.querySelector<ContextualEntrypointButtonElement|
                                          ContextualEntrypointAndMenuElement>(
               '#contextEntrypoint') ||
        null;
  }

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
    this.dragAndDropHandler_ =
        new DragAndDropHandler(this, this.dragAndDropEnabled);
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

    this.searchboxListenerIds.push(
        this.getSearchboxCallbackRouter()
            .updateAutoSuggestedTabContext.addListener(
                this.updateAutoSuggestedTabContext_.bind(this)));

    this.focusInput();

    // TODO(crbug.com/497887993): Move to contextual tasks composebox when the
    // lens composebox is removed.
    if (this.smartTabSharingVisible) {
      const {active} = await this.pageHandler_.getSmartTabSharingActive();
      this.smartTabSharingActive = active;
      if (active) {
        this.clearContextForSmartTabSharingActive_();
      }
    }
    this.syncResizeObservers_();
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

  private tearDownResizeObservers_() {
    for (const observer of this.resizeObservers_) {
      observer.disconnect();
    }
    this.resizeObservers_ = [];
  }

  private syncResizeObservers_() {
    this.tearDownResizeObservers_();
    if (!this.isConnected || !this.observeResize) {
      return;
    }
    this.setupResizeObservers_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tearDownResizeObservers_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('entrypointName') ||
        changedProperties.has('searchboxLayoutMode')) {
      this.isOmniboxInCompactMode_ = this.entrypointName === 'Omnibox' &&
          this.searchboxLayoutMode === 'Compact';
    }

    if (changedProperties.has('inputState') ||
        changedProperties.has('entrypointName') ||
        changedProperties.has('webuiOmniboxSimplificationEnabled')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      const hasBackground = this.entrypointName === 'Omnibox' ?
          this.webuiOmniboxSimplificationEnabled :
          false;
      this.applyContextButtonBackground = hasBackground && !inToolMode;
    }

    if (changedProperties.has('inputPlaceholderOverride') ||
        changedProperties.has('enableFileHint')) {
      this.updateInputPlaceholder();
    }

    // If it is the first render, or 'entrypointName' has changed from its default.
    if (!this.hasUpdated || changedProperties.has('entrypointName')) {
      // TODO(crbug.com/511319142): Add this first branch's logic to
      // contextual tasks specific composebox.
      if (this.entrypointName === 'ContextualTasks') {
        this.voiceSearchCoherenceEnabled = loadTimeData.getBoolean(
            'voiceSearchCoherenceCobrowsingComposeboxEnabled');
      } else {
        this.voiceSearchCoherenceEnabled =
            loadTimeData.getBoolean('voiceSearchCoherenceComposeboxesEnabled');
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('observeResize')) {
      this.syncResizeObservers_();
    }
  }

  /* Used by drag/drop host interface so the
  drag and drop handler can access addDroppedFiles(). */
  getDropTarget() {
    return this;
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
    if (this.expanding_ && !this.submitEnabled) {
      requestAnimationFrame(() => {
        this.animationState = GlowAnimationState.EXPANDING;
      });
    }
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

  isExpanded(): boolean {
    return this.expanding_;
  }

  setExpandingForTesting(expanding: boolean) {
    this.expanding_ = expanding;
  }

  // Default logic moved to mixin. This override is for contextual_tasks.
  override hasValidQuery(): boolean {
    // If there is at least one file that supports unimodal search, query is
    // valid.
    if (this.files.values().find(
            (file: ComposeboxFile) => file.supportsUnimodal)) {
      return true;
    }

    // If an autocomplete match is selected, it's a valid query.
    if (this.selectedMatchIndex >= 0 && !!this.result) {
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

  // TODO(crbug.com/486706573): common logic moved to mixin. Move embedder
  // specific logic to the embedder class (contextual_tasks).
  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const fromAutoSuggestedChip =
        uuidToDelete === this.automaticActiveTab_?.uuid &&
        (fromUserAction === true);
    const file =
        super.deleteFile(uuidToDelete, fromUserAction, fromAutoSuggestedChip);

    if (!file) {
      return null;
    }

    if (fromAutoSuggestedChip) {
      // TODO(crbug.com/492797638): Consider folding this into the
      // `InputStateDeletion` metric.
      const metricName = 'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
          this.composeboxSource;
      recordUserAction(metricName);
      recordBoolean(metricName, true);
      this.automaticActiveTab_ = null;
    }
    // We should not be querying autocomplete in the presence of a tab
    // with delayed upload until URL suggestions are implemented.
    // `deleteContext_` gets called before the active tab chip token is cleared,
    // therefore, check if we're removing this chip to see if the delayed tab
    // is getting removed.
    if (fromAutoSuggestedChip || !this.getHasAutomaticActiveTabChipToken()) {
      this.queryAutocomplete(/* clearMatches= */ true);
    } else {
      // TODO(crbug.com/482150500): Have URL-suggestions for tabs with delayed
      // uploads.
      this.clearAutocompleteMatches();
    }
    return file;
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles(e.detail.files);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.composeboxSource);
  }

  // TODO(crbug.com/486707842): Move this to contextual tasks composebox.
  injectInput(
      title: string, thumbnail: string, fileToken: UnguessableToken,
      supportsUnimodal: boolean, iconName?: string) {
    const attachment = ComposeboxFile.createFromInjectedInput(
        fileToken, thumbnail, title, iconName ?? null);
    attachment.supportsUnimodal = supportsUnimodal;

    this.onFileContextAdded(attachment);
  }

  setInputProgrammatically(
      queryText: string, willSubmitAfterInjection: boolean) {
    this.input = queryText;

    if (!willSubmitAfterInjection) {
      // If not submitting immediately, suggestions for the new input might be
      // desired.
      this.queryAutocomplete(/*clearMatches=*/ true);
      return;
    }

    // Stop any in-flight autocomplete queries to prevent them from returning
    // and triggering an automatic selection that would overwrite the injected
    // input. This also prevents unnecessary backend work for a query that is
    // about to be submitted.
    this.getSearchboxHandler().stopAutocomplete(/*clearResult=*/ true);

    // Clear lastQueriedInput to ensure that if any autocomplete results still
    // arrive (e.g., if stopAutocomplete didn't stop them in time), they will
    // be ignored because result.input won't match lastQueriedInput.
    this.lastQueriedInput = '';

    // Clear any existing matches to ensure the dropdown is hidden and no stale
    // matches are displayed or interactable while waiting for submission.
    this.clearAutocompleteMatches();
  }

  // TODO(crbug.com/486707842): Move this to contextual tasks composebox.
  private updateAutoSuggestedTabContext_(tab: TabInfo|null) {
    if (this.smartTabSharingActive) {
      if (this.automaticActiveTab_) {
        this.deleteFile(this.automaticActiveTab_.uuid);
        this.automaticActiveTab_ = null;
      }
      return;
    }

    const shouldDeleteAutomaticActiveTab = this.automaticActiveTab_ &&
        (!tab || this.automaticActiveTab_.url !== tab.url);
    if (shouldDeleteAutomaticActiveTab) {
      this.deleteFile(this.automaticActiveTab_!.uuid);
      this.automaticActiveTab_ = null;

      // TODO(crbug.com/482150500): Correctly query for url based suggestions
      // when delayed tab is present. Right now, while url-based suggestions are
      // not set-up, clear the autocomplete matches.
      if (!tab) {
        this.queryAutocomplete(/* clearMatches= */ true);
      }
      return;
    }

    if (tab) {
      // Ignore the `TabInfo` update if there is a matching
      // `automaticActiveTab_`, unless the title has changed.
      if (this.automaticActiveTab_ &&
          tab.url === this.automaticActiveTab_.url &&
          tab.tabId === this.automaticActiveTab_.tabId) {
        if (this.automaticActiveTab_.name !== tab.title) {
          const updatedFile = new ComposeboxFile(
              this.automaticActiveTab_.uuid, tab.title,
              this.automaticActiveTab_.type, this.automaticActiveTab_.inputType,
              this.automaticActiveTab_);
          this.automaticActiveTab_ = updatedFile;
          const fileMap = new Map(this.files);
          fileMap.set(updatedFile.uuid, updatedFile);
          this.files = fileMap;
        }
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
      this.addTabContextHandleCallback(
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

  override async addTabContextHandleCallback(
      tabUpload: TabUpload, replaceAutoActiveTabToken: boolean = false):
      Promise<ComposeboxFile|null> {
    const attachment = await super.addTabContextHandleCallback(
        tabUpload, replaceAutoActiveTabToken, (attachment) => {
          // Do not reset pending active tab to avoid overwriting
          // synchronous "pending statuses" that are queued (since this
          // function is asynchronous and can run much later).
          if (replaceAutoActiveTabToken) {
            this.automaticActiveTab_ =
                Object.assign(attachment, {uuid: attachment.uuid});
          }
        });

    if (!attachment) {
      return null;
    }
    // Adding a tab is asynchronous. For auto active tabs, a title update
    // might be received after the upload process has been started. In order
    // to prevent adding duplicate chips from this update, simply update the
    // title of the initial upload instead based on whatever the latest
    // title update received is.
    if (replaceAutoActiveTabToken && this.automaticActiveTab_) {
      if (this.automaticActiveTab_.name !==
          this.pendingAutomaticActiveTabTitle_) {
        const updatedFile = new ComposeboxFile(
            this.automaticActiveTab_.uuid,
            this.pendingAutomaticActiveTabTitle_,
            this.automaticActiveTab_.type,
            this.automaticActiveTab_.inputType,
            this.automaticActiveTab_);
        this.automaticActiveTab_ = updatedFile;
        const fileMap = new Map(this.files);
        fileMap.set(updatedFile.uuid, updatedFile);
        this.files = fileMap;
      }
    }
    return attachment;
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

  // TODO(crbug.com/486706573): Refactor this function and move the common logic
  // to the mixin class. Move embedder specific logic to the embedder class.
  override updateInputPlaceholder() {
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

    super.updateInputPlaceholder();
  }

  protected onComposeboxFocusin_(e: FocusEvent) {
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

  protected onComposeboxFocusout_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    // If the the composebox is collapsible and empty, collapse it.
    // Else, keep the composebox expanded.
    this.expanding_ = this.isCollapsible ? this.submitEnabled : true;
    this.pageHandler_.focusChanged(false);
    this.fire('composebox-focus-out');
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
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
    }
    // Query for ZPS even if there's no context.
    if (this.showZps) {
      // Clear the autocomplete matches here, as failure to do so triggers a
      // DCHECK in `ZpsSection::InitMatches()` whenever the user tries to upload
      // a file after having uploaded an invalid file earlier in the session.
      this.queryAutocomplete(/* clearMatches= */ true);
    }
  }

  // TODO(crbug.com/486706573): Common logic is moved to the mixin class. Move
  // embedder specific logic to the embedder class.
  override closeComposebox() {
    super.closeComposebox();

    if (this.isCollapsible) {
      this.expanding_ = false;
      this.animationState = GlowAnimationState.NONE;
      this.getInputElement().inputElement.blur();
    }
  }

  override submitCleanup() {
    if (this.isCollapsible) {
      this.clearAllInputs(/* querySubmitted= */ true,
                          /* shouldBlockAutoSuggestedTabs= */ false);
    }

    super.submitCleanup();
  }

  // TODO(crbug.com/486706573): Refactor this function and move the common logic
  // to the mixin class. Move embedder specific logic to the embedder class.
  override onAutocompleteResultChanged(result: AutocompleteResult) {
    if (this.lastQueriedInput === null ||
        this.lastQueriedInput.trimStart() !== result.input) {
      return;
    }

    // TODO(crbug.com/460888279): This is a temporary, merge safe fix. Ideally,
    // the ACController is not sending multiple responses for a single query,
    // especially when the matches is empty. Remove this logic once a long term
    // fix is found.
    if (this.composeboxNoFlickerSuggestionsFix && this.showTypedSuggest &&
        !this.haveReceivedSynchronousAutocompleteResponse) {
      // The first autcomplete response for ZPS contains no matches, since
      // composebox doesn't support ZPS from local providers (ex. history
      // suggestion). Similarly, since composebox doesn't support local
      // providers, typed suggest first response returns a single verbatim
      // match, which doesn't show in the dropdown. To prevent closing the
      // dropdown before the actual response from the suggest server is
      // received, add the previous non-verbatim matches to this first response.
      if (this.result && this.result.matches.length > 0 &&
          result.matches.length <= 1) {
        result.matches.push(...this.result.matches.filter(
            match => match.type !== 'search-what-you-typed'));
      }
      this.haveReceivedSynchronousAutocompleteResponse = true;
    }
    this.result = result;
    /* Indicates when suggestion results have changed so that zero state
     * suggestion results in contextual tasks composebox can update accordingly.
     */
    this.fire('result-changed', result);

    const hasMatches = this.result.matches.length > 0;
    const firstMatch = hasMatches ? this.result.matches[0] : null;
    // Zero suggest matches are not allowed to be default. Therefore, this
    // makes sure zero suggest results aren't focused when they are returned.
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      this.selectFirstMatch();
    } else if (
        this.input.trim() && hasMatches && this.selectedMatchIndex >= 0 &&
        this.selectedMatchIndex < this.result.matches.length) {
      // Restore the selection and update the input. Don't restore when the
      // user deletes all their input and autocomplete is queried or else the
      // empty input will change to the value of the first result.
      this.$.matches.selectIndex(this.selectedMatchIndex);

      // Set the selected match since the `selectedMatchIndex` does not change
      // (and therefore `selectedMatch` does not get updated since
      // `onSelectedMatchIndexChanged_` is not called).
      this.selectedMatch = this.result.matches[this.selectedMatchIndex]!;
      this.input = this.selectedMatch.fillIntoEdit;
    } else {
      this.$.matches.unselect();
    }

    // Smart compose hints should only be updated from the async response.
    // This prevents the hint flicker from an empty smart compose response
    // in the synchronous pass.
    if (this.haveReceivedSynchronousAutocompleteResponse) {
      // Populate the smart compose suggestion.
      const nextHint = this.result.smartComposeInlineHint?.trim() ?
          this.result.smartComposeInlineHint :
          '';
      if (this.smartComposeInlineHint !== nextHint) {
        this.smartComposeInlineHint = nextHint;
      }

      // Smart compose stats are incremented on every response from the
      // server.
      if (this.smartComposeInlineHint) {
        this.smartComposeStats.shownCount++;
        this.smartComposeStats.shownLength +=
            this.smartComposeInlineHint.length;
      }
    }

    this.haveReceivedSynchronousAutocompleteResponse = true;
  }


  // TODO(crbug.com/486706573): common logic is moved
  // to the mixin class. Move embedder specific logic to the embedder class.
  override clearAllInputs(
      querySubmitted: boolean, shouldBlockAutoSuggestedTabs: boolean) {
    // Reset side-panel specific suggested tab context URL/Title pointers
    this.automaticActiveTab_ = null;
    this.pendingAutomaticActiveTabUrl_ = '';
    this.pendingAutomaticActiveTabTitle_ = '';
    super.clearAllInputs(querySubmitted, shouldBlockAutoSuggestedTabs);
  }

  override shouldDisableFileInputs() {
    return super.shouldDisableFileInputs() ||
        this.entrypointName === 'ContextualTasks';
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    const errorType = fileAttachment.errorType ?? null;
    if (errorType) {
      const processFilesError = mapUploadErrorToProcessFilesError(
          errorType as ContextUploadErrorType);
      if (processFilesError !== ProcessFilesError.NONE) {
        this.handleProcessFilesError(processFilesError);
        this.deleteFileContext(fileAttachment.uuid);
        return;
      }
    }

    const pendingStatus = this.files.get(fileAttachment.uuid)?.status;
    const composeboxFile = ComposeboxFile.createFromFile(
        fileAttachment.uuid as unknown as UnguessableToken,
        {name: fileAttachment.name, type: fileAttachment.mimeType},
        pendingStatus ?? ContextUploadStatus.kNotUploaded,
        {dataUrl: fileAttachment.imageDataUrl ?? null, supportsUnimodal: true});
    this.onFileContextAdded(composeboxFile);
  }

  // TODO(crbug.com/486707998): Move this to omnibox composebox.
  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContextHandleCallback({
      tabId: tabAttachment.tabId,
      title: tabAttachment.title,
      url: tabAttachment.url,
      delayUpload: /*delay_upload=*/ false,
      origin: TabUploadOrigin.OTHER,
    } as TabUpload);
  }

  addFileContextForTesting(file: ComposeboxFile) {
    this.onFileContextAdded(file);
  }

  // TODO(crbug.com/486707842): Move this to contextual tasks composebox.
  setAutomaticActiveTabForTesting(file: ComposeboxFile) {
    this.automaticActiveTab_ = file;
  }

  // TODO(crbug.com/486707842): Move this to contextual tasks composebox.
  updateAutoSuggestedTabContextForTesting(tab: TabInfo|null) {
    this.updateAutoSuggestedTabContext_(tab);
  }

  // TODO: crbug.com/486707842 - Move to the Contextual Tasks embedder
  override onSmartTabSharingActiveChanged(e: CustomEvent<{active: boolean}>) {
    super.onSmartTabSharingActiveChanged(e);
    if (e.detail.active) {
      this.clearContextForSmartTabSharingActive_();
    }
  }

  private clearContextForSmartTabSharingActive_() {
    this.clearManualTabs_();
    if (this.automaticActiveTab_) {
      const uuid = this.automaticActiveTab_.uuid;
      this.automaticActiveTab_ = null;
      this.deleteFile(uuid, /*fromUserAction=*/ false);
    }
  }

  // TODO: crbug.com/486707842 - Move to the Contextual Tasks embedder
  private clearManualTabs_() {
    const fileMap = new Map(this.files);
    for (const [uuid, file] of fileMap.entries()) {
      if (file.type === 'tab' &&
          (!this.automaticActiveTab_ ||
           file.uuid !== this.automaticActiveTab_.uuid)) {
        this.deleteFile(uuid, /*fromUserAction=*/ false);
      }
    }
  }
}
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/Componentization.md)

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
