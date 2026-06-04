// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from '//resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import {SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';

/* @fileoverview Helper functions for implementing a custom searchbox. */

type Constructor<T> = new (...args: any[]) => T;

export const SearchboxMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<SearchboxMixinInterface> => {
  class SearchboxMixin extends superClass implements SearchboxMixinInterface {
    static get properties() {
      return {
        dropdownIsVisible: {
          type: Boolean,
          reflect: true,
        },

        /** The value of the input element's 'aria-live' attribute. */
        inputAriaLive: {
          type: String,
        },

        multiLineEnabled: {
          type: Boolean,
          reflect: true,
        },

        result: {
          type: Object,
        },

        selectedMatch: {
          type: Object,
        },

        selectedMatchIndex: {
          type: Number,
        },

        /** The aria description to include on the input element. */
        searchboxAriaDescription: {
          type: String,
        },

        /** Searchbox default icon (i.e., Google G icon or the search loupe). */
        searchboxIcon: {
          type: String,
        },

        showThumbnail: {
          type: Boolean,
          reflect: true,
        },
      };
    }
    composeboxSource: string = loadTimeData.valueExists('composeboxSource') ?
        loadTimeData.getString('composeboxSource') :
        'Unknown';
    accessor searchboxAriaDescription: string = '';
    accessor dropdownIsVisible: boolean = false;
    accessor lastQueriedInput: string|null = null;
    accessor multiLineEnabled: boolean = false;
    accessor result: AutocompleteResult|null = null;
    accessor selectedMatch: AutocompleteMatch|null = null;
    accessor selectedMatchIndex: number = -1;
    accessor inputAriaLive: string = '';
    accessor searchboxIcon: string = '';
    accessor showThumbnail: boolean = false;

    initialInputScrollHeight: number = 0;

    private lastIgnoredEnterEvent_: KeyboardEvent|null = null;

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      const changedPrivateProperties =
          changedProperties as Map<PropertyKey, unknown>;
      if (changedPrivateProperties.has('selectedMatch')) {
        this.inputAriaLive = this.computeInputAriaLive_();
      }
      if (changedPrivateProperties.has('result') ||
          changedPrivateProperties.has('selectedMatchIndex')) {
        this.selectedMatch = this.computeSelectedMatch_();
      }
    }

    override updated(changedProperties: PropertyValues<this>) {
      super.updated(changedProperties);

      const changedPrivateProperties =
          changedProperties as Map<PropertyKey, unknown>;
      if (changedPrivateProperties.has('showThumbnail')) {
        const dropdown = this.getDropdownElement();
        if (dropdown) {
          dropdown.showThumbnail = this.showThumbnail;
        }
      }
    }

    getInputElement(): SearchboxInputElement {
      assertNotReached();
    }

    getDropdownElement(): SearchboxDropdownElement {
      assertNotReached();
    }

    getWrapperElement(): HTMLElement {
      assertNotReached();
    }

    pageHandler(): PageHandlerInterface {
      assertNotReached();
    }

    /**
     * Clears the autocomplete result on the page and on the autocomplete
     * backend.
     */
    clearAutocompleteMatches() {
      this.dropdownIsVisible = false;
      this.result = null;
      this.getDropdownElement().unselect();
      this.pageHandler().stopAutocomplete(/*clearResult=*/ true);
      // Autocomplete sends updates once it is stopped. Invalidate those results
      // by setting the |this.lastQueriedInput| to its default value.
      this.lastQueriedInput = null;
    }

    queryAutocomplete(
        input: string, preventInlineAutocomplete: boolean = false) {
      this.lastQueriedInput = input;

      preventInlineAutocomplete = preventInlineAutocomplete ||
          this.getInputElement().preventInlineAutocomplete(input);
      // Get the cursor position from the DOM. Since DOM updates are async in
      // lit, if the input was set via code rather than user interaction, the
      // cursor position fetched from the dom would be stale, so use the text
      // length instead, since that's what the dom cursor position will be set
      // to once the update propagates.
      const cursorPosition =
          this.getInputElement().inputElement.value === input ?
          this.getInputElement().inputElement.selectionStart || 0 :
          input.length;
      this.pageHandler().queryAutocomplete(
          input, preventInlineAutocomplete, cursorPosition);

      this.dispatchEvent(new CustomEvent('query-autocomplete', {
        bubbles: true,
        composed: true,
        detail: {inputValue: input},
      }));
    }

    navigateToMatch(matchIndex: number, e: KeyboardEvent|MouseEvent) {
      assert(matchIndex >= 0);
      const match = this.result!.matches[matchIndex];
      assert(match);
      this.pageHandler().openAutocompleteMatch(
          matchIndex, match.destinationUrl, this.dropdownIsVisible,
          (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
          e.shiftKey);
      this.getInputElement().setInput({
        text: match.fillIntoEdit,
        inline: '',
        moveCursorToEnd: true,
      });
      this.clearAutocompleteMatches();
      e.preventDefault();
    }

    // TODO(b/519266700): Remove/refactor from mixin if only used by one
    // embedder.
    async onAutocompleteResultChanged(result: AutocompleteResult) {
      if (this.lastQueriedInput === null ||
          this.lastQueriedInput.trimStart() !== result.input) {
        return;  // Stale result; ignore.
      }

      this.result = result;
      const hasMatches = this.hasMatches();
      const hasPrimaryMatches = result.matches?.some(match => {
        const sideType =
            result.suggestionGroupsMap[match.suggestionGroupId]?.sideType ||
            SideType.kDefaultPrimary;
        return sideType === SideType.kDefaultPrimary;
      });

      this.dropdownIsVisible = hasPrimaryMatches;

      // In multi-line mode, suppress the dropdown when text wraps or when the
      // only match is the mirror query.
      if (this.multiLineEnabled && this.dropdownIsVisible) {
        const isUserTyping = result.input.trim().length > 0;
        if (isUserTyping &&
            this.shouldSuppressDropdownForMultiline_(
                result.matches?.length || 0)) {
          this.dropdownIsVisible = false;
        }
      }

      const firstMatch = hasMatches ? this.result.matches[0] : null;
      if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
        // Select the default match and update the input.
        this.getDropdownElement().selectFirst();
        this.getInputElement().setInput({
          text: this.lastQueriedInput,
          inline: firstMatch.inlineAutocompletion,
        });

        // Navigate to the default up-to-date match if the user typed and
        // pressed 'Enter' too fast.
        if (this.lastIgnoredEnterEvent_) {
          this.navigateToMatch(0, this.lastIgnoredEnterEvent_);
          this.lastIgnoredEnterEvent_ = null;
        }
      } else if (
          this.getInputElement().inputElement.value.trim() && hasMatches &&
          this.selectedMatchIndex >= 0 &&
          this.selectedMatchIndex < this.result.matches.length) {
        // Restore the selection and update the input. Don't restore when the
        // user deletes all their input and autocomplete is queried or else the
        // empty input will change to the value of the first result.
        await this.getDropdownElement().selectIndex(this.selectedMatchIndex);
        this.getInputElement().setInput({
          text: this.selectedMatch!.fillIntoEdit,
          inline: '',
          moveCursorToEnd: true,
        });
      } else {
        // Remove the selection and update the input.
        this.getDropdownElement().unselect();
        this.getInputElement().setInput({
          inline: '',
        });
      }
    }

    private shouldSuppressDropdownForMultiline_(numMatches: number): boolean {
      const inputHasWrapped = this.initialInputScrollHeight > 0 &&
          this.getInputElement().scrollHeight > this.initialInputScrollHeight;
      return inputHasWrapped || numMatches === 1;
    }

    onInputFocusChanged(e: CustomEvent<{value: string}>) {
      if (this.dropdownIsVisible) {
        return;
      }
      this.queryAutocomplete(e.detail.value);
    }

    onSearchboxInputTextUpdated(
        e: CustomEvent<{value: string, isComposing: boolean}>,
        forceAutocomplete: boolean = false) {
      const inputValue = e.detail.value;
      if (inputValue.trim() || forceAutocomplete) {
        this.queryAutocomplete(inputValue, e.detail.isComposing);
      } else {
        this.clearAutocompleteMatches();
      }
    }

    onInputWrapperFocusout(e: FocusEvent) {
      const newlyFocusedEl = e.relatedTarget as Element;
      // Hide the matches and stop autocomplete only when the focus goes outside
      // of the searchbox wrapper. If focus is still in the searchbox wrapper,
      // exit early.
      if (this.getWrapperElement().contains(newlyFocusedEl)) {
        return;
      }

      if (this.lastQueriedInput === '') {
        // Clear the input as well as the matches if the input was empty when
        // the matches arrived.
        this.getInputElement().setInput({text: '', inline: ''});
        this.clearAutocompleteMatches();
      } else {
        this.dropdownIsVisible = false;

        // Stop autocomplete but leave (potentially stale) results and continue
        // listening for key presses. These stale results should never be shown.
        // They correspond to the potentially stale suggestion left in the
        // searchbox when blurred. That stale result may be navigated to by
        // focusing and pressing 'Enter'.
        this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
      }
      this.pageHandler().onFocusChanged(false);
    }

    async onInputWrapperKeydown(e: KeyboardEvent) {
      const modifier =
          isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
      if (modifier && e.key === 'z') {
        e.stopPropagation();
        return;
      }

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

      await this.handleKeyNavigation(e);
    }

    hasMatches(): boolean {
      return this.result !== null && !!this.result.matches &&
          this.result.matches.length > 0;
    }

    async handleKeyNavigation(e: KeyboardEvent) {
      if (e.key === 'Backspace' || e.key === 'Tab') {
        return;
      }

      // ArrowUp/ArrowDown query autocomplete when matches are not visible.
      if (!this.dropdownIsVisible) {
        if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
          if (this.multiLineEnabled &&
              this.shouldSuppressDropdownForMultiline_(
                  this.result?.matches?.length || 0)) {
            return;
          }
          const inputValue = this.getInputElement().inputElement.value;
          if (inputValue.trim() || !inputValue) {
            this.queryAutocomplete(inputValue);
          }
          e.preventDefault();
          return;
        }
      }

      if (e.key === 'Escape') {
        this.fire('escape-searchbox', {
          event: e,
          emptyInput: !this.getInputElement().inputElement.value,
        });
      }

      // Do not handle the following keys if there are no matches available.
      if (!this.result || this.result.matches.length === 0) {
        return;
      }

      if (e.key === 'Delete') {
        if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
          if (this.selectedMatch && this.selectedMatch.supportsDeletion) {
            this.pageHandler().deleteAutocompleteMatch(
                this.selectedMatchIndex, this.selectedMatch.destinationUrl);
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
        if (this.multiLineEnabled && e.shiftKey) {
          return;
        }
        e.preventDefault();
        const array: HTMLElement[] =
            [this.getDropdownElement(), this.getInputElement()];
        if (!array.includes(e.target as HTMLElement)) {
          return;
        }
        const currentInput = this.result?.input;
        const lastQueriedInput = this.lastQueriedInput?.trimStart();
        if (currentInput !== undefined && lastQueriedInput !== undefined &&
            lastQueriedInput === currentInput) {
          if (this.selectedMatch) {
            this.navigateToMatch(this.selectedMatchIndex, e);
          }
        } else {
          // User typed and pressed 'Enter' too quickly. Ignore this for now
          // because the matches are stale. Navigate to the default match (if
          // one exists) once the up-to-date matches arrive.
          this.lastIgnoredEnterEvent_ = e;
        }
        return;
      }

      // Do not handle the following keys if there are key modifiers.
      if (hasKeyModifiers(e)) {
        return;
      }

      // Clear the input as well as the matches when 'Escape' is pressed if the
      // the first match is selected or there are no selected matches.
      if (e.key === 'Escape' && this.selectedMatchIndex <= 0) {
        this.getInputElement().setInput({text: '', inline: ''});
        this.clearAutocompleteMatches();
        e.preventDefault();
        return;
      }

      e.preventDefault();

      if (e.key === 'ArrowDown') {
        await this.getDropdownElement().selectNext();
        this.pageHandler().onNavigationLikely(
            this.selectedMatchIndex, this.selectedMatch!.destinationUrl,
            NavigationPredictor.kUpOrDownArrowButton);
      } else if (e.key === 'ArrowUp') {
        await this.getDropdownElement().selectPrevious();
        this.pageHandler().onNavigationLikely(
            this.selectedMatchIndex, this.selectedMatch!.destinationUrl,
            NavigationPredictor.kUpOrDownArrowButton);
      } else if (e.key === 'Escape' || e.key === 'PageUp') {
        await this.getDropdownElement().selectFirst();
      } else if (e.key === 'PageDown') {
        await this.getDropdownElement().selectLast();
      }

      // Focus the selected match if focus is currently in the matches.
      if (this.shadowRoot.activeElement === this.getDropdownElement()) {
        this.getDropdownElement().focusSelected();
      }

      // Update the input.
      const newFill = this.selectedMatch!.fillIntoEdit;
      const newInline = this.selectedMatchIndex === 0 &&
              this.selectedMatch!.allowedToBeDefaultMatch ?
          this.selectedMatch!.inlineAutocompletion :
          '';
      const newFillEnd = newFill.length - newInline.length;
      const text = newFill.substr(0, newFillEnd);
      assert(text);
      this.getInputElement().setInput({
        text: text,
        inline: newInline,
        moveCursorToEnd: newInline.length === 0,
      });
    }

    onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>) {
      this.selectedMatchIndex = e.detail.value;
    }

    onMatchClick() {
      this.clearAutocompleteMatches();
    }

    async onMatchFocusin(e: CustomEvent<number>) {
      // Select the match that received focus.
      await this.getDropdownElement().selectIndex(e.detail);
      // Input selection (if any) likely drops due to focus change. Simply fill
      // the input with the match and move the cursor to the end.
      const input =
        this.shadowRoot.querySelector<SearchboxInputElement>('#input');
      assert(input);
      input.setInput({
        text: this.selectedMatch!.fillIntoEdit,
        inline: '',
        moveCursorToEnd: true,
      });
    }

    private computeSelectedMatch_() {
      if (!this.result || !this.result.matches) {
        return null;
      }
      return this.result.matches[this.selectedMatchIndex] || null;
    }

    private computeInputAriaLive_(): string {
      return this.selectedMatch ? 'off' : 'polite';
    }
  }

  return SearchboxMixin;
};

