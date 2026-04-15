// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_dropdown.js';
import './searchbox_icon.js';
import './searchbox_thumbnail.js';
import './searchbox_input.js';

import type {ComposeboxState, ContextualUpload} from '//resources/cr_components/composebox/common.js';
import {ContextType, recordContextAdditionMethod, recordContextualElementClickedMetric, recordModelModeSelection, recordToolModeSelection} from '//resources/cr_components/composebox/common.js';
import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageCallbackRouter, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './searchbox.css.js';
import {getHtml} from './searchbox.html.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';
import type {SearchboxMixinInterface} from './searchbox_mixin.js';
import {SearchboxMixin} from './searchbox_mixin.js';

// Register --placeholder-opacity as type <number> so that we can animate it.
CSS.registerProperty({
  name: '--placeholder-opacity',
  syntax: '<number>',
  initialValue: '1',
  inherits: true,
});


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
    SearchboxMixinInterface {
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

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      enableThumbnailSizingTweaks_: {
        type: Boolean,
        reflect: true,
      },

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

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor canShowSecondarySide: boolean = false;
  accessor colorSourceIsBaseline: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor isDark: boolean = false;
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor searchboxLayoutMode: string = '';
  accessor showThumbnail: boolean = false;
  accessor placeholderText: string = '';
  protected accessor enableThumbnailSizingTweaks_: boolean =
      loadTimeData.getBoolean('enableThumbnailSizingTweaks');
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor thumbnailUrl_: string = '';
  protected accessor isThumbnailDeletable_: boolean = false;
  protected accessor useWebkitSearchIcons_: boolean = false;

  protected callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerInterface;

  private autocompleteResultChangedListenerId_: number|null = null;
  private thumbnailChangedListenerId_: number|null = null;

  constructor() {
    performance.mark('realbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
    this.thumbnailChangedListenerId_ =
        this.callbackRouter_.setThumbnail.addListener(
            this.onSetThumbnail_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    assert(this.thumbnailChangedListenerId_);
    this.callbackRouter_.removeListener(this.thumbnailChangedListenerId_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ =
          this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline;
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('result') ||
        changedPrivateProperties.has('selectedMatchIndex')) {
      this.selectedMatch = this.computeSelectedMatch_();
    }

    if (changedPrivateProperties.has('thumbnailUrl_')) {
      this.showThumbnail = !!this.thumbnailUrl_;
    }
  }

  override firstUpdated() {
    performance.measure('realbox-creation', 'realbox-creation-start');
    this.initialInputScrollHeight = this.$.input.scrollHeight;
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

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e);
  }

  protected onSearchboxInputFilesPasted_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files, ComposeboxContextAddedMethod.COPY_PASTE);
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

  protected openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified) {
    if (mode !== ToolMode.kUnspecified) {
      recordToolModeSelection(mode, this.composeboxSource, 'ClassicPopup');
    }
    if (model !== ModelMode.kUnspecified) {
      recordModelModeSelection(model, this.composeboxSource, 'ClassicPopup');
    }

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

  protected processFiles_(
      files: FileList|null,
      contextAdditionMethod: ComposeboxContextAddedMethod) {
    if (!files || files.length === 0) {
      return;
    }
    recordContextAdditionMethod(contextAdditionMethod, this.composeboxSource);

    if (contextAdditionMethod === ComposeboxContextAddedMethod.CONTEXT_MENU) {
      // In practice, the `files` list will only contain a single file when
      // using the CONTEXT_MENU context addition method in the searchbox.
      for (const file of files) {
        const contextType =
            file.type.includes('image') ? ContextType.IMAGE : ContextType.FILE;
        recordContextualElementClickedMetric(
            this.composeboxSource, 'ClassicPopup', contextType);
      }
    }

    this.openComposebox_(Array.from(files, (file) => ({file})));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox': SearchboxElement;
  }
}

customElements.define(SearchboxElement.is, SearchboxElement);
