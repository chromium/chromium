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
import type {AutocompleteMatch, AutocompleteResult, InputKeywordModel, OmniboxPopupSelection, PageCallbackRouter, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputMethod, KeywordActivationMethod, SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {KeywordModeEntryMethod, KeywordModeManager} from './keyword_mode_manager.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
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
    static get properties() {
      return {
        virtualFocusEnabled: {
          type: Boolean,
        },
        dropdownIsVisible: {
          type: Boolean,
          reflect: true,
        },

        multiLineEnabled: {
          type: Boolean,
          reflect: true,
        },

        singleLineOnInlineAutocomplete: {
          type: Boolean,
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

        inputIsEmpty_: {
          type: Boolean,
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
    accessor virtualFocusEnabled: boolean = false;
    accessor searchboxAriaDescription: string = '';
    accessor dropdownIsVisible: boolean = false;
    // Tracks the latest query sent for autocompletion. Used to filter out
    // stale results. `activeQueryId` is reset to -1 when the last query needs
    // to be abandoned. `nextQueryId_` is monotonically increasing to avoid
    // reusing IDs.
    activeQueryId: number = -1;
    private nextQueryId_: number = 0;
    lastQueriedInput: string|null = null;
    accessor multiLineEnabled: boolean = false;
    accessor singleLineOnInlineAutocomplete: boolean = false;
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
    accessor inputKeywordModel: InputKeywordModel|null = null;
    private accessor inputIsEmpty_: boolean = true;
    accessor searchboxIcon: string = '';
    accessor showThumbnail: boolean = false;

    private keywordModeManager_: KeywordModeManager =
        new KeywordModeManager({
          onKeywordModelChanged: () => {
            this.inputKeywordModel = this.keywordModeManager_.inputKeywordModel;
          },
          onKeywordCleared: (event) => {
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
          onKeywordEntered: () => {
            this.getInputElement().setInputText('');
          },
        });

    get keywordModeManager(): KeywordModeManager {
      return this.keywordModeManager_;
    }

    override get inKeywordMode(): boolean {
      return this.keywordModeManager_.isInKeywordMode;
    }

    initialInputScrollHeight: number = 0;

    // Tracks the Ctrl key across a keydown sequence so that a Ctrl press
    // already consumed by another shortcut (e.g. Ctrl+V) does not also append
    // the desired TLD on Ctrl+Enter. Mirrors `OmniboxEditModel`'s
    // `control_key_state_`.
    controlKeyState: ControlKeyState = ControlKeyState.UP;

    private lastIgnoredEnterEvent_: KeyboardEvent|null = null;
    // The line of the match the backend was last asked to delete, while
    // waiting for the post-deletion result. Null if no match was selected at
    // the time. See `shouldPreserveSelectionOnMatchDeletion()`.
    private pendingDeletionLine_: number|null = null;
    // Set when the user deletes a suggestion, so that the results replacing
    // the current ones narrate the new selection. See
    // `onAutocompleteResultChanged()`.
    private narrateNextResultSelection_: boolean = false;
    private searchboxEventTracker_: EventTracker = new EventTracker();
    private callbackRouter_: PageCallbackRouter =
        SearchboxBrowserProxy.getInstance().callbackRouter;
    private keywordSpaceTriggeringListenerId_: number|null = null;
    private availableKeywordModelsListenerId_: number|null = null;

    override connectedCallback() {
      super.connectedCallback();

      this.keywordSpaceTriggeringListenerId_ =
          this.callbackRouter_.setKeywordSpaceTriggeringEnabled.addListener(
              (enabled: boolean) => {
                this.keywordModeManager_.keywordSpaceTriggeringEnabled =
                    enabled;
              });

      this.availableKeywordModelsListenerId_ =
          this.callbackRouter_.setAvailableKeywordModels.addListener(
              (models: InputKeywordModel[],
               defaultSearchProviderKeyword: string) => {
                this.keywordModeManager_.availableKeywordModels = models;
                this.keywordModeManager_.defaultSearchEngineKeyword =
                    defaultSearchProviderKeyword;
              });

      // On user interaction, freeze the current results to avoid result updates
      // potentially erasing user changes like cursor position.
      this.searchboxEventTracker_.add(this, 'input-mousedown', () => {
        this.activeQueryId = -1;
      });
      this.searchboxEventTracker_.add(
          this.getInputElement(), 'searchbox-input-empty-changed',
          (e: Event) => {
            this.inputIsEmpty_ =
                (e as CustomEvent<{value: boolean}>).detail.value;
          });
      // When deleting a match, unfreeze `activeQueryId` so post-deletion
      // results are accepted. The match element sends the deletion request
      // itself, and its line is the event's detail.
      this.searchboxEventTracker_.add(this, 'match-remove', (e: Event) => {
        // Like `OmniboxEditModel::TryDeletingPopupLine()`, only move the
        // selection to the deleted line if a match was selected.
        const hasSelectedMatch = this.virtualFocusEnabled ?
            this.selection.line >= 0 :
            this.selectedMatchIndex >= 0;
        this.onMatchDeletionRequested_(
            hasSelectedMatch ? (e as CustomEvent<number|null>).detail : null);
      });
      // Listen for 'keyup' on window to reliably catch Control key releases
      // even if the user clicks outside the searchbox while holding Control.
      this.searchboxEventTracker_.add(window, 'keyup', (e: Event) => {
        if (this.shouldAppendDotComOnCtrlEnter() &&
            (e as KeyboardEvent).key === 'Control') {
          this.controlKeyState = ControlKeyState.UP;
        }
      });
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      this.searchboxEventTracker_.removeAll();
      if (this.keywordSpaceTriggeringListenerId_ !== null) {
        this.callbackRouter_.removeListener(
            this.keywordSpaceTriggeringListenerId_);
        this.keywordSpaceTriggeringListenerId_ = null;
      }
      if (this.availableKeywordModelsListenerId_ !== null) {
        this.callbackRouter_.removeListener(
            this.availableKeywordModelsListenerId_);
        this.availableKeywordModelsListenerId_ = null;
      }
    }

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      const changedPrivateProperties =
          changedProperties as Map<PropertyKey, unknown>;
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
      this.pendingDeletionLine_ = null;
      this.narrateNextResultSelection_ = false;
    }

    queryAutocomplete(
        input: string, preventInlineAutocomplete: boolean, isOnFocus: boolean) {
      this.activeQueryId = this.nextQueryId_++;
      this.lastQueriedInput = input;
      // The results of a new query are not the ones left by a deletion.
      this.pendingDeletionLine_ = null;
      this.narrateNextResultSelection_ = false;

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

      this.fire('query-autocomplete', {inputValue: input});
    }

    shouldAppendDotComOnCtrlEnter(): boolean {
      return false;
    }

    /**
     * Whether the post-deletion result should keep the selection on the
     * deleted match's line, clamped to the remaining matches, like
     * `OmniboxEditModel::TryDeletingPopupLine()`. Otherwise, it's handled like
     * any other result, which e.g. reselects the default match, if any.
     */
    shouldPreserveSelectionOnMatchDeletion(): boolean {
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
          this.result!.sequenceId, matchIndex, match.destinationUrl,
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
        this.keywordModeManager_.exit();
        this.clearAutocompleteMatches();
      }
      e.preventDefault();
    }

    openCtrlEnterMatch(matchIndex: number) {
      const match = matchIndex >= 0 && this.result?.matches ?
          this.result.matches[matchIndex] :
          null;
      this.pageHandler().openPopupSelection(
          this.result?.sequenceId ?? 0, {
            line: matchIndex,
            state: SelectionLineState.kCtrlEnter,
            actionIndex: 0,
          },
          1);
      if (match) {
        this.getInputElement().setInput({
          text: match.fillIntoEdit,
          inline: '',
          moveCursorToEnd: true,
        });
      }
      this.keywordModeManager_.exit();
      this.clearAutocompleteMatches();
    }

    openContextMenu(): void {
      // Overridden by embedders with contextual entrypoints (e.g. Full WebUI
      // popup).
    }

    isAutocompleteResultStale(result: AutocompleteResult): boolean {
      return result.queryId !== this.activeQueryId;
    }

    shouldDropdownBeVisible(): boolean {
      return this.hasMatches();
    }

    updateDropdownVisibility(): void {
      this.dropdownIsVisible = this.shouldDropdownBeVisible();

      if (this.multiLineEnabled && this.dropdownIsVisible) {
        const isUserTyping = (this.result?.input.length ?? 0) > 0;
        if (isUserTyping && this.getInputElement()?.isMultiline()) {
          this.dropdownIsVisible = false;
        }
      }
    }

    async onAutocompleteResultChanged(result: AutocompleteResult) {
      if (this.isAutocompleteResultStale(result)) {
        return;
      }

      // The first result accepted after a deletion request is normally the
      // post-deletion result.
      const deletedLine = this.pendingDeletionLine_;
      this.pendingDeletionLine_ = null;

      this.result = result;
      const hasMatches = this.hasMatches();
      this.updateDropdownVisibility();

      const lineAfterDeletion = this.getLineAfterMatchDeletion_(deletedLine);
      const firstMatch = hasMatches ? this.result.matches[0] : null;
      if (lineAfterDeletion !== null) {
        await this.selectLineAfterMatchDeletion_(lineAfterDeletion);
      } else if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
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
        const index = hasMatches && this.matchIndex >= 0 ?
            Math.min(this.matchIndex, this.result.matches.length - 1) :
            -1;
        if (this.getInputElement().inputElement.value.trim() && index >= 0) {
          // Restore the selection and update the input. Don't restore when the
          // user deletes all their input and autocomplete is queried or else
          // the empty input will change to the value of the first result.
          const selection: OmniboxPopupSelection = {
            line: index,
            state: SelectionLineState.kNormal,
            actionIndex: 0,
          };
          if (this.virtualFocusEnabled) {
            this.setSelection(selection);
          }
          await this.getDropdownElement().selectIndex(index);
          await this.updateComplete;
          this.fillInputForSelection(selection);
        } else {
          // Remove the selection and update the input.
          this.getDropdownElement().unselect();
          this.getInputElement().setInput({
            inline: '',
          });
        }
      }

      // The results replaced the ones a suggestion was deleted from, so tell
      // the user what is selected now, as the native omnibox does. Typing and
      // other result updates are not narrated; screen readers echo the input.
      if (this.narrateNextResultSelection_) {
        this.narrateNextResultSelection_ = false;
        await this.updateComplete;
        // Narrating no selection clears the deleted match's label.
        this.narrateSelection(
            this.dropdownIsVisible ? this.selection : kDefaultSelection);
      }
    }

    /**
     * Returns the line to select in the post-deletion result, or null to
     * handle it like any other result. Like
     * `OmniboxEditModel::TryDeletingPopupLine()`, the selection stays on the
     * deleted match's line, clamped to the remaining matches.
     */
    private getLineAfterMatchDeletion_(deletedLine: number|null): number|null {
      if (deletedLine === null || deletedLine < 0 || !this.hasMatches() ||
          !this.shouldPreserveSelectionOnMatchDeletion()) {
        return null;
      }
      const matches = this.result!.matches;
      const line = Math.min(deletedLine, matches.length - 1);
      // Selecting the default match like for any other result also restores
      // its inline autocompletion.
      return line === 0 && matches[0]!.allowedToBeDefaultMatch ? null : line;
    }

    /**
     * Selects the match at `line` and previews it in the input, as if the user
     * had navigated to it. It is narrated by `onAutocompleteResultChanged()`,
     * like any selection a post-deletion result leaves behind.
     */
    private async selectLineAfterMatchDeletion_(line: number) {
      const selection: OmniboxPopupSelection = {
        line,
        state: SelectionLineState.kNormal,
        actionIndex: 0,
      };
      if (this.virtualFocusEnabled) {
        this.setSelection(selection);
      } else {
        await this.getDropdownElement().selectIndex(line);
      }
      // Wait for `selectedMatch` to be updated.
      await this.updateComplete;
      this.fillInputForSelection(selection);
    }

    onInputFocusChanged(e: CustomEvent<{value: string, isOnFocus: boolean}>) {
      if (this.shouldAppendDotComOnCtrlEnter()) {
        this.controlKeyState = ControlKeyState.UP;
      }
      if (this.dropdownIsVisible) {
        return;
      }
      const input = e.detail.value;
      const isOnFocus = e.detail.isOnFocus;
      this.queryAutocomplete(
          input, /*preventInlineAutocomplete=*/ false, isOnFocus);
    }

    onSearchboxInputTextUpdated(e: CustomEvent<{
      value: string,
      isComposing: boolean,
      event?: Event,
    }>) {
      const input = e.detail.value;
      const cursorPosition =
          this.getInputElement().inputElement?.selectionStart ?? null;
      const event = e.detail.event ?? null;

      if (this.keywordModeManager_.acceptInputTrigger(
              input, cursorPosition, event)) {
        const isSpaceInMiddle = this.keywordModeManager_.entryMethod ===
            KeywordModeEntryMethod.SPACE_IN_MIDDLE;
        const remainingText = isSpaceInMiddle ?
            input.slice(
                cursorPosition ??
                (this.keywordModeManager_.activeKeyword.length + 1)) :
            '';
        this.getInputElement().setInputText(remainingText);
        if (isSpaceInMiddle) {
          this.getInputElement().setSelectionRange(0, 0);
        }
        this.queryAutocomplete(
            remainingText, /*preventInlineAutocomplete=*/ false,
            /*isOnFocus=*/ false);
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
          if (this.controlKeyState === ControlKeyState.UP) {
            this.controlKeyState = ControlKeyState.DOWN;
          }
        } else if (e.ctrlKey && e.key !== 'Enter') {
          if (this.controlKeyState === ControlKeyState.DOWN) {
            this.controlKeyState = ControlKeyState.DOWN_AND_CONSUMED;
          }
        }
      }

      const KEYDOWN_HANDLED_KEYS = [
        ' ',
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

    isInputEmpty(): boolean {
      return this.inputIsEmpty_;
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
      const inputEl = this.getInputElement()?.inputElement;
      const isInput = inputEl ? path.includes(inputEl) : false;
      return isInput || path.includes(this.getDropdownElement()) ||
          path.some(el => {
            const node = el as HTMLElement;
            return node.tagName === 'CR-SEARCHBOX-COMPOSE-BUTTON';
          });
    }

    /**
     * Handles Enter key presses on virtually focused elements (AIM, Action,
     * Remove Suggestion). Returns true if the event was handled.
     */
    handleVirtualFocusEnter(e: KeyboardEvent): boolean {
      if (this.selection.state === SelectionLineState.kFocusedButtonAim) {
        e.preventDefault();
        const button =
            this.shadowRoot.querySelector('cr-searchbox-compose-button');
        if (button) {
          button.fire('compose-click', {
            button: 0,
            ctrlKey: e.ctrlKey,
            metaKey: e.metaKey,
            shiftKey: e.shiftKey,
          });
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
          this.deleteMatch_(this.selection.line, this.selectedMatch);
        }
        return true;
      }

      if (this.isContextEntrypointVirtualFocused()) {
        e.preventDefault();
        this.openContextMenu();
        return true;
      }

      if (this.selection.state === SelectionLineState.kKeywordMode) {
        e.preventDefault();
        this.getInputElement().focus();
        return true;
      }

      return false;
    }

    /**
     * Handles Space key presses on virtually focused buttons (Action, Remove
     * Suggestion, Context Entrypoint, etc.). Unlike Enter, Space does not
     * activate the AIM button or intercept Keyword Mode, allowing a space
     * character to be typed into the input instead.
     */
    handleVirtualFocusSpace(e: KeyboardEvent): boolean {
      if (this.selection.state === SelectionLineState.kFocusedButtonAim) {
        this.setSelection({
          line: this.selection.line,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        });
        return false;
      }

      if (this.selection.state === SelectionLineState.kKeywordMode) {
        return false;
      }

      return this.handleVirtualFocusEnter(e);
    }

    /**
     * Writes the text the input shows for `selection`: the typed text again
     * when the selection is the input itself or a button outside the matches,
     * otherwise the selected match's text. Does not narrate; see
     * `narrateSelection()`.
     */
    fillInputForSelection(selection: OmniboxPopupSelection) {
      if (selection.line === -1) {
        // The input itself, or a button outside the matches: show the typed
        // text again, without any inline autocompletion.
        this.getInputElement().setInput({
          text: this.lastQueriedInput ?? '',
          inline: '',
          moveCursorToEnd: true,
        });
        return;
      }

      const match = this.selectedMatch;
      if (!match) {
        return;
      }
      const newFill = this.computeMatchFillIntoEdit(match);
      const isKeywordMode = this.keywordModeManager_.isInKeywordMode ||
          selection.state === SelectionLineState.kKeywordMode;
      const isNormal = selection.state === SelectionLineState.kNormal;
      const newInline = isNormal && !isKeywordMode && selection.line === 0 &&
              match.allowedToBeDefaultMatch ?
          match.inlineAutocompletion :
          '';
      const newFillEnd = newFill.length - newInline.length;
      this.getInputElement().setInput({
        text: newFill.substr(0, newFillEnd),
        inline: newInline,
        moveCursorToEnd: newInline.length === 0,
        isMatchPreview: this.isMatchPreview_(match, selection.line),
      });
    }

    /**
     * Narrates `selection` to screen readers with the label from
     * `getSelectionA11yLabel()`. This is the only narration of selection
     * changes, so nothing else may announce them. Call it after the input
     * reflects the selection (see `fillInputForSelection()`), so the label is
     * the last thing screen readers are told about and the value change does
     * not talk over it.
     */
    narrateSelection(selection: OmniboxPopupSelection) {
      this.getInputElement().setSelectionA11yLabel(
          this.getSelectionA11yLabel(this.selectedMatch, selection));
    }

    /**
     * Applies a user-driven selection change: fills the input for
     * `nextSelection`, narrates it, and tells the browser a navigation is
     * likely when `key` is an arrow or page key.
     */
    updateInputForSelection(nextSelection: OmniboxPopupSelection, key: string) {
      this.fillInputForSelection(nextSelection);
      this.narrateSelection(nextSelection);

      const isMatchSelection =
          nextSelection.state !== SelectionLineState.kFocusedButtonAim &&
          nextSelection.line !== -1;
      if (isMatchSelection && this.selectedMatch &&
          (key === 'ArrowDown' || key === 'ArrowUp' || key === 'PageDown' ||
           key === 'PageUp')) {
        this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
        this.pageHandler().onNavigationLikely(
            nextSelection.line, this.selectedMatch.destinationUrl,
            NavigationPredictor.kUpOrDownArrowButton);
      }
    }

    /**
     * Returns the label screen readers narrate for `selection`, where `match`
     * is the match `selection` resolves to, if any. This is the only narration
     * of virtual focus selection changes, so elements that can be virtually
     * focused must not announce themselves too. Subclasses that add virtually
     * focusable buttons should override this to label them; returning '' makes
     * screen readers narrate the input instead.
     */
    getSelectionA11yLabel(
        match: AutocompleteMatch|null,
        selection: OmniboxPopupSelection): string {
      if (selection.state === SelectionLineState.kFocusedButtonAim) {
        const button =
            this.shadowRoot.querySelector('cr-searchbox-compose-button');
        return button ? (button.a11yLabel || button.labelText) : '';
      }
      if (!match || selection.line < 0) {
        return '';
      }
      switch (selection.state) {
        case SelectionLineState.kNormal:
          return match.a11yLabel;
        case SelectionLineState.kKeywordMode:
          return match.keywordModel?.chipA11y || '';
        case SelectionLineState.kFocusedButtonAction:
          return match.actions[selection.actionIndex]?.a11yLabel || '';
        case SelectionLineState.kFocusedButtonRemoveSuggestion:
          return match.removeButtonA11yLabel;
        default:
          return '';
      }
    }

    private handleEnterNavigation_(e: KeyboardEvent) {
      if (this.multiLineEnabled && e.shiftKey) {
        return;
      }

      const isPureCtrlEnter = this.shouldAppendDotComOnCtrlEnter() &&
          e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey &&
          this.controlKeyState !== ControlKeyState.DOWN_AND_CONSUMED;

      e.preventDefault();
      if (this.handleVirtualFocusEnter(e)) {
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

      if (e.key === ' ' || e.key === '\u3000') {
        if (this.virtualFocusEnabled && this.dropdownIsVisible &&
            !e.isComposing && !hasKeyModifiers(e)) {
          this.handleVirtualFocusSpace(e);
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

        if (!this.virtualFocusEnabled || !this.dropdownIsVisible ||
            !this.isVirtualFocusEventTarget_(e)) {
          return;
        }
      }

      // ArrowUp/ArrowDown query autocomplete when matches are not
      // visible.
      if (!this.dropdownIsVisible) {
        if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
          if (this.multiLineEnabled && this.getInputElement()?.isMultiline()) {
            return;
          }
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
        this.fire('escape-searchbox', {
          event: e,
          emptyInput: !this.getInputElement().inputElement.value,
        });
      }

      // Do not handle the following keys if inside an IME composition session.
      if (e.isComposing) {
        return;
      }

      if (this.virtualFocusEnabled && e.key === 'Enter' &&
          this.handleVirtualFocusEnter(e)) {
        return;
      }

      // Do not handle the following keys if there are no matches available.
      if (!this.result || this.result.matches.length === 0) {
        return;
      }

      if (e.key === 'Delete') {
        if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
          if (this.selectedMatch && this.selectedMatch.supportsDeletion) {
            this.deleteMatch_(this.matchIndex, this.selectedMatch);
            e.preventDefault();
          }
        }
        return;
      }

      if (this.virtualFocusEnabled) {
        let step = SelectionStep.kStateOrLine;
        let direction = SelectionDirection.kForward;
        let valid = false;

        if (!e.altKey && !e.ctrlKey && !e.metaKey) {
          if (e.key === 'Tab' && this.dropdownIsVisible) {
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

          // Entering keyword mode via the keyboard must also enter keyword
          // mode in the browser, like clicking the keyword chip does in
          // `cr-searchbox-match`; otherwise the browser's keyword state
          // diverges from the WebUI's. Sent before `updateComplete` so it
          // precedes the `setPopupSelection()` issued on update.
          if (nextSelection.state === SelectionLineState.kKeywordMode) {
            const match = this.result?.matches[nextSelection.line];
            if (match?.keywordModel) {
              this.pageHandler().activateKeyword(
                  nextSelection.line, match.destinationUrl,
                  mojoTimeTicks(Date.now()), KeywordActivationMethod.kKeyboard);
            }
          }

          this.getInputElement().focus();

          await this.updateComplete;

          this.updateInputForSelection(nextSelection, e.key);
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

      if (e.key === 'ArrowDown' || e.key === 'ArrowUp' ||
          e.key === 'PageDown' || e.key === 'PageUp') {
        this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
      }

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
        if (!this.keywordModeManager_.isInKeywordMode) {
          assert(text);
        }
        const isMatchPreview =
            this.isMatchPreview_(this.selectedMatch, this.selectedMatchIndex);
        this.getInputElement().setInput({
          text: text,
          inline: newInline,
          moveCursorToEnd: newInline.length === 0,
          isMatchPreview: isMatchPreview,
        });
      }
    }

    private isMatchPreview_(
        match: AutocompleteMatch|null, selectionLine: number): boolean {
      return !!match &&
          (selectionLine > 0 ||
           (!match.allowedToBeDefaultMatch && selectionLine === 0));
    }

    onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>) {
      this.selectedMatchIndex = e.detail.value;
    }

    onMatchClick() {
      this.keywordModeManager_.exit();
      this.clearAutocompleteMatches();
    }

    async onMatchFocusin(e: CustomEvent<number>) {
      if (this.virtualFocusEnabled) {
        return;
      }
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
          isMatchPreview: true,
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
      if (!this.virtualFocusEnabled) {
        this.selectedMatchIndex = selection.line;
      }
      this.setSelection(selection);
      await this.updateComplete;
      this.updateInputForSelection(selection, 'click');
      const inputValue = this.getInputElement().getInputValue();
      if (inputValue) {
        this.queryAutocomplete(
            inputValue, /*preventInlineAutocomplete=*/ false,
            /*isOnFocus=*/ false);
      } else {
        this.lastQueriedInput = '';
      }
      this.getInputElement().focus();
    }

    private computeSelectedMatch_() {
      if (!this.result || !this.result.matches) {
        return null;
      }
      return this.result.matches[this.matchIndex] || null;
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

    /** Asks the backend to delete `match`, the match at `line`. */
    private deleteMatch_(line: number, match: AutocompleteMatch) {
      this.onMatchDeletionRequested_(line);
      this.pageHandler().deleteAutocompleteMatch(line, match.destinationUrl);
    }

    /**
     * Called when the backend is asked to delete the match at `line`, which is
     * null if the selection shouldn't move to that line afterwards. The
     * post-deletion result narrates the selection it leaves behind.
     */
    private onMatchDeletionRequested_(line: number|null) {
      // Unfreeze `activeQueryId` so post-deletion results are accepted.
      this.unfreezeActiveQueryId();
      this.pendingDeletionLine_ = line;
      this.narrateNextResultSelection_ = true;
    }
  }

  return SearchboxMixin;
};

export interface SearchboxMixinInterface extends
    SearchboxSelectionMixinInterface {
  virtualFocusEnabled: boolean;
  matchIndex: number;
  composeboxSource: string;
  controlKeyState: ControlKeyState;
  dropdownIsVisible: boolean;
  initialInputScrollHeight: number;
  activeQueryId: number;
  lastQueriedInput: string|null;
  multiLineEnabled: boolean;
  singleLineOnInlineAutocomplete: boolean;
  result: AutocompleteResult|null;
  searchboxAriaDescription: string;
  selectedMatch: AutocompleteMatch|null;
  selectedMatchIndex: number;
  readonly inputKeywordModel: InputKeywordModel|null;
  keywordModeManager: KeywordModeManager;
  showThumbnail: boolean;

  acceptInlineAutocomplete(e: KeyboardEvent): boolean;
  clearAutocompleteMatches(): void;
  computeMatchFillIntoEdit(match: AutocompleteMatch): string;
  fillInputForSelection(selection: OmniboxPopupSelection): void;
  getDropdownElement(): SearchboxDropdownElement;
  getInputElement(): SearchboxInputElement;
  getWrapperElement(): HTMLElement;
  handleKeyNavigation(e: KeyboardEvent): void;
  handleVirtualFocusEnter(e: KeyboardEvent): boolean;
  handleVirtualFocusSpace(e: KeyboardEvent): boolean;
  getSelectionA11yLabel(
      match: AutocompleteMatch|null, selection: OmniboxPopupSelection): string;
  hasMatches(): boolean;
  isAutocompleteResultStale(result: AutocompleteResult): boolean;
  isBackgroundTabNavigation(e: KeyboardEvent|MouseEvent): boolean;
  isInputEmpty(): boolean;
  narrateSelection(selection: OmniboxPopupSelection): void;
  shouldDropdownBeVisible(): boolean;
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
  openContextMenu(): void;
  openCtrlEnterMatch(matchIndex: number): void;
  onSearchboxInputTextUpdated(e: CustomEvent<{
    value: string,
    isComposing: boolean,
    event?: Event,
  }>): void;
  onSelectedMatchIndexChanged(e: CustomEvent<{value: number}>): void;
  pageHandler(): PageHandlerInterface;
  queryAutocomplete(
      input: string, preventInlineAutocomplete: boolean,
      isOnFocus: boolean): void;
  getTabId(): number|null;
  shouldAppendDotComOnCtrlEnter(): boolean;
  shouldPreserveSelectionOnMatchDeletion(): boolean;
  updateInputForSelection(nextSelection: OmniboxPopupSelection, key: string):
      void;
}
