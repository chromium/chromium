// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {KeywordType, SelectionLineState} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, InputKeywordModel, OmniboxPopupSelection} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

/**
 * Loosely based on `metrics::OmniboxEventProto::KeywordModeEntryMethod` in
 * third_party/metrics_proto/omnibox_event.proto, but not 1:1.
 */
export enum KeywordModeEntryMethod {
  NONE = 0,
  TAB = 1,
  SPACE_AT_END = 2,
  QUESTION_MARK = 3,
  KEYBOARD_SHORTCUT = 4,
  CLICK = 5,
  SPACE_IN_MIDDLE = 6,
}

export interface KeywordClearedEvent {
  restoredText: string;
  cursorPosition: number;
}

export interface InputSelectionState {
  value: string;
  selectionStart: number|null;
  selectionEnd: number|null;
}

export interface KeywordModeManagerDelegate {
  onKeywordModelChanged(): void;
  onKeywordCleared(event: KeywordClearedEvent): void;
  onKeywordEntered(): void;
}

/**
 * Manages keyword mode state, entry methods, and text formatting algorithms for
 * the WebUI searchbox.
 */
export class KeywordModeManager {
  keywordSpaceTriggeringEnabled: boolean = loadTimeData.isInitialized() &&
          loadTimeData.valueExists('keywordSpaceTriggeringEnabled') ?
      loadTimeData.getBoolean('keywordSpaceTriggeringEnabled') :
      true;

  private availableKeywordModels_: Map<string, InputKeywordModel> = new Map();
  private defaultSearchEngineKeyword_: string = '';
  private inputKeywordModel_: InputKeywordModel|null = null;
  private entryMethod_: KeywordModeEntryMethod = KeywordModeEntryMethod.NONE;
  private delegate_: KeywordModeManagerDelegate;

  constructor(delegate: KeywordModeManagerDelegate) {
    this.delegate_ = delegate;
  }

  get entryMethod(): KeywordModeEntryMethod {
    return this.entryMethod_;
  }

  get defaultSearchEngineKeyword(): string {
    return this.defaultSearchEngineKeyword_;
  }

  set defaultSearchEngineKeyword(keyword: string) {
    this.defaultSearchEngineKeyword_ = keyword;
  }

  get availableKeywordModels(): InputKeywordModel[] {
    return Array.from(this.availableKeywordModels_.values());
  }

  set availableKeywordModels(models: InputKeywordModel[]) {
    this.availableKeywordModels_ =
        new Map(models.map(model => [model.keyword.toLowerCase(), model]));
  }

  get inputKeywordModel(): InputKeywordModel|null {
    return this.inputKeywordModel_;
  }

  set inputKeywordModel(model: InputKeywordModel|null) {
    this.inputKeywordModel_ = model;
    this.delegate_.onKeywordModelChanged();
  }

  get isInKeywordMode(): boolean {
    return this.inputKeywordModel_?.type === KeywordType.kInKeyword;
  }

  get activeKeyword(): string {
    return this.isInKeywordMode && this.inputKeywordModel_?.keyword ?
        this.inputKeywordModel_.keyword :
        '';
  }

