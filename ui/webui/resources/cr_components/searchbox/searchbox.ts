// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_dropdown.js';
import './searchbox_icon.js';
import './searchbox_thumbnail.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NavigationPredictor} from './omnibox.mojom-webui.js';
import {getTemplate} from './searchbox.html.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxIconElement} from './searchbox_icon.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter, PageHandlerInterface} from './searchbox.mojom-webui.js';
import {SideType} from './searchbox.mojom-webui.js';
import {decodeString16, mojoString16} from './utils.js';

interface Input {
  text: string;
  inline: string;
}

interface InputUpdate {
  text?: string;
  inline?: string;
  moveCursorToEnd?: boolean;
}

export interface SearchboxElement {
  $: {
    icon: SearchboxIconElement,
    input: HTMLInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

const SearchboxElementBase = I18nMixin(WebUiListenerMixin(PolymerElement));

/** A real search box that behaves just like the Omnibox. */
export class SearchboxElement extends SearchboxElementBase {
  static get is() {
    return 'cr-searchbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
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
        reflectToAttribute: true,
      },

      colorSourceIsBaseline: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether the cr-searchbox-dropdown should be visible. */
      dropdownIsVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether the searchbox should match the searchbox. */
      matchSearchbox: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxMatchSearchboxTheme'),
        reflectToAttribute: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxLensSearch'),
        reflectToAttribute: true,
      },

      searchboxChromeRefreshTheming: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxCr23Theming'),
        reflectToAttribute: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxCr23SteadyStateShadow'),
        reflectToAttribute: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      isLensSearchbox_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isLensSearchbox'),
        reflectToAttribute: true,
      },

      /**
       * Whether user is deleting text in the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      isDeletingInput_: {
        type: Boolean,
        value: false,
      },

      /**
       * The 'Enter' keydown event that was ignored due to matches being stale.
       * Used to navigate to the default match once up-to-date matches arrive.
       */
      lastIgnoredEnterEvent_: {
        type: Object,
        value: null,
      },

      /**
       * Last state of the input (text and inline autocompletion). Updated
       * by the user input or by the currently selected autocomplete match.
       */
      lastInput_: {
        type: Object,
        value: {text: '', inline: ''},
      },

      /** The last queried input text. */
      lastQueriedInput_: {
        type: String,
        value: null,
      },

      /**
       * True if user just pasted into the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      pastedInInput_: {
        type: Boolean,
        value: false,
      },

      placeholderText_: {
        type: String,
        computed: `computePlaceholderText_(showThumbnail)`,
      },

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {
        type: String,
        value: () => loadTimeData.getString('searchboxDefaultIcon'),
      },

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxVoiceSearch'),
        reflectToAttribute: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchboxLensSearch'),
        reflectToAttribute: true,
      },

      result_: {
        type: Object,
      },

      /** The currently selected match, if any. */
      selectedMatch_: {
        type: Object,
        computed: `computeSelectedMatch_(result_, selectedMatchIndex_)`,
      },

      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <cr-searchbox-dropdown> API to change selection.
       */
      selectedMatchIndex_: {
        type: Number,
        value: -1,
      },

      showThumbnail: {
        type: Boolean,
        computed: `computeShowThumbnail_(thumbnailUrl_)`,
        reflectToAttribute: true,
      },

      thumbnailUrl_: {
        type: String,
        value: '',
      },

