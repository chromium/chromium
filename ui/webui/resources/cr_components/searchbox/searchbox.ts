// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_compose_button.js';
import './searchbox_dropdown.js';
import './searchbox_icon.js';
import './searchbox_thumbnail.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/search/animated_glow.js';
import './searchbox_input.js';

import type {ComposeboxState, ContextualUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import {GlifAnimationState, recordContextAdditionMethod} from '//resources/cr_components/composebox/common.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import type {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './searchbox.css.js';
import {getHtml} from './searchbox.html.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';
import type {SearchboxMixinInterface} from './searchbox_mixin.js';
import {SearchboxMixin} from './searchbox_mixin.js';
import {waitForLazyRender} from './utils.js';

// LINT.IfChange(GhostLoaderTagName)
const LENS_GHOST_LOADER_TAG_NAME = 'cr-searchbox-ghost-loader';
// LINT.ThenChange(/chrome/browser/resources/lens/shared/searchbox_ghost_loader.ts:GhostLoaderTagName)
// The NTP Realbox entry point is always part of the Next experience, so log
// the source value with the "crn" component.
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE = 'chrome.crn.rb';
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE = '42';

// Register --placeholder-opacity as type <number> so that we can animate it.
CSS.registerProperty({
  name: '--placeholder-opacity',
  syntax: '<number>',
  initialValue: '1',
  inherits: true,
});

import {PlaceholderTextCycler} from './placeholder_text_cycler.js';

interface ClickEventDetail {
  button: number;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
}

export interface SearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const SearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

/** A real search box that behaves just like the Omnibox. */
export class SearchboxElement extends SearchboxElementBase implements
    DragAndDropHost, SearchboxMixinInterface {
  static get is() {
    return 'cr-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties(): any {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      colorSourceIsBaseline: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflect: true,
      },

      /** The aria description to include on the input element. */
      searchboxAriaDescription: {type: String},

      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },

      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },

      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },

      cyclingPlaceholders: {
        type: Boolean,
      },

      composeboxEnabled: {type: Boolean},

      composeButtonEnabled: {type: Boolean},

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      enableThumbnailSizingTweaks_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether user is deleting text in the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      isDeletingInput_: {type: Boolean},

      /**
       * Last state of the input (text and inline autocompletion). Updated
       * by the user input or by the currently selected autocomplete match.
       */
      lastInput_: {type: Object},

      /**
       * True if user just pasted into the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      pastedInInput_: {type: Boolean},

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      showThumbnail: {
        type: Boolean,
        reflect: true,
      },

      thumbnailUrl_: {type: String},
      isThumbnailDeletable_: {type: Boolean},

      /** The value of the input element's 'aria-live' attribute. */
      inputAriaLive_: {type: String},

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      tabSuggestions_: {type: Array},
      inputState_: {type: Object},
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      animationState: {
        reflect: true,
        type: String,
      },
      showModelPicker_: {
        type: Boolean,
      },
    };
  }

  accessor canShowSecondarySide: boolean = false;
  accessor colorSourceIsBaseline: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor isDark: boolean = false;
  accessor searchboxAriaDescription: string = '';
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor searchboxLayoutMode: string = '';
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor cyclingPlaceholders: boolean = false;
  accessor composeboxEnabled: boolean = false;
  accessor composeButtonEnabled: boolean = false;
  accessor showThumbnail: boolean = false;
  accessor placeholderText: string = '';
  accessor isDraggingFile: boolean = false;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  protected accessor inputAriaLive_: string = '';
  protected accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');
  protected accessor enableThumbnailSizingTweaks_: boolean =
      loadTimeData.getBoolean('enableThumbnailSizingTweaks');
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor showModelPicker_: boolean =
      loadTimeData.valueExists('contextualMenuUsePecApi') ?
      loadTimeData.getBoolean('contextualMenuUsePecApi') :
      false;
  protected accessor thumbnailUrl_: string = '';
  protected accessor isThumbnailDeletable_: boolean = false;
  private accessor useWebkitSearchIcons_: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor inputState_: InputState|null = null;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  protected dragAndDropHandler: DragAndDropHandler|null = null;

  private autocompleteResultChangedListenerId_: number|null = null;
  private thumbnailChangedListenerId_: number|null = null;
  private onTabStripChangedListenerId_: number|null = null;
  private placeholderCycler_: PlaceholderTextCycler|null = null;
  private contextMenuOpened_: boolean = false;

  constructor() {
    performance.mark('realbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
    this.thumbnailChangedListenerId_ =
        this.callbackRouter_.setThumbnail.addListener(
            this.onSetThumbnail_.bind(this));
    this.onTabStripChangedListenerId_ =
        this.callbackRouter_.onTabStripChanged.addListener(
            this.refreshTabSuggestions_.bind(this));
    this.inputState_ = (await this.pageHandler_.getInputState()).state;
    if (this.inputState_) {
      this.inputState_.activeModel = ModelMode.kUnspecified;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    assert(this.thumbnailChangedListenerId_);
    this.callbackRouter_.removeListener(this.thumbnailChangedListenerId_);
    assert(this.onTabStripChangedListenerId_);
    this.callbackRouter_.removeListener(this.onTabStripChangedListenerId_);

    this.placeholderCycler_?.stop();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('composeButtonEnabled') ||
        changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ = this.composeButtonEnabled ||
          (this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline);
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('result') ||
        changedPrivateProperties.has('selectedMatchIndex')) {
      this.selectedMatch = this.computeSelectedMatch_();
    }

    if (changedPrivateProperties.has('selectedMatch')) {
      this.inputAriaLive_ = this.computeInputAriaLive_();
    }

    if (changedPrivateProperties.has('thumbnailUrl_')) {
      this.showThumbnail = !!this.thumbnailUrl_;
    }
  }

  override firstUpdated() {
    performance.measure('realbox-creation', 'realbox-creation-start');
    this.initialInputScrollHeight = this.$.input.scrollHeight;

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler_.getPlaceholderConfig();
        const texts = config.texts;
        if (texts.length === 0) {
          // PEC API returned no placeholders; feature is disabled.
          return;
        }
        this.placeholderText = texts[0]!;
        this.placeholderCycler_ = new PlaceholderTextCycler(
            this.$.input.inputElement, texts,
            Number(config.changeTextAnimationInterval.microseconds / 1000n),
            Number(config.fadeTextAnimationDuration.microseconds / 1000n));
        this.placeholderCycler_.start();
      });
    }
  }

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  private computeInputAriaLive_(): string {
    return this.selectedMatch ? 'off' : 'polite';
  }

  getDropTarget() {
    return this;
  }

  addDroppedFiles(files: FileList) {
    this.processFiles_(files, ComposeboxContextAddedMethod.DRAG_AND_DROP);
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    if (!isEnabled) {
      return false;
    }

    if (!this.isInputEmpty()) {
      return false;
    }

    if (this.dropdownIsVisible && this.composeButtonEnabled) {
      return false;
    }

    return true;
  }

  queryInputAutocomplete() {
    this.queryAutocomplete(this.$.input.inputElement.value, false);
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  focusInput() {
    this.$.input.focus();
  }

  blurInput() {
    this.$.input.blur();
  }

  selectAll() {
    this.$.input.select();
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private onSetThumbnail_(thumbnailUrl: string, isDeletable: boolean) {
    this.thumbnailUrl_ = thumbnailUrl;
    this.isThumbnailDeletable_ = isDeletable;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onInputFocus_() {
    this.pageHandler_.onFocusChanged(true);
    this.placeholderCycler_?.stop();
  }

  protected onInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onInputTextUpdated(e, this.isLensSearchbox_);
  }

  protected onSearchboxInputFilesPasted_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files, ComposeboxContextAddedMethod.COPY_PASTE);
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    const newlyFocusedEl = e.relatedTarget as Element;

    // If this is a Lens searchbox, treat the ghost loader as keeping searchbox
    // focus.
    // TODO(380467089): This workaround wouldn't be needed if the ghost loader
    // was part of the searchbox element. Remove this workaround once they are
    // combined.
    if (this.isLensSearchbox_ &&
        newlyFocusedEl?.tagName.toLowerCase() === LENS_GHOST_LOADER_TAG_NAME) {
      return;
    }

    super.onInputWrapperFocusout(e);
    this.placeholderCycler_?.start();
  }

  override handleKeyNavigation(e: KeyboardEvent) {
    if (this.showThumbnail) {
      const thumbnail =
          this.shadowRoot.querySelector<HTMLElement>('cr-searchbox-thumbnail');
      if (thumbnail === this.shadowRoot.activeElement) {
        if (e.key === 'Backspace' || e.key === 'Enter') {
          // Remove thumbnail, focus input, and notify browser.
          this.thumbnailUrl_ = '';
          this.$.input.focus();
          this.clearAutocompleteMatches();
          this.pageHandler_.onThumbnailRemoved();
          const inputValue = this.$.input.inputElement.value;
          // Clearing the autocomplete matches above doesn't allow for
          // navigation directly after removing the thumbnail. Must manually
          // query autocomplete after removing the thumbnail since the
          // thumbnail isn't part of the text input.
          this.queryAutocomplete(inputValue, false);
          e.preventDefault();
        } else if (e.key === 'Tab' && !e.shiftKey) {
          this.$.input.focus();
          e.preventDefault();
        } else if (
            this.dropdownIsVisible &&
            (e.key === 'ArrowUp' || e.key === 'ArrowDown')) {
          // If the dropdown is visible, arrowing up and down unfocuses the
          // thumbnail and follows standard arrow up/down behavior (selects
          // the next/previous match).
          this.$.input.focus();
        }
      } else if (
          this.isThumbnailDeletable_ &&
          this.$.input.inputElement.selectionStart === 0 &&
          this.$.input.inputElement.selectionEnd === 0 &&
          this.$.input === this.shadowRoot.activeElement &&
          (e.key === 'Backspace' || (e.key === 'Tab' && e.shiftKey))) {
        // Backspacing or shift-tabbing the thumbnail results in the thumbnail
        // being focused.
        thumbnail?.focus();
        e.preventDefault();
      }
    }

    if (this.composeButtonEnabled && e.key === 'Tab' &&
        this.$.input?.lastInput()?.inline &&
        this.$.input === this.shadowRoot.activeElement) {
      if (e.shiftKey) {
        this.$.input.setInput({inline: ''});
        return;
      }

      const newText =
          this.$.input.lastInput()!.text + this.$.input.lastInput()!.inline;
      this.$.input.setInput({
        text: newText,
        inline: '',
        moveCursorToEnd: true,
      });
      this.queryAutocomplete(newText, false);
      e.preventDefault();
      return;
    }

    super.handleKeyNavigation(e);
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected async onMatchFocusin_(e: CustomEvent<number>) {
    // Select the match that received focus.
    await this.getDropdownElement().selectIndex(e.detail);
    // Input selection (if any) likely drops due to focus change. Simply fill
    // the input with the match and move the cursor to the end.
    this.$.input.setInput({
      text: this.selectedMatch!.fillIntoEdit,
      inline: '',
      moveCursorToEnd: true,
    });
  }

  protected onMatchClick_() {
    this.clearAutocompleteMatches();
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(
        e.detail.files, ComposeboxContextAddedMethod.CONTEXT_MENU);
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    const attachment: TabUpload = {
      tabId: e.detail.id,
      url: e.detail.url,
      title: e.detail.title,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    };
    this.openComposebox_([attachment]);
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    // Only refresh tab suggestions if the context menu is opened.
    const requiresRefresh = forceRefresh || this.contextMenuOpened_;
    if (!requiresRefresh) {
      return;
    }
    const {tabs} = await this.pageHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
  }

  protected async onGetTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.pageHandler_.getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected onContextMenuClosed_() {
    this.contextMenuOpened_ = false;
    this.blur();
  }

  protected onContextMenuOpened_() {
    this.contextMenuOpened_ = true;
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onComposeClick_(e: CustomEvent<ClickEventDetail>) {
    // TODO(crbug.com/463667769): Call submitQuery here since RealboxHandler is
    // now a `ContextualSearchboxHandler`.
    this.pageHandler_.activateMetricsFunnel('AiModeButton');

    chrome.histograms.recordUserAction(
        'ContextualSearch.AiModeButtonClick.NtpRealbox');
    chrome.histograms.recordBoolean(
        'ContextualSearch.AiModeButtonClick.NtpRealbox', true);

    if (!this.composeboxEnabled || this.$.input.inputElement.value.trim()) {
      const metricName =
          'ContextualSearch.UserAction.SubmitQueryV2.WithoutContext.NewTabPage';
      chrome.histograms.recordUserAction(metricName);
      chrome.histograms.recordBoolean(metricName, true);

      // Construct navigation url.
      const searchParams = new URLSearchParams();
      searchParams.append('sourceid', 'chrome');
      searchParams.append('udm', '50');
      searchParams.append('aep', DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE);
      searchParams.append(
          'source', DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE);

      if (this.$.input.inputElement.value.trim()) {
        searchParams.append('q', this.$.input.inputElement.value.trim());
      }
      const queryUrl =
          new URL('/search', loadTimeData.getString('googleBaseUrl'));
      queryUrl.search = searchParams.toString();
      const href = queryUrl.href;

      // Handle mouse events.
      if (e.detail.ctrlKey || e.detail.metaKey) {
        window.open(href, '_blank');
      } else if (e.detail.shiftKey) {
        window.open(href, '_blank', 'noopener');
      } else {
        window.open(href, '_self');
      }
    } else {
      this.openComposebox_();
    }

    chrome.histograms.recordBoolean(
        'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
        !this.isInputEmpty());
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler_.activateMetricsFunnel('PlusButton');
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposebox_([], e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposebox_([], ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposebox_([], ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposebox_([], ToolMode.kUnspecified, e.detail.model);
  }

  protected openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified) {
    this.fire<ComposeboxState>('open-composebox', {
      text: this.$.input.inputElement.value,
      files: uploads,
      mode: mode,
      model: model,
    });
    this.setInputText('');
  }

  hasThumbnail(): boolean {
    return !!this.thumbnailUrl_;
  }

  protected onRemoveThumbnailClick_() {
    /* Remove thumbnail, focus input, and notify browser. */
    this.thumbnailUrl_ = '';
    this.$.input.focus();
    this.clearAutocompleteMatches();
    this.pageHandler_.onThumbnailRemoved();
    // Clearing the autocomplete matches above doesn't allow for
    // navigation directly after removing the thumbnail. Must manually
    // query autocomplete after removing the thumbnail since the
    // thumbnail isn't part of the text input.
    this.queryInputAutocomplete();
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeSelectedMatch_(): AutocompleteMatch|null {
    if (!this.result || !this.result.matches) {
      return null;
    }
    return this.result.matches[this.selectedMatchIndex] || null;
  }

  protected inputHasMatches_(): boolean {
    return !!this.result && !!this.result.matches &&
        this.result.matches.length > 0;
  }

  protected computePlaceholderText_(placeholderText: string): string {
    if (placeholderText) {
      return placeholderText;
    }
    return this.showThumbnail ? this.i18n('searchBoxHintMultimodal') :
                                this.i18n('searchBoxHint');
  }

  protected getThumbnailTabindex_(): string {
    // If the thumbnail can't be deleted, returning an empty string will set the
    // tabindex to nothing, which will make the thumbnail not focusable.
    return this.isThumbnailDeletable_ ? '1' : '';
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex = e.detail.value;
  }

  protected onHadSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hadSecondarySide = e.detail.value;
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  protected useCompactLayout_(): boolean {
    return this.searchboxLayoutMode === 'Compact';
  }

  private processFiles_(
      files: FileList|null,
      contextAdditionMethod: ComposeboxContextAddedMethod) {
    if (!files || files.length === 0) {
      return;
    }
    recordContextAdditionMethod(
        contextAdditionMethod, loadTimeData.getString('composeboxSource'));
    this.openComposebox_(Array.from(files, (file) => ({file})));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox': SearchboxElement;
  }
}

customElements.define(SearchboxElement.is, SearchboxElement);
