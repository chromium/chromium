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
  private inputKeywordModel_: InputKeywordModel|null = null;
  private entryMethod_: KeywordModeEntryMethod = KeywordModeEntryMethod.NONE;
  private delegate_: KeywordModeManagerDelegate;

  constructor(delegate: KeywordModeManagerDelegate) {
    this.delegate_ = delegate;
  }

  get entryMethod(): KeywordModeEntryMethod {
    return this.entryMethod_;
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
      keyword: string, displayText: string,
      entryMethod: KeywordModeEntryMethod): void {
    // TODO(crbug.com/546826241): To fully support keyword mode entryMethod
    // state needs to be saved/restored across tabs.
    this.entryMethod_ = entryMethod;
    this.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: keyword,
      displayText: displayText,
    };
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
        KeywordModeEntryMethod.CLICK);
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
        KeywordModeEntryMethod.TAB);
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
        this.acceptQuestionMark_(input, cursorPosition);
  }

  private isSpaceEvent_(event: Event|null): boolean {
    if (event instanceof KeyboardEvent) {
      return event.key === ' ' || event.key === '\u3000';
    }
    if (event instanceof InputEvent) {
      return (event.data === ' ' || event.data === '\u3000') &&
          event.inputType !== 'insertFromPaste';
    }
    return false;
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

    this.enter(keyword, displayText, KeywordModeEntryMethod.SPACE_AT_END);
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

    this.enter(keyword, displayText, KeywordModeEntryMethod.SPACE_IN_MIDDLE);
    return true;
  }

  private acceptQuestionMark_(input: string, cursorPosition: number): boolean {
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

    // Input must have been typed, not backspaced to '?'. E.g. '?q<backspace>'
    // should not enter keyword mode.
    // TODO(b/504669216): this isn't handled yet.

    // Input must have been typed, not pasted.
    // TODO(b/504669216): webUI doesn't track paste state yet.

    this.enter('?', '', KeywordModeEntryMethod.QUESTION_MARK);
    return true;
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
      selection?: OmniboxPopupSelection): void {
    if (!selectedMatch) {
      if (!this.isInKeywordMode) {
        this.inputKeywordModel = null;
      }
      return;
    }
    const isKeywordChipSelected =
        selection?.state === SelectionLineState.kKeywordMode ||
        selectedMatch.keywordModel?.type === KeywordType.kInstant;

    if (isKeywordChipSelected && selectedMatch.keywordModel) {
      if (!this.isInKeywordMode ||
          this.activeKeyword.toLowerCase() !==
              selectedMatch.keywordModel.keyword.toLowerCase()) {
        this.enter(
            selectedMatch.keywordModel.keyword,
            selectedMatch.keywordModel.chipHint, KeywordModeEntryMethod.TAB);
      }
      return;
    }
    if (selectedMatch.keywordModel?.type === KeywordType.kInKeyword) {
      return;
    }
    if (this.isInKeywordMode) {
      this.exit();
    }
    if (!selectedMatch.keywordModel) {
      this.inputKeywordModel = null;
      return;
    }
    this.inputKeywordModel = {
      type: selectedMatch.keywordModel.type,
      keyword: selectedMatch.keywordModel.keyword,
      displayText: selectedMatch.keywordModel.chipHint,
    };
  }
}
