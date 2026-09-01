// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {SuggestInventory} from '//resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {NavigationPredictor} from '//resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, InputKeywordModel, OmniboxPopupSelection, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputMethod, SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {KeywordModeManager} from './keyword_mode_manager.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';
import {kDefaultSelection} from './searchbox_match.js';
import type {SearchboxSelectionMixinInterface} from './searchbox_selection_mixin.js';
import {SearchboxSelectionMixin, selectionsEqual} from './searchbox_selection_mixin.js';
import {mojoTimeTicks} from './utils.js';


/* @fileoverview Helper functions for implementing a custom searchbox. */

export enum ControlKeyState {
  UP,
  DOWN,
  DOWN_AND_CONSUMED,
}

type Constructor<T> = new (...args: any[]) => T;

export const SearchboxMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<SearchboxMixinInterface> => {
  class SearchboxMixin extends SearchboxSelectionMixin
  (superClass) implements SearchboxMixinInterface {
    accessor virtualFocusEnabled: boolean = false;

    static get properties() {
      return {
        virtualFocusEnabled: {
          type: Boolean,
        },
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

        inputKeywordModel: {
          type: Object,
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
    // Tracks the latest query sent for autocompletion. Used to filter out
    // stale results. `activeQueryId` is reset to -1 when the last query needs
    // to be abandoned. `nextQueryId_` is monotonically increasing to avoid
    // reusing IDs.
    activeQueryId: number = -1;
    private nextQueryId_: number = 0;
    accessor lastQueriedInput: string|null = null;
    accessor multiLineEnabled: boolean = false;
    accessor result: AutocompleteResult|null = null;
    accessor selectedMatch: AutocompleteMatch|null = null;
    accessor selectedMatchIndex: number = -1;

    get matchIndex(): number {
      if (this.virtualFocusEnabled) {
        if (this.selection.line >= 0) {
          return this.selection.line;
        }
        return (this.result?.matches?.[0]?.allowedToBeDefaultMatch) ? 0 : -1;
      }
      if (this.selectedMatchIndex >= 0) {
        return this.selectedMatchIndex;
      }
      return (this.result?.matches?.[0]?.allowedToBeDefaultMatch) ? 0 : -1;
    }
    accessor inputAriaLive: string = '';
    accessor searchboxIcon: string = '';
    accessor showThumbnail: boolean = false;

    private keywordModeManager_: KeywordModeManager = new KeywordModeManager({
      onKeywordModelChanged: () => {
        this.requestUpdate('inputKeywordModel');
      },
      onKeywordCleared:
          (event) => {
            this.getInputElement().setInput({
              text: event.restoredText,
              inline: '',
              moveCursorToEnd: false,
            });
            this.getInputElement().inputElement?.setSelectionRange(
                event.cursorPosition, event.cursorPosition);
            this.queryAutocomplete(
                event.restoredText, /*preventInlineAutocomplete=*/ true,
                /*isOnFocus=*/ false);
          },
      onKeywordEntered:
          () => {
            this.getInputElement().setInputText('');
          },
    });

    get inputKeywordModel(): InputKeywordModel|null {
      return this.keywordModeManager_.inputKeywordModel;
    }

    set inputKeywordModel(model: InputKeywordModel|null) {
      this.keywordModeManager_.inputKeywordModel = model;
    }

    get keywordModeManager(): KeywordModeManager {
      return this.keywordModeManager_;
    }

    initialInputScrollHeight: number = 0;

    private controlKeyState_: ControlKeyState = ControlKeyState.UP;
    private lastIgnoredEnterEvent_: KeyboardEvent|null = null;
    private searchboxEventTracker_: EventTracker = new EventTracker();

    override connectedCallback() {
      super.connectedCallback();

      // On user interaction, freeze the current results to avoid result updates
      // potentially erasing user changes like cursor position.
      this.searchboxEventTracker_.add(this, 'input-mousedown', () => {
        this.activeQueryId = -1;
      });
      // When deleting a match, unfreeze `activeQueryId` so post-deletion
      // results are accepted.
      this.searchboxEventTracker_.add(this, 'match-remove', () => {
        this.activeQueryId = this.nextQueryId_ - 1;
      });
      // Listen for 'keyup' on window to reliably catch Control key releases
      // even if the user clicks outside the searchbox while holding Control.
      this.searchboxEventTracker_.add(window, 'keyup', (e: Event) => {
        if (this.shouldAppendDotComOnCtrlEnter() &&
            (e as KeyboardEvent).key === 'Control') {
          this.controlKeyState_ = ControlKeyState.UP;
        }
      });
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      this.searchboxEventTracker_.removeAll();
    }

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      const changedPrivateProperties =
          changedProperties as Map<PropertyKey, unknown>;
      if (changedPrivateProperties.has('selectedMatch')) {
        this.inputAriaLive = this.computeInputAriaLive_();
      }
      if (changedPrivateProperties.has('result') ||
          changedPrivateProperties.has('selectedMatchIndex') ||
          changedPrivateProperties.has('selection')) {
        this.selectedMatch = this.computeSelectedMatch_();
      }
      if (changedPrivateProperties.has('result') ||
          changedPrivateProperties.has('selectedMatchIndex') ||
          changedPrivateProperties.has('selectedMatch') ||
          changedPrivateProperties.has('selection')) {
        this.keywordModeManager_.onSelectedMatchChanged(
            this.selectedMatch, this.selection);
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

    getTabId(): number|null {
      return null;
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
      // by setting `activeQueryId` to -1.
      this.activeQueryId = -1;
      this.lastQueriedInput = null;
    }

    queryAutocomplete(
        input: string, preventInlineAutocomplete: boolean, isOnFocus: boolean) {
      this.activeQueryId = this.nextQueryId_++;
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
      const keyword = this.keywordModeManager_.activeKeyword;
      this.pageHandler().queryAutocomplete(
          this.activeQueryId, this.getTabId(), input, preventInlineAutocomplete,
          cursorPosition, SuggestInventory.kDefault, isOnFocus, keyword,
          InputMethod.kKeyboard);

      this.dispatchEvent(new CustomEvent('query-autocomplete', {
        bubbles: true,
        composed: true,
        detail: {inputValue: input},
      }));
    }

    shouldAppendDotComOnCtrlEnter(): boolean {
      return false;
    }

    isBackgroundTabNavigation(_e: KeyboardEvent|MouseEvent): boolean {
      return false;
    }

    navigateToMatch(matchIndex: number, e: KeyboardEvent|MouseEvent) {
      assert(matchIndex >= 0);
      const match = this.result!.matches[matchIndex];
      assert(match);
      this.pageHandler().openAutocompleteMatch(
          matchIndex, match.destinationUrl,
          /*areMatchesShowing=*/ this.dropdownIsVisible,
          /*mouseButton=*/ (e as MouseEvent).button || 0, {
            altKey: e.altKey,
            ctrlKey: e.ctrlKey,
            metaKey: e.metaKey,
            shiftKey: e.shiftKey,
          },
          /*viaKeyboard=*/ e instanceof KeyboardEvent);

      const isBackgroundTab = this.isBackgroundTabNavigation(e);
      if (!isBackgroundTab) {
        const fillText = this.keywordModeManager_.formatMatchFillIntoEdit(
            match, matchIndex, this.lastQueriedInput);
        this.getInputElement().setInput({
          text: fillText,
          inline: '',
          moveCursorToEnd: true,
        });
        this.clearAutocompleteMatches();
      }
      e.preventDefault();
    }

    openCtrlEnterMatch(matchIndex: number) {
      assert(matchIndex >= 0);
      const match = this.result!.matches[matchIndex];
      assert(match);
      this.pageHandler().openPopupSelection(
          this.result!.sequenceId, {
            line: matchIndex,
            state: SelectionLineState.kCtrlEnter,
            actionIndex: 0,
          },
          1);
      this.getInputElement().setInput({
        text: match.fillIntoEdit,
        inline: '',
        moveCursorToEnd: true,
      });
      this.clearAutocompleteMatches();
    }

    isAutocompleteResultStale(result: AutocompleteResult): boolean {
      return result.queryId !== this.activeQueryId;
    }

    updateDropdownVisibility(): void {
      this.dropdownIsVisible = this.hasMatches();
    }

    async onAutocompleteResultChanged(result: AutocompleteResult) {
      if (this.isAutocompleteResultStale(result)) {
        return;
      }

      this.result = result;
      const hasMatches = this.hasMatches();
      this.updateDropdownVisibility();

      const firstMatch = hasMatches ? this.result.matches[0] : null;
      if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
        // Select the default match and update the input.
        if (this.virtualFocusEnabled) {
          const available = this.getAvailableSelections(this.result);
          this.setSelection(available[0] || kDefaultSelection);
        } else {
          this.getDropdownElement().selectFirst();
        }
        this.getInputElement().setInput({
          text: this.lastQueriedInput ?? '',
          inline: firstMatch.inlineAutocompletion,
        });

        // Navigate to the default up-to-date match if the user typed and
        // pressed 'Enter' too fast.
        if (this.lastIgnoredEnterEvent_) {
          this.navigateToMatch(0, this.lastIgnoredEnterEvent_);
          this.lastIgnoredEnterEvent_ = null;
        }

      } else {
        const index = this.matchIndex;
        if (this.getInputElement().inputElement.value.trim() && hasMatches &&
            index >= 0 && index < this.result.matches.length) {
          const match = this.result.matches[index]!;
          this.selectedMatch = match;
          if (this.virtualFocusEnabled) {
            this.setSelection({
              line: index,
              state: SelectionLineState.kNormal,
              actionIndex: 0,
            });
          }
          // Restore the selection and update the input. Don't restore when the
          // user deletes all their input and autocomplete is queried or else
          // the empty input will change to the value of the first result.
          await this.getDropdownElement().selectIndex(index);
          this.getInputElement().setInput({
            text: this.computeMatchFillIntoEdit(match),
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
    }

    onInputFocusChanged(e: CustomEvent<{value: string, isOnFocus: boolean}>) {
      if (this.shouldAppendDotComOnCtrlEnter()) {
        this.controlKeyState_ = ControlKeyState.UP;
      }
      if (this.dropdownIsVisible) {
        return;
      }
      const input = e.detail.value;
      const isOnFocus = e.detail.isOnFocus;
      this.queryAutocomplete(
          input, /*preventInlineAutocomplete=*/ false, isOnFocus);
    }

    onSearchboxInputTextUpdated(
        e: CustomEvent<{value: string, isComposing: boolean}>) {
      const input = e.detail.value;
      const cursorPosition =
          this.getInputElement().inputElement?.selectionStart ?? null;

      if (this.keywordModeManager_.acceptInputTrigger(input, cursorPosition)) {
        this.getInputElement().setInputText('');
        this.queryAutocomplete(
            '', /*preventInlineAutocomplete=*/ false, /*isOnFocus=*/ false);
        return;
      }

      const isEmpty =
          !input.trim() && !this.keywordModeManager_.isInKeywordMode;
      if (isEmpty) {
        this.clearAutocompleteMatches();
      } else {
        this.queryAutocomplete(
            input, /*preventInlineAutocomplete=*/ e.detail.isComposing,
            /*isOnFocus=*/ false);
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

      if (this.lastQueriedInput === '' &&
          !this.keywordModeManager_.isInKeywordMode) {
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
        // focusing and pressing 'Enter'. Reset `activeQueryId` to prevent an
        // in-flight result from re-opening the popup.
        this.activeQueryId = -1;
        this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
      }
      this.pageHandler().onFocusChanged(false);
    }

    async onInputWrapperKeydown(e: KeyboardEvent) {
      // On user interaction, freeze the current results to avoid result updates
      // potentially erasing user changes like cursor position. Freezing on
      // 'enter' would break the fast-enter navigations via
      // `lastIgnoredEnterEvent_`. It'd cause the searchbox to discard the
      // pending results the navigation is waiting for, causing the navigation
      // to never occur. But this is a hack; comparing `e.key !=== 'Enter'` is
      // only a semi-accurate heuristic for whether a navigation is about to
      // occur.
      if (e.key !== 'Enter') {
        this.activeQueryId = -1;
      }
      const modifier =
          isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
      if (modifier && e.key === 'z') {
        e.stopPropagation();
        return;
      }

      if (this.shouldAppendDotComOnCtrlEnter()) {
        if (e.key === 'Control') {
          if (this.controlKeyState_ === ControlKeyState.UP) {
            this.controlKeyState_ = ControlKeyState.DOWN;
          }
        } else if (e.ctrlKey && e.key !== 'Enter') {
          if (this.controlKeyState_ === ControlKeyState.DOWN) {
            this.controlKeyState_ = ControlKeyState.DOWN_AND_CONSUMED;
          }
        }
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
      return !!this.result && !!this.result.matches &&
          this.result.matches.length > 0;
    }

    /**
     * Determines whether the key event originated from an element participating
     * in virtual focus navigation (the input, dropdown matches, or compose
     * button). Events from nested controls (e.g. contextual entrypoints, lens
     * button, voice search) return false so native browser Tab navigation
     * applies.
     */
    private isVirtualFocusEventTarget_(e: KeyboardEvent): boolean {
      const path = e.composedPath();
      if (path.length === 0) {
        return true;
      }
      return path.includes(this.getInputElement()) ||
          path.includes(this.getDropdownElement()) || path.some(el => {
            const node = el as HTMLElement;
            return node.tagName === 'CR-SEARCHBOX-COMPOSE-BUTTON';
          });
    }

    /**
     * Handles Enter key presses on virtually focused elements (AIM, Action,
     * Remove Suggestion). Returns true if the event was handled.
     */
    private handleVirtualFocusEnter_(e: KeyboardEvent): boolean {
      if (this.selection.state === SelectionLineState.kFocusedButtonAim) {
        e.preventDefault();
        const button =
            this.shadowRoot.querySelector('cr-searchbox-compose-button');
        if (button) {
          button.dispatchEvent(new CustomEvent('compose-click', {
            bubbles: true,
            composed: true,
            detail: {
              button: 0,
              ctrlKey: e.ctrlKey,
              metaKey: e.metaKey,
              shiftKey: e.shiftKey,
            },
          }));
        }
        return true;
      }

      if (this.selection.state === SelectionLineState.kFocusedButtonAction) {
        e.preventDefault();
        const action = this.selectedMatch?.actions[this.selection.actionIndex];
        if (action) {
          this.pageHandler().executeAction(
              this.selection.line, this.selection.actionIndex,
              this.selectedMatch!.destinationUrl, mojoTimeTicks(Date.now()), 0,
              e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
        }
        return true;
      }

      if (this.selection.state ===
          SelectionLineState.kFocusedButtonRemoveSuggestion) {
        e.preventDefault();
        if (this.selectedMatch && this.selectedMatch.supportsDeletion) {
          this.unfreezeActiveQueryId();
          this.pageHandler().deleteAutocompleteMatch(
              this.selection.line, this.selectedMatch.destinationUrl);
        }
        return true;
      }

      if (this.selection.state === SelectionLineState.kKeywordMode) {
        e.preventDefault();
        this.getInputElement().focus();
        return true;
      }

      return false;
    }

    private updateInputForSelection_(
        nextSelection: OmniboxPopupSelection, key: string) {
      if (this.selectedMatch) {
        const newFill = this.computeMatchFillIntoEdit(this.selectedMatch);
        const isKeywordMode = this.keywordModeManager_.isInKeywordMode ||
            nextSelection.state === SelectionLineState.kKeywordMode;
        const newInline = !isKeywordMode && nextSelection.line === 0 &&
                this.selectedMatch.allowedToBeDefaultMatch ?
            this.selectedMatch.inlineAutocompletion :
            '';
        const newFillEnd = newFill.length - newInline.length;
        const text = newFill.substr(0, newFillEnd);
        this.getInputElement().setInput({
          text: text,
          inline: newInline,
          moveCursorToEnd: newInline.length === 0,
        });

        if (key === 'ArrowDown' || key === 'ArrowUp') {
          this.pageHandler().onNavigationLikely(
              nextSelection.line, this.selectedMatch.destinationUrl,
              NavigationPredictor.kUpOrDownArrowButton);
        }
      } else if (nextSelection.line === -1) {
        this.getInputElement().setInput({
          text: this.lastQueriedInput ?? '',
          inline: '',
          moveCursorToEnd: true,
        });
      }
    }

    private handleEnterNavigation_(e: KeyboardEvent) {
      if (this.multiLineEnabled && e.shiftKey) {
        return;
      }

      const isPureCtrlEnter = this.shouldAppendDotComOnCtrlEnter() &&
          e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey &&
          this.controlKeyState_ !== ControlKeyState.DOWN_AND_CONSUMED;

      e.preventDefault();
      if (this.handleVirtualFocusEnter_(e)) {
        return;
      }
      // If no new query's `results` are pending (though new async results for
      // the current query may be pending), navigate. Otherwise, the user
      // pressed enter after sending a new query that hasn't returned any
      // results yet. Wait for the 1st results of the new query before
      // navigating.
      if (this.activeQueryId === -1 ||
          this.result?.queryId === this.activeQueryId) {
        if (this.selectedMatch) {
          if (isPureCtrlEnter) {
            this.openCtrlEnterMatch(this.matchIndex);
          } else {
            this.navigateToMatch(this.matchIndex, e);
          }
        }
      } else {
        // User typed and pressed 'Enter' too quickly. Ignore this for now
        // because the matches are stale. Navigate to the default match (if
        // one exists) once the up-to-date matches arrive.
        this.lastIgnoredEnterEvent_ = e;
        // Unfreeze `activeQueryId` so pending query results are accepted.
        this.activeQueryId = this.nextQueryId_ - 1;
      }
    }

    async handleKeyNavigation(e: KeyboardEvent) {
      if (e.key === 'Backspace') {
        const inputEl = this.getInputElement().inputElement;
        if (inputEl && this.keywordModeManager_.handleBackspace(inputEl)) {
          e.preventDefault();
        }
        return;
      }

      if (e.key === 'Tab') {
        if (!this.virtualFocusEnabled && !e.shiftKey && !e.isComposing &&
            this.keywordModeManager_.acceptTab(
                this.selectedMatch, this.matchIndex)) {
          e.preventDefault();
          return;
        }

        if (!this.virtualFocusEnabled || !this.isVirtualFocusEventTarget_(e)) {
          return;
        }
      }

      // ArrowUp/ArrowDown query autocomplete when matches are not
      // visible.
      if (!this.dropdownIsVisible) {
        if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
          const inputValue = this.getInputElement().inputElement.value;
          if (inputValue.trim() || !inputValue) {
            this.queryAutocomplete(
                inputValue, /*preventInlineAutocomplete=*/ false,
                /*isOnFocus=*/ !inputValue);
          }
          e.preventDefault();
          return;
        }
      }

      if (e.key === 'Escape') {
        this.dispatchEvent(new CustomEvent('escape-searchbox', {
          bubbles: true,
          composed: true,
          detail: {
            event: e,
            emptyInput: !this.getInputElement().inputElement.value,
          },
        }));
      }

      // Do not handle the following keys if there are no matches available.
      if (!this.result || this.result.matches.length === 0) {
        return;
      }

      if (e.key === 'Delete') {
        if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
          if (this.selectedMatch && this.selectedMatch.supportsDeletion) {
            // Unfreeze `activeQueryId` so post-deletion results are accepted.
            this.activeQueryId = this.nextQueryId_ - 1;
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

      if (this.virtualFocusEnabled) {
        if (e.key === 'Enter' && this.handleVirtualFocusEnter_(e)) {
          return;
        }

        let step = SelectionStep.kStateOrLine;
        let direction = SelectionDirection.kForward;
        let valid = false;

        if (!e.altKey && !e.ctrlKey && !e.metaKey) {
          if (e.key === 'Tab') {
            step = SelectionStep.kStateOrLine;
            direction = e.shiftKey ? SelectionDirection.kBackward :
                                     SelectionDirection.kForward;
            valid = true;
          } else if (!e.shiftKey) {
            if (e.key === 'ArrowDown') {
              step = SelectionStep.kWholeLine;
              direction = SelectionDirection.kForward;
              valid = true;
            } else if (e.key === 'ArrowUp') {
              step = SelectionStep.kWholeLine;
              direction = SelectionDirection.kBackward;
              valid = true;
            } else if (e.key === 'PageDown') {
              step = SelectionStep.kAllLines;
              direction = SelectionDirection.kForward;
              valid = true;
            } else if (e.key === 'PageUp' || e.key === 'Escape') {
              step = SelectionStep.kAllLines;
              direction = SelectionDirection.kBackward;
              valid = true;
            }
          }
        }

        if (valid) {
          if (e.key === 'Tab') {
            if (this.stepCyclesSelection(
                    this.result, this.selection, direction, step)) {
              this.setSelection(kDefaultSelection);
              // Do not preventDefault, allow native browser focus to move.
              return;
            }
          }

          const nextSelection = this.getNextSelection(
              this.result, this.selection, direction, step);

          if (selectionsEqual(nextSelection, this.selection)) {
            if (e.key === 'Escape') {
              this.getInputElement().setInput({text: '', inline: ''});
              this.clearAutocompleteMatches();
              e.preventDefault();
            }
            return;
          }

          e.preventDefault();
          this.setSelection(nextSelection);

          this.getInputElement().focus();

          await this.updateComplete;

          this.updateInputForSelection_(nextSelection, e.key);
          return;
        }
      }

      if (e.key === 'Enter') {
        this.handleEnterNavigation_(e);
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
      // Legacy fallback for Arrow keys and Tab. (Tab does nothing).
      e.preventDefault();

      if (e.key === 'ArrowDown') {
        await this.getDropdownElement().selectNext();
      } else if (e.key === 'ArrowUp') {
        await this.getDropdownElement().selectPrevious();
      } else if (e.key === 'Escape' || e.key === 'PageUp') {
        await this.getDropdownElement().selectFirst();
      } else if (e.key === 'PageDown') {
        await this.getDropdownElement().selectLast();
      }

      await this.updateComplete;

      if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        if (this.selectedMatch) {
          this.pageHandler().onNavigationLikely(
              this.selectedMatchIndex, this.selectedMatch.destinationUrl,
              NavigationPredictor.kUpOrDownArrowButton);
        }
      }

      // Focus the selected match if focus is currently in the matches.
      if (this.shadowRoot.activeElement === this.getDropdownElement()) {
        this.getDropdownElement().focusSelected();
      }

      // Update the input.
      if (this.selectedMatch) {
        const newFill = this.computeMatchFillIntoEdit(this.selectedMatch);
        const newInline = this.selectedMatchIndex === 0 &&
                this.selectedMatch.allowedToBeDefaultMatch ?
            this.selectedMatch.inlineAutocompletion :
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
      const input = this.getInputElement();
      assert(input);
      if (this.selectedMatch) {
        input.setInput({
          text: this.computeMatchFillIntoEdit(this.selectedMatch),
          inline: '',
          moveCursorToEnd: true,
        });
      }
    }

    computeMatchFillIntoEdit(match: AutocompleteMatch): string {
      return this.keywordModeManager_.formatMatchFillIntoEdit(
          match, this.matchIndex, this.lastQueriedInput);
    }

    async onKeywordClick(e: Event) {
      const detail =
          (e as CustomEvent<{match?: AutocompleteMatch, matchIndex?: number}>)
              .detail;
      const match = detail.match;
      assert(match?.keywordModel);
      this.keywordModeManager_.handleKeywordClick(match);
      const matchIndex = detail.matchIndex ??
          (this.result?.matches ? this.result.matches.indexOf(match) : 0);
      const selection: OmniboxPopupSelection = {
        line: matchIndex >= 0 ? matchIndex : 0,
        state: SelectionLineState.kKeywordMode,
        actionIndex: 0,
      };
      this.setSelection(selection);
      await this.updateComplete;
      this.updateInputForSelection_(selection, 'click');
      this.getInputElement().focus();
    }

    private computeSelectedMatch_() {
      if (!this.result || !this.result.matches) {
        return null;
      }
      return this.result.matches[this.matchIndex] || null;
    }

    private computeInputAriaLive_(): string {
      return this.selectedMatch ? 'off' : 'polite';
    }

    /**
     * Accepts the inline autocompletion by appending it to the input text and
     * moving the cursor to the end. Returns `true` if inline autocomplete was
     * handled, `false` otherwise.
     */
    acceptInlineAutocomplete(e: KeyboardEvent): boolean {
      const input = this.getInputElement();
      const lastInput = input?.lastInput();
      if (!lastInput?.inline) {
        return false;
      }

      if (e.shiftKey) {
        input.setInput({inline: ''});
        return true;
      }

      const newText = lastInput.text + lastInput.inline;
      input.setInput({
        text: newText,
        inline: '',
        moveCursorToEnd: true,
      });
      this.queryAutocomplete(
          newText, /*preventInlineAutocomplete=*/ false, /*isOnFocus=*/ false);
      e.preventDefault();
      return true;
    }

    unfreezeActiveQueryId() {
      this.activeQueryId = this.nextQueryId_ - 1;
    }
  }

  return SearchboxMixin;
};

export interface SearchboxMixinInterface extends
    SearchboxSelectionMixinInterface {
  virtualFocusEnabled: boolean;
  matchIndex: number;
  composeboxSource: string;
  dropdownIsVisible: boolean;
  initialInputScrollHeight: number;
  inputAriaLive: string;
  activeQueryId: number;
  lastQueriedInput: string|null;
  multiLineEnabled: boolean;
  result: AutocompleteResult|null;
  searchboxAriaDescription: string;
  selectedMatch: AutocompleteMatch|null;
  selectedMatchIndex: number;
  inputKeywordModel: InputKeywordModel|null;
  keywordModeManager: KeywordModeManager;
  showThumbnail: boolean;

  acceptInlineAutocomplete(e: KeyboardEvent): boolean;
  clearAutocompleteMatches(): void;
  computeMatchFillIntoEdit(match: AutocompleteMatch): string;
  getDropdownElement(): SearchboxDropdownElement;
  getInputElement(): SearchboxInputElement;
  getWrapperElement(): HTMLElement;
  handleKeyNavigation(e: KeyboardEvent): void;
  hasMatches(): boolean;
  isAutocompleteResultStale(result: AutocompleteResult): boolean;
  isBackgroundTabNavigation(e: KeyboardEvent|MouseEvent): boolean;
  updateDropdownVisibility(): void;
  unfreezeActiveQueryId(): void;

  navigateToMatch(matchIndex: number, e: KeyboardEvent|MouseEvent): void;
  onAutocompleteResultChanged(result: AutocompleteResult|null): void;
  onInputFocusChanged(e: CustomEvent<{value: string}>): void;
  onInputWrapperFocusout(e: FocusEvent): void;
  onInputWrapperKeydown(e: KeyboardEvent): void;
  onMatchClick(): void;
  onMatchFocusin(e: CustomEvent<number>): void;
  onKeywordClick(e: Event): void;
  openCtrlEnterMatch(matchIndex: number): void;
  onSearchboxInputTextUpdated(
      e: CustomEvent<{value: string, isComposing: boolean}>): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  pageHandler(): PageHandlerInterface;
  queryAutocomplete(
      input: string, preventInlineAutocomplete: boolean,
      isOnFocus: boolean): void;
  getTabId(): number|null;
  shouldAppendDotComOnCtrlEnter(): boolean;
}
