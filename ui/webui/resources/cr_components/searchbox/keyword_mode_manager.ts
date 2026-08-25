// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {KeywordType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, InputKeywordModel} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

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
  private inputKeywordModel_: InputKeywordModel|null = null;
  private delegate_: KeywordModeManagerDelegate;

  constructor(delegate: KeywordModeManagerDelegate) {
    this.delegate_ = delegate;
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
   * Enters keyword mode with the specified keyword and displayText hint.
   */
  enter(keyword: string, displayText: string): void {
    this.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: keyword,
      displayText: displayText,
    };
  }

  /**
   * Exits keyword mode, resetting the keyword model.
   */
  exit(): void {
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

    const remainingText = inputState.value;
    // TODO(b/504669216): Restoring keyword text correctly is more
    //   complicated than just prepending keyword and a space.
    const restoredKeywordText = this.activeKeyword + ' ';
    const restoredText = restoredKeywordText + remainingText;
    const newCursorPos = restoredKeywordText.length;

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
    this.enter(match.keywordModel.keyword, match.keywordModel.chipHint);
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
    this.enter(match.keywordModel.keyword, match.keywordModel.chipHint);
    this.delegate_.onKeywordEntered();
    return true;
  }

  /**
   * Evaluates whether the updated input text and cursor position trigger
   * keyword mode (e.g. space after instant keyword, or leading '?').
   * If triggered, enters keyword mode and returns true.
   */
  acceptInputTrigger(input: string, cursorPosition: number|null): boolean {
    if (cursorPosition === null) {
      return false;
    }
    return this.acceptSpaceAtEnd_(input, cursorPosition) ||
        this.acceptQuestionMark_(input, cursorPosition);
  }

  private acceptSpaceAtEnd_(input: string, cursorPosition: number): boolean {
    // Cursor must be at end.
    if (cursorPosition !== input.length) {
      return false;
    }

    // Input must end in space.
    if (!input.endsWith(' ') && !input.endsWith('　')) {
      return false;
    }

    // Chip must be shown.
    if (this.inputKeywordModel_?.type !== KeywordType.kChip) {
      return false;
    }

    // Input must match keyword.
    const keyword = this.inputKeywordModel_.keyword;
    if (!keyword || input.slice(0, -1) !== keyword) {
      return false;
    }

    // Space must have been typed, not backspaced to a space. E.g. 'keyword
    // q<backspace>' should not accept keyword mode.
    // TODO(b/504669216): this isn't handled yet.

    // Space must have been typed, not pasted.
    // TODO(b/504669216): webUI doesn't track paste state yet.

    // Space triggering must be enabled.
    // TODO(b/504669216): webUI isn't aware of
    //   `kKeywordSpaceTriggeringEnabled` pref.

    this.enter(keyword, this.inputKeywordModel_.displayText);
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

    this.enter('?', '');
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
    const isDefaultMatch = matchIndex === 0 && match.allowedToBeDefaultMatch;
    if (isDefaultMatch && lastQueriedInput) {
      return lastQueriedInput + match.inlineAutocompletion;
    }
    if (this.isInKeywordMode) {
      const keyword = this.inputKeywordModel_?.keyword;
      if (keyword && match.fillIntoEdit.startsWith(keyword + ' ')) {
        return match.fillIntoEdit.substring(keyword.length + 1);
      }
    }
    return match.fillIntoEdit;
  }

  /**
   * Updates or preserves the keyword model when the selected autocomplete match
   * changes.
   */
  onSelectedMatchChanged(selectedMatch: AutocompleteMatch|null): void {
    // If the input is in keyword mode, preserve keyword mode.
    if (this.isInKeywordMode) {
      return;
    }
    if (!selectedMatch?.keywordModel) {
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