      /** The value of the input element's 'aria-live' attribute. */
      inputAriaLive_: {
        type: String,
        computed: `computeInputAriaLive_(selectedMatch_)`,
      },
    };
  }

  colorSourceIsBaseline: boolean;
  dropdownIsVisible: boolean;
  hadSecondarySide: boolean;
  hasSecondarySide: boolean;
  isDark: boolean;
  matchSearchbox: boolean;
  searchboxLensSearchEnabled: boolean;
  searchboxChromeRefreshTheming: boolean;
  searchboxSteadyStateShadow: boolean;
  showThumbnail: boolean;
  private inputAriaLive_: string;
  private isDeletingInput_: boolean;
  private lastIgnoredEnterEvent_: KeyboardEvent|null;
  private lastInput_: Input;
  private lastQueriedInput_: string|null;
  private pastedInInput_: boolean;
  private placeholderText_: string;
  private searchboxIcon_: string;
  private searchboxVoiceSearchEnabled_: boolean;
  private searchboxLensSearchEnabled_: boolean;
  private result_: AutocompleteResult|null;
  private selectedMatch_: AutocompleteMatch|null;
  private selectedMatchIndex_: number;
  private thumbnailUrl_: string;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;
  private inputTextChangedListenerId_: number|null = null;
  private thumbnailChangedListenerId_: number|null = null;

  constructor() {
    performance.mark('realbox-creation-start');
    super();
    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  private computeInputAriaLive_(): string {
    return this.selectedMatch_ ? 'off' : 'polite';
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged_.bind(this));
    this.inputTextChangedListenerId_ =
        this.callbackRouter_.setInputText.addListener(
            this.onSetInputText_.bind(this));
    this.thumbnailChangedListenerId_ =
        this.callbackRouter_.setThumbnail.addListener(
            this.onSetThumbnail_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    assert(this.inputTextChangedListenerId_);
    this.callbackRouter_.removeListener(this.inputTextChangedListenerId_);
    assert(this.thumbnailChangedListenerId_);
    this.callbackRouter_.removeListener(this.thumbnailChangedListenerId_);
  }

  override ready() {
    super.ready();
    performance.measure('realbox-creation', 'realbox-creation-start');
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !== decodeString16(result.input)) {
      return;  // Stale result; ignore.
    }

    this.result_ = result;
    const hasMatches = result?.matches?.length > 0;
    const hasPrimaryMatches = result?.matches?.some(match => {
      const sideType =
          result.suggestionGroupsMap[match.suggestionGroupId]?.sideType ||
          SideType.kDefaultPrimary;
      return sideType === SideType.kDefaultPrimary;
    });
    this.dropdownIsVisible = hasPrimaryMatches;

    this.$.input.focus();

    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      // Select the default match and update the input.
      this.$.matches.selectFirst();
      this.updateInput_({
        text: this.lastQueriedInput_,
        inline: decodeString16(firstMatch.inlineAutocompletion) || '',
      });

      // Navigate to the default up-to-date match if the user typed and pressed
      // 'Enter' too fast.
      if (this.lastIgnoredEnterEvent_) {
        this.navigateToMatch_(0, this.lastIgnoredEnterEvent_);
        this.lastIgnoredEnterEvent_ = null;
      }
    } else if (
        hasMatches && this.selectedMatchIndex_ !== -1 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input.
      this.$.matches.selectIndex(this.selectedMatchIndex_);
      this.updateInput_({
        text: decodeString16(this.selectedMatch_!.fillIntoEdit),
        inline: '',
        moveCursorToEnd: true,
      });
    } else {
      // Remove the selection and update the input.
      this.$.matches.unselect();
      this.updateInput_({
        inline: '',
      });
    }
  }

  private onSetInputText_(inputText: string) {
    this.updateInput_({text: inputText, inline: ''});
  }

  private onSetThumbnail_(thumbnailUrl: string) {
    this.thumbnailUrl_ = thumbnailUrl;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onHeaderFocusin_() {
    // The header got focus. Unselect the selected match and clear the input.
    assert(this.lastQueriedInput_ === '');
    this.$.matches.unselect();
    this.updateInput_({text: '', inline: ''});
  }

  private onInputCutCopy_(e: ClipboardEvent) {
    // Only handle cut/copy when input has content and it's all selected.
    if (!this.$.input.value || this.$.input.selectionStart !== 0 ||
        this.$.input.selectionEnd !== this.$.input.value.length ||
        !this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (this.selectedMatch_ && !this.selectedMatch_.isSearchType) {
      e.clipboardData!.setData(
          'text/plain', this.selectedMatch_.destinationUrl.url);
      e.preventDefault();
      if (e.type === 'cut') {
        this.updateInput_({text: '', inline: ''});
        this.clearAutocompleteMatches_();
      }
    }
  }

  private onInputFocus_() {
    this.pageHandler_.onFocusChanged(true);
  }

  private onInputInput_(e: InputEvent) {
    const inputValue = this.$.input.value;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    if (lastInputValue === inputValue) {
      return;
    }

    this.updateInput_({text: inputValue, inline: ''});

    // If a character has been typed, mark 'CharTyped'. Otherwise clear it. If
    // 'CharTyped' mark already exists, there's a pending typed character for
    // which the results have not been painted yet. In that case, keep the
    // earlier mark.
    if (loadTimeData.getBoolean('reportMetrics')) {
      const charTyped = !this.isDeletingInput_ && !!inputValue.trim();
      const metricsReporter = MetricsReporterImpl.getInstance();
      if (charTyped) {
        if (!metricsReporter.hasLocalMark('CharTyped')) {
          metricsReporter.mark('CharTyped');
        }
      } else {
        metricsReporter.clearMark('CharTyped');
      }
    }

    if (inputValue.trim()) {
      // TODO(crbug.com/40732045): Rather than disabling inline autocompletion
      // when the input event is fired within a composition session, change the
      // mechanism via which inline autocompletion is shown in the searchbox.
      this.queryAutocomplete_(inputValue, e.isComposing);
    } else {
      this.clearAutocompleteMatches_();
    }

    this.pastedInInput_ = false;
  }

  private onInputKeydown_(e: KeyboardEvent) {
    // Ignore this event if the input does not have any inline autocompletion.
    if (!this.lastInput_.inline) {
      return;
    }

    const inputValue = this.$.input.value;
    const inputSelection = inputValue.substring(
        this.$.input.selectionStart!, this.$.input.selectionEnd!);
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    // If the current input state (its value and selection) matches its last
    // state (text and inline autocompletion) and the user types the next
    // character in the inline autocompletion, stop the keydown event. Just move
    // the selection and requery autocomplete. This is needed to avoid flicker.
    if (inputSelection === this.lastInput_.inline &&
        inputValue === lastInputValue &&
        this.lastInput_.inline[0].toLocaleLowerCase() ===
            e.key.toLocaleLowerCase()) {
      const text = this.lastInput_.text + e.key;
      assert(text);
      this.updateInput_({
        text: text,
        inline: this.lastInput_.inline.substr(1),
      });

      // If 'CharTyped' mark already exists, there's a pending typed character
      // for which the results have not been painted yet. In that case, keep the
      // earlier mark.
      if (loadTimeData.getBoolean('reportMetrics')) {
        const metricsReporter = MetricsReporterImpl.getInstance();
        if (!metricsReporter.hasLocalMark('CharTyped')) {
            metricsReporter.mark('CharTyped');
        }
      }

      this.queryAutocomplete_(this.lastInput_.text);
      e.preventDefault();
    }
  }

  private onInputKeyup_(e: KeyboardEvent) {
    if (e.key !== 'Tab') {
      return;
    }

    if (!this.dropdownIsVisible) {
      // Query for zero-prefix matches if user is tabbing into an empty input
      // and matches are not visible.
      if (!this.$.input.value) {
        this.queryAutocomplete_('');
      } else if (this.showThumbnail) {
        // Query current input if tabbing into input while thumbnail is showing
        // and matches are not visible.
        this.queryAutocomplete_(this.$.input.value);
      }
    }
  }

  private onInputMouseDown_(e: MouseEvent) {
    // Non-main (generally left) mouse clicks are ignored.
    if (e.button !== 0) {
      return;
    }

    // Query autocomplete if dropdown is not visible
    if (this.dropdownIsVisible) {
      return;
    }
    this.queryAutocomplete_(this.$.input.value);
  }

  private onInputPaste_() {
    this.pastedInInput_ = true;
  }

  private onInputWrapperFocusout_(e: FocusEvent) {
    // Hide the matches and stop autocomplete only when the focus goes outside
    // of the searchbox wrapper.
    if (!this.$.inputWrapper.contains(e.relatedTarget as Element)) {
      if (this.lastQueriedInput_ === '') {
        // Clear the input as well as the matches if the input was empty when
        // the matches arrived.
        this.updateInput_({text: '', inline: ''});
        this.clearAutocompleteMatches_();
      } else {
        this.dropdownIsVisible = false;

        // Stop autocomplete but leave (potentially stale) results and continue
        // listening for key presses. These stale results should never be shown.
        // They correspond to the potentially stale suggestion left in the
        // searchbox when blurred. That stale result may be navigated to by
        // focusing and pressing 'Enter'.
        this.pageHandler_.stopAutocomplete(/*clearResult=*/ false);
      }
      this.pageHandler_.onFocusChanged(false);
    }
  }

  private onInputWrapperKeydown_(e: KeyboardEvent) {
    const KEYDOWN_HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Backspace',
      'Delete',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
      'Tab',
    ];
    if (!KEYDOWN_HANDLED_KEYS.includes(e.key)) {
      return;
    }

    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    if (this.showThumbnail) {
      const thumbnail =
          this.shadowRoot!.querySelector<HTMLElement>('cr-searchbox-thumbnail');
      if (thumbnail === this.shadowRoot!.activeElement) {
        if (e.key === 'Backspace' || e.key === 'Enter') {
          // Remove thumbnail, focus input, and notify browser.
          this.thumbnailUrl_ = '';
          this.$.input.focus();
          this.clearAutocompleteMatches_();
          this.pageHandler_.onThumbnailRemoved();
          const inputValue = this.$.input.value;
          // Clearing the autocomplete matches above doesn't allow for
          // navigation directly after removing the thumbnail. Must manually
          // query autocomplete after removing the thumbnail since the
          // thumbnail isn't part of the text input.
          this.queryAutocomplete_(inputValue);
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
          this.$.input.selectionStart === 0 &&
          this.$.input.selectionEnd === 0 &&
          this.$.input === this.shadowRoot!.activeElement &&
          (e.key === 'Backspace' || (e.key === 'Tab' && e.shiftKey))) {
        // Backspacing or shift-tabbing the thumbnail results in the thumbnail
        // being focused.
        thumbnail?.focus();
        e.preventDefault();
      }
    }

    if (e.key === 'Backspace' || e.key === 'Tab') {
      return;
    }

    // ArrowUp/ArrowDown query autocomplete when matches are not visible.
    if (!this.dropdownIsVisible) {
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        const inputValue = this.$.input.value;
        if (inputValue.trim() || !inputValue) {
          this.queryAutocomplete_(inputValue);
        }
        e.preventDefault();
        return;
      }
    }

    // Do not handle the following keys if there are no matches available.
    if (!this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (e.key === 'Delete') {
      if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
        if (this.selectedMatch_ && this.selectedMatch_.supportsDeletion) {
          this.pageHandler_.deleteAutocompleteMatch(
              this.selectedMatchIndex_, this.selectedMatch_.destinationUrl);
          e.preventDefault();
        }
      }
      return;
    }

    // Do not handle the following keys if inside an IME composition session.
    if (e.isComposing) {
      return;
    }

    if (e.key === 'Enter') {
      const array: HTMLElement[] = [this.$.matches, this.$.input];
      if (array.includes(e.target as HTMLElement)) {
        if (this.lastQueriedInput_ !== null &&
            this.lastQueriedInput_.trimStart() ===
                decodeString16(this.result_.input)) {
          if (this.selectedMatch_) {
            this.navigateToMatch_(this.selectedMatchIndex_, e);
          }
        } else {
          // User typed and pressed 'Enter' too quickly. Ignore this for now
          // because the matches are stale. Navigate to the default match (if
          // one exists) once the up-to-date matches arrive.
          this.lastIgnoredEnterEvent_ = e;
          e.preventDefault();
        }
      }
      return;
    }

    // Do not handle the following keys if there are key modifiers.
    if (hasKeyModifiers(e)) {
      return;
    }

    // Clear the input as well as the matches when 'Escape' is pressed if the
    // the first match is selected or there are no selected matches.
    if (e.key === 'Escape' && this.selectedMatchIndex_ <= 0) {
      this.updateInput_({text: '', inline: ''});
      this.clearAutocompleteMatches_();
      e.preventDefault();
      return;
    }

    if (e.key === 'ArrowDown') {
      this.$.matches.selectNext();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, this.selectedMatch_!.destinationUrl,
          NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, this.selectedMatch_!.destinationUrl,
          NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      this.$.matches.selectFirst();
    } else if (e.key === 'PageDown') {
      this.$.matches.selectLast();
    }
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot!.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }

    // Update the input.
    const newFill = decodeString16(this.selectedMatch_!.fillIntoEdit);
    const newInline = this.selectedMatchIndex_ === 0 &&
            this.selectedMatch_!.allowedToBeDefaultMatch ?
        decodeString16(this.selectedMatch_!.inlineAutocompletion) :
        '';
    const newFillEnd = newFill.length - newInline.length;
    const text = newFill.substr(0, newFillEnd);
    assert(text);
    this.updateInput_({
      text: text,
      inline: newInline,
      moveCursorToEnd: newInline.length === 0,
    });
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  private onMatchFocusin_(e: CustomEvent<number>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail);
    // Input selection (if any) likely drops due to focus change. Simply fill
    // the input with the match and move the cursor to the end.
    this.updateInput_({
      text: decodeString16(this.selectedMatch_!.fillIntoEdit),
      inline: '',
      moveCursorToEnd: true,
    });
  }

  private onMatchClick_() {
    this.clearAutocompleteMatches_();
  }

  private onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  private onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  private onRemoveThumbnailClick_() {
    /* Remove thumbnail, focus input, and notify browser. */
    this.thumbnailUrl_ = '';
    this.$.input.focus();
    this.clearAutocompleteMatches_();
    this.pageHandler_.onThumbnailRemoved();
    // Clearing the autocomplete matches above doesn't allow for
    // navigation directly after removing the thumbnail. Must manually
    // query autocomplete after removing the thumbnail since the
    // thumbnail isn't part of the text input.
    const inputValue = this.$.input.value;
    this.queryAutocomplete_(inputValue);
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeSelectedMatch_(): AutocompleteMatch|null {
    if (!this.result_ || !this.result_.matches) {
      return null;
    }
    return this.result_.matches[this.selectedMatchIndex_] || null;
  }

  private computeShowThumbnail_(): boolean {
    return !!this.thumbnailUrl_;
  }

  private computePlaceholderText_(): string {
    return this.showThumbnail ? this.i18n('searchBoxHintMultimodal') :
                                this.i18n('searchBoxHint');
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  private clearAutocompleteMatches_() {
    this.dropdownIsVisible = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.pageHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = null;
  }

  private navigateToMatch_(matchIndex: number, e: KeyboardEvent|MouseEvent) {
    assert(matchIndex >= 0);
    const match = this.result_!.matches[matchIndex];
    assert(match);
    this.pageHandler_.openAutocompleteMatch(
        matchIndex, match.destinationUrl, this.dropdownIsVisible,
        (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
        e.shiftKey);
    this.updateInput_({
      text: decodeString16(this.selectedMatch_!.fillIntoEdit),
      inline: '',
      moveCursorToEnd: true,
    });
    this.clearAutocompleteMatches_();
    e.preventDefault();
  }

  private queryAutocomplete_(
      input: string, preventInlineAutocomplete: boolean = false) {
    this.lastQueriedInput_ = input;

    const caretNotAtEnd = this.$.input.selectionStart !== input.length;
    preventInlineAutocomplete = preventInlineAutocomplete ||
        this.isDeletingInput_ || this.pastedInInput_ || caretNotAtEnd;
    this.pageHandler_.queryAutocomplete(
        mojoString16(input), preventInlineAutocomplete);
  }

  /**
   * Updates the input state (text and inline autocompletion) with |update|.
   */
  private updateInput_(update: InputUpdate) {
    const newInput = Object.assign({}, this.lastInput_, update);
    const newInputValue = newInput.text + newInput.inline;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;

    const inlineDiffers = newInput.inline !== this.lastInput_.inline;
    const preserveSelection = !inlineDiffers && !update.moveCursorToEnd;
    let needsSelectionUpdate = !preserveSelection;

    const oldSelectionStart = this.$.input.selectionStart;
    const oldSelectionEnd = this.$.input.selectionEnd;

    if (newInputValue !== this.$.input.value) {
      this.$.input.value = newInputValue;
      needsSelectionUpdate = true;  // Setting .value blows away selection.
    }

    if (newInputValue.trim() && needsSelectionUpdate) {
      // If the cursor is to be moved to the end (implies selection should not
      // be perserved), set the selection start to same as the selection end.
      this.$.input.selectionStart = preserveSelection ? oldSelectionStart :
          update.moveCursorToEnd                      ? newInputValue.length :
                                                        newInput.text.length;
      this.$.input.selectionEnd =
          preserveSelection ? oldSelectionEnd : newInputValue.length;
    }

    this.isDeletingInput_ = lastInputValue.length > newInputValue.length &&
        lastInputValue.startsWith(newInputValue);
    this.lastInput_ = newInput;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox': SearchboxElement;
  }
}

customElements.define(SearchboxElement.is, SearchboxElement);