export interface SearchboxMixinInterface {
  composeboxSource: string;
  dropdownIsVisible: boolean;
  initialInputScrollHeight: number;
  inputAriaLive: string;
  lastQueriedInput: string|null;
  multiLineEnabled: boolean;
  result: AutocompleteResult|null;
  searchboxAriaDescription: string;
  selectedMatch: AutocompleteMatch|null;
  selectedMatchIndex: number;
  showThumbnail: boolean;

  clearAutocompleteMatches(): void;
  getDropdownElement(): SearchboxDropdownElement;
  getInputElement(): SearchboxInputElement;
  getWrapperElement(): HTMLElement;
  handleKeyNavigation(e: KeyboardEvent): void;
  hasMatches(): boolean;

  navigateToMatch(matchIndex: number, e: KeyboardEvent|MouseEvent): void;
  onAutocompleteResultChanged(result: AutocompleteResult|null): void;
  onInputFocusChanged(e: CustomEvent<{value: string}>): void;
  onInputWrapperFocusout(e: FocusEvent): void;
  onInputWrapperKeydown(e: KeyboardEvent): void;
  onMatchClick(): void;
  onMatchFocusin(e: CustomEvent<number>): void;
  onSearchboxInputTextUpdated(
      e: CustomEvent<{value: string, isComposing: boolean}>,
      forceAutocomplete?: boolean): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  pageHandler(): PageHandlerInterface;
  queryAutocomplete(input: string, preventInlineAutocomplete?: boolean): void;
}