  /**
   * Enters keyword mode with the specified keyword, displayText hint, and entry
   * method.
   */
  enter(
      keyword: string, displayText: string, entryMethod: KeywordModeEntryMethod,
      placeholder: string = ''): void {
    // TODO(crbug.com/546826241): To fully support keyword mode entryMethod
    // state needs to be saved/restored across tabs.
    this.entryMethod_ = entryMethod;
    this.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: keyword,
      displayText: displayText,
      iconPath:
          this.availableKeywordModels_.get(keyword.toLowerCase())?.iconPath ||
          '',
      placeholder: placeholder,
    };
  }

  /**
   * Enters keyword mode for the default search engine if available.
   * Returns true if keyword mode was entered.
   */
  enterDefaultSearchEngineKeywordMode(entryMethod: KeywordModeEntryMethod):
      boolean {
    if (!this.defaultSearchEngineKeyword_) {
      return false;
    }
    const model = this.availableKeywordModels_.get(
        this.defaultSearchEngineKeyword_.toLowerCase());
    if (!model) {
      return false;
    }
    this.enter(
        model.keyword, model.displayText || model.keyword, entryMethod,
        model.placeholder || '');
    return true;
  }

  /**
   * Exits keyword mode, resetting the keyword model and entry state.
   */
  exit(): void {
    this.entryMethod_ = KeywordModeEntryMethod.NONE;
    this.inputKeywordModel = null;
  }

  /**
   * Handles Backspace when cursor is at index 0 in keyword mode, exiting
   * keyword mode and notifying the delegate of the restored text and cursor.
   * Returns true if Backspace was handled.
   */
  handleBackspace(inputState: InputSelectionState): boolean {
    const cursorAtStart =
        inputState.selectionStart === 0 && inputState.selectionEnd === 0;
    if (!this.isInKeywordMode || !cursorAtStart) {
      return false;
    }

    let prefix = this.activeKeyword ? `${this.activeKeyword} ` : '';
    if (this.entryMethod_ === KeywordModeEntryMethod.TAB && !inputState.value) {
      prefix = this.activeKeyword;
    } else if (this.entryMethod_ === KeywordModeEntryMethod.QUESTION_MARK) {
      prefix = '?';
    } else if (this.entryMethod_ === KeywordModeEntryMethod.KEYBOARD_SHORTCUT) {
      prefix = '';
    }

    const restoredText = prefix + inputState.value;
    const newCursorPos = prefix.length;

    this.exit();
    this.delegate_.onKeywordCleared({
      restoredText: restoredText,
      cursorPosition: newCursorPos,
    });
    return true;
  }

  /**
   * Handles clicking a keyword chip on an autocomplete match, entering keyword
   * mode and notifying the delegate.
   */
  handleKeywordClick(match: AutocompleteMatch): void {
    assert(match.keywordModel);
    this.enter(
        match.keywordModel.keyword, match.keywordModel.chipHint,
        KeywordModeEntryMethod.CLICK, match.keywordModel.placeholder);
    this.delegate_.onKeywordEntered();
  }

  /**
   * Evaluates whether pressing Tab on the selected match triggers keyword mode.
   * Only default matches (matchIndex === 0 and allowedToBeDefaultMatch) can
   * accept keyword mode via Tab. If triggered, enters keyword mode, notifies
   * the delegate, and returns true.
   */
  acceptTab(match: AutocompleteMatch|null, matchIndex: number): boolean {
    if (!match?.keywordModel) {
      return false;
    }
    const isDefaultMatch = matchIndex === 0 && match.allowedToBeDefaultMatch;
    if (!isDefaultMatch) {
      return false;
    }
    this.enter(
        match.keywordModel.keyword, match.keywordModel.chipHint,
        KeywordModeEntryMethod.TAB, match.keywordModel.placeholder);
    this.delegate_.onKeywordEntered();
    return true;
  }

  /**
   * Evaluates whether the updated input text and cursor position trigger
   * keyword mode (e.g. space after instant keyword, or leading '?').
   * If triggered, enters keyword mode and returns true.
   */
  acceptInputTrigger(
      input: string, cursorPosition: number|null, event: Event|null): boolean {
    if (cursorPosition === null) {
      return false;
    }
    return this.acceptSpaceAtEnd_(input, cursorPosition, event) ||
        this.acceptSpaceInMiddle_(input, cursorPosition, event) ||
        this.acceptQuestionMark_(input, cursorPosition, event);
  }

  /**
   * Returns true if the event represents one of the specified characters being
   * typed (not pasted or backspaced to).
   */
  private isTypedCharacterEvent_(event: Event|null, chars: string[]): boolean {
    if (event instanceof KeyboardEvent) {
      return chars.includes(event.key);
    }
    if (event instanceof InputEvent) {
      return event.data !== null && chars.includes(event.data) &&
          event.inputType !== 'insertFromPaste';
    }
    return false;
  }

  private isSpaceEvent_(event: Event|null): boolean {
    return this.isTypedCharacterEvent_(event, [' ', '\u3000']);
  }

  private acceptSpaceAtEnd_(
      input: string, cursorPosition: number, event: Event|null): boolean {
    // Space triggering must be enabled.
    if (!this.keywordSpaceTriggeringEnabled) {
      return false;
    }

    // Must not already be in keyword mode.
    if (this.isInKeywordMode) {
      return false;
    }

    // Space must have been typed, not backspaced to a space or pasted.
    if (!this.isSpaceEvent_(event)) {
      return false;
    }

    // Cursor must be at end.
    if (cursorPosition !== input.length) {
      return false;
    }

    // Input must end in space.
    if (!input.endsWith(' ') && !input.endsWith('\u3000')) {
      return false;
    }

    // Keyword candidate is the single word preceding the space.
    const candidate = input.slice(0, -1);
    if (!candidate || candidate.includes(' ') || candidate.includes('\u3000')) {
      return false;
    }

    // Must match an available keyword.
    const lowerCandidate = candidate.toLowerCase();
    const model = this.availableKeywordModels_.get(lowerCandidate) ||
        (this.inputKeywordModel_?.keyword.toLowerCase() === lowerCandidate ?
             this.inputKeywordModel_ :
             null);
    if (!model) {
      return false;
    }

    const keyword = model.keyword;
    const displayText = model.displayText || keyword;
    const placeholder = model.placeholder || '';

    this.enter(
        keyword, displayText, KeywordModeEntryMethod.SPACE_AT_END, placeholder);
    return true;
  }

  private acceptSpaceInMiddle_(
      input: string, cursorPosition: number, event: Event|null): boolean {
    // Space triggering must be enabled.
    if (!this.keywordSpaceTriggeringEnabled) {
      return false;
    }

    // Must not already be in keyword mode.
    if (this.isInKeywordMode) {
      return false;
    }

    // Space must have been typed, not backspaced to a space or pasted.
    if (!this.isSpaceEvent_(event)) {
      return false;
    }

    // Cursor must be after at least 1 keyword character and the typed space,
    // with at least 1 character after the space.
    const spacePosition = cursorPosition - 1;
    if (spacePosition <= 0 || cursorPosition >= input.length) {
      return false;
    }

    // Character at spacePosition must be a space.
    const spaceChar = input[spacePosition];
    if (spaceChar !== ' ' && spaceChar !== '\u3000') {
      return false;
    }

    // Character preceding the space must not be whitespace.
    const charBeforeSpace = input[spacePosition - 1];
    if (charBeforeSpace === ' ' || charBeforeSpace === '\u3000') {
      return false;
    }

    // Keyword candidate is the single word preceding the space.
    const candidate = input.slice(0, spacePosition);
    if (candidate.includes(' ') || candidate.includes('\u3000')) {
      return false;
    }

    // Must match an available keyword.
    const lowerCandidate = candidate.toLowerCase();
    const model = this.availableKeywordModels_.get(lowerCandidate) ||
        (this.inputKeywordModel_?.keyword.toLowerCase() === lowerCandidate ?
             this.inputKeywordModel_ :
             null);
    if (!model) {
      return false;
    }

    // Text after the space must not be empty or start with whitespace.
    const textAfter = input.slice(cursorPosition);
    if (!textAfter.trim() || textAfter.startsWith(' ') ||
        textAfter.startsWith('\u3000')) {
      return false;
    }

    const keyword = model.keyword;
    const displayText = model.displayText || keyword;
    const placeholder = model.placeholder || '';

    this.enter(
        keyword, displayText, KeywordModeEntryMethod.SPACE_IN_MIDDLE,
        placeholder);
    return true;
  }

  private acceptQuestionMark_(
      input: string, cursorPosition: number, event: Event|null): boolean {
    // Cursor must be after '?'.
    if (cursorPosition !== 1) {
      return false;
    }

    // Input must be '?'.
    if (input !== '?') {
      return false;
    }

    // Must not already be in keyword mode.
    if (this.isInKeywordMode) {
      return false;
    }

    // Question mark must have been typed, not backspaced to '?' or pasted.
    if (!this.isTypedCharacterEvent_(event, ['?'])) {
      return false;
    }

    return this.enterDefaultSearchEngineKeywordMode(
        KeywordModeEntryMethod.QUESTION_MARK);
  }

  /**
   * Formats a match's fillIntoEdit string when in keyword mode (stripping the
   * redundant keyword prefix), or restores the original input text for default
   * matches (avoiding prepending URL schemes like https://).
   */
  formatMatchFillIntoEdit(
      match: AutocompleteMatch, matchIndex: number,
      lastQueriedInput?: string|null): string {
    if (this.isInKeywordMode) {
      const keyword = this.inputKeywordModel_?.keyword;
      if (keyword) {
        const lowerKeyword = keyword.toLowerCase();
        const lowerFill = match.fillIntoEdit.toLowerCase();
        if (lowerFill.startsWith(lowerKeyword + ' ')) {
          return match.fillIntoEdit.substring(keyword.length + 1);
        }
        if (lowerFill === lowerKeyword ||
            (match.keywordModel?.type !== KeywordType.kInKeyword &&
             match.keywordModel?.keyword.toLowerCase() === lowerKeyword)) {
          return '';
        }
      }
      return match.fillIntoEdit;
    }
    const isDefaultMatch = matchIndex === 0 && match.allowedToBeDefaultMatch;
    if (isDefaultMatch && lastQueriedInput) {
      return lastQueriedInput + match.inlineAutocompletion;
    }
    return match.fillIntoEdit;
  }

  /**
   * Updates or preserves the keyword model when the selected autocomplete match
   * or popup selection changes.
   */
  onSelectedMatchChanged(
      selectedMatch: AutocompleteMatch|null,
      selection: OmniboxPopupSelection): void {
    // The case when the searchbox is closed, clobbered, or has no results.
    if (!selectedMatch) {
      // If we're in keyword mode, don't leave. Otherwise, e.g. if we've
      // selected a match with a chip but not selected the chip yet, then clear
      // the hint state.
      if (!this.isInKeywordMode) {
        this.exit();
      }
      return;
    }

    // When a non-keyword match is focused, input should not be in keyword mode.
    if (!selectedMatch.keywordModel) {
      this.exit();
      return;
    }

    // When either a chip, an instant keyword, or in-keyword match are focused,
    // input should be in keyword mode.
    if (selection.state === SelectionLineState.kKeywordMode ||
        selectedMatch.keywordModel.type === KeywordType.kInKeyword) {
      if (this.activeKeyword.toLowerCase() !==
          selectedMatch.keywordModel.keyword.toLowerCase()) {
        this.enter(
            selectedMatch.keywordModel.keyword,
            selectedMatch.keywordModel.chipHint, KeywordModeEntryMethod.TAB,
            selectedMatch.keywordModel.placeholder);
      }
      return;
    }

    // When a match with a chip is focused, but the chip is not, the input
    // should be in keyword hint mode.
    assert(selectedMatch.keywordModel.type === KeywordType.kChip);
    this.entryMethod_ = KeywordModeEntryMethod.NONE;
    this.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: selectedMatch.keywordModel.keyword,
      displayText: selectedMatch.keywordModel.chipHint,
      iconPath: this.availableKeywordModels_
                    .get(selectedMatch.keywordModel.keyword.toLowerCase())
                    ?.iconPath ||
          '',
      placeholder: selectedMatch.keywordModel.placeholder,
    };
  }
}
