// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Action, AutocompleteMatch, AutocompleteResult, OmniboxPopupSelection} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {KeywordType, SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {kDefaultSelection} from './searchbox_match.js';

export type {
  Action,
  AutocompleteMatch,
  AutocompleteResult,
  OmniboxPopupSelection,
};
export {SelectionDirection, SelectionLineState, SelectionStep};

export function selectionsEqual(
    a: OmniboxPopupSelection, b: OmniboxPopupSelection): boolean {
  return a.line === b.line && a.state === b.state &&
      a.actionIndex === b.actionIndex;
}

export function selectionToString(s: OmniboxPopupSelection): string {
  return `{${s.line},${s.state},${s.actionIndex}}`;
}

export function selectionIsNativelySupported(s: OmniboxPopupSelection):
    boolean {
  return s.state !== SelectionLineState.kFocusedButtonContextEntrypoint;
}

function findSelectionIndex(
    selections: OmniboxPopupSelection[],
    target: OmniboxPopupSelection): number {
  let index = selections.findIndex(s => selectionsEqual(target, s));
  if (index < 0 && target.state === SelectionLineState.kKeywordMode) {
    index = selections.findIndex(
        s =>
            selectionsEqual({...target, state: SelectionLineState.kNormal}, s));
  }
  if (index < 0 && target.state === SelectionLineState.kNormal) {
    index = selections.findIndex(
        s => selectionsEqual(
            {...target, state: SelectionLineState.kKeywordMode}, s));
  }
  return index;
}

function getSelectionsForMatch(
    match: AutocompleteMatch, matchIndex: number): OmniboxPopupSelection[] {
  if (match.isHidden && !match.allowedToBeDefaultMatch) {
    return [];
  }
  const selections: OmniboxPopupSelection[] = [{
    line: matchIndex,
    state: match.keywordModel?.type === KeywordType.kInstant ?
        SelectionLineState.kKeywordMode :
        SelectionLineState.kNormal,
    actionIndex: 0,
  }];
  if (match.keywordModel?.type === KeywordType.kChip) {
    selections.push({
      line: matchIndex,
      state: SelectionLineState.kKeywordMode,
      actionIndex: 0,
    });
  }
  if (match.actions) {
    match.actions.forEach((_: Action, actionIndex: number) => {
      selections.push({
        line: matchIndex,
        state: SelectionLineState.kFocusedButtonAction,
        actionIndex: actionIndex,
      });
    });
  }
  if (match.supportsDeletion) {
    selections.push({
      line: matchIndex,
      state: SelectionLineState.kFocusedButtonRemoveSuggestion,
      actionIndex: 0,
    });
  }
  return selections;
}

export function getMatchSelections(result: AutocompleteResult|null):
    OmniboxPopupSelection[] {
  if (!result) {
    return [];
  }
  return result.matches.flatMap(
      (match: AutocompleteMatch, matchIndex: number) =>
          getSelectionsForMatch(match, matchIndex));
}

type Constructor<T> = new (...args: any[]) => T;

export interface SearchboxSelectionMixinInterface {
  isAimButtonVisible: boolean;
  showContextEntrypoint: boolean;

  selection: OmniboxPopupSelection;
  setSelection(selection: OmniboxPopupSelection): void;

  getAvailableSelections(result: AutocompleteResult|null):
      OmniboxPopupSelection[];

  stepCyclesSelection(
      result: AutocompleteResult|null, from: OmniboxPopupSelection,
      direction: SelectionDirection, step: SelectionStep): boolean;

  getNextSelection(
      result: AutocompleteResult|null, from: OmniboxPopupSelection,
      direction: SelectionDirection,
      step: SelectionStep): OmniboxPopupSelection;

  onSelectionChanged(e: CustomEvent<{value: OmniboxPopupSelection}>): void;
  isAiModeVirtualFocused(): boolean;
}

export type SearchboxSelectionMixinBase = CrLitElement;

export const SearchboxSelectionMixin = <
    T extends Constructor<SearchboxSelectionMixinBase>>(
    superClass: T): T&Constructor<SearchboxSelectionMixinInterface> => {
  class SearchboxSelectionMixin extends superClass implements
      SearchboxSelectionMixinInterface {
    private selection_: OmniboxPopupSelection = kDefaultSelection;

    get isAimButtonVisible(): boolean {
      return false;
    }

    get showContextEntrypoint(): boolean {
      return false;
    }

    get selection(): OmniboxPopupSelection {
      return this.selection_;
    }

    setSelection(selection: OmniboxPopupSelection) {
      if (selectionsEqual(this.selection_, selection)) {
        return;
      }

      const oldSelection = this.selection_;
      this.selection_ = selection;

      this.requestUpdate('selection', oldSelection);
    }

    onSelectionChanged(e: CustomEvent<{value: OmniboxPopupSelection}>) {
      this.setSelection(e.detail.value);
    }

    isAiModeVirtualFocused(): boolean {
      return this.selection_.state === SelectionLineState.kFocusedButtonAim;
    }

    getAvailableSelections(result: AutocompleteResult|null):
        OmniboxPopupSelection[] {
      if (!result) {
        return [];
      }
      const available = getMatchSelections(result);

      if (this.showContextEntrypoint) {
        available.push({
          line: -1,
          state: SelectionLineState.kFocusedButtonContextEntrypoint,
          actionIndex: 0,
        });
      }

      if (this.isAimButtonVisible) {
        const insertionIndex =
            available.length > 0 && result.matches[0]?.allowedToBeDefaultMatch ?
            1 :
            0;
        available.splice(insertionIndex, 0, {
          line: result.matches.findIndex(
              (m: AutocompleteMatch) => m.allowedToBeDefaultMatch),
          state: SelectionLineState.kFocusedButtonAim,
          actionIndex: 0,
        });
        if (!result.matches[0]?.allowedToBeDefaultMatch) {
          // If there is no default match, we need a generic selection to
          // represent the input field so that it can be focused.
          available.splice(0, 0, {
            line: -1,
            state: SelectionLineState.kNormal,
            actionIndex: 0,
          });
        }
      }

      return available;
    }

    /**
     * Determines whether stepping from `from` in `direction` with `step`
     * granularity wraps around (cycles) the available selections.
     */
    stepCyclesSelection(
        result: AutocompleteResult|null, from: OmniboxPopupSelection,
        direction: SelectionDirection, step: SelectionStep): boolean {
      const available = this.getAvailableSelections(result);
      if (available.length === 0) {
        return true;
      }

      const fromIndex = findSelectionIndex(available, from);
      // When starting from the input (not in available selections), stepping
      // backward immediately cycles/exits, while stepping forward enters the
      // first available selection without cycling.
      if (fromIndex < 0) {
        return direction === SelectionDirection.kBackward;
      }

      // Compute the next selection target.
      const next = this.getNextSelection(result, from, direction, step);
      const nextIndex = findSelectionIndex(available, next);
      if (nextIndex < 0) {
        return true;
      }
      return direction === SelectionDirection.kForward ?
          nextIndex <= fromIndex :
          nextIndex >= fromIndex;
    }

    getNextSelection(
        result: AutocompleteResult|null, from: OmniboxPopupSelection,
        direction: SelectionDirection,
        step: SelectionStep): OmniboxPopupSelection {
      const available = this.getAvailableSelections(result);
      if (available.length === 0) {
        return from;
      }
      const isNormal = (selection: OmniboxPopupSelection) =>
          (selection.state === SelectionLineState.kNormal ||
           (selection.state === SelectionLineState.kKeywordMode &&
            result?.matches[selection.line]?.keywordModel?.type ===
                KeywordType.kInstant)) &&
          selection.line !== -1;
      let fromIndex = findSelectionIndex(available, from);

      const selectionsList = [...available];
      if (fromIndex < 0) {
        selectionsList.splice(0, 0, from);
        fromIndex = 0;
      }
      if (step === SelectionStep.kAllLines) {
        if (direction === SelectionDirection.kBackward && from.line === -1) {
          return from;
        }
        const normalIndex = direction === SelectionDirection.kBackward ?
            selectionsList.findIndex(isNormal) :
            selectionsList.findLastIndex(isNormal);
        return normalIndex < 0 ? from : selectionsList[normalIndex]!;
      }

      for (let offset = 1; offset < selectionsList.length; offset++) {
        const offsetDirection =
            direction === SelectionDirection.kForward ? offset : -offset;
        const newIndex = fromIndex + offsetDirection;

        const remainder = (lhs: number, rhs: number) =>
            ((lhs % rhs) + rhs) % rhs;
        const index = remainder(newIndex, selectionsList.length);
        const selection = selectionsList[index]!;
        if (step === SelectionStep.kStateOrLine || isNormal(selection)) {
          return selection;
        }
      }
      return from;
    }
  }

  return SearchboxSelectionMixin;
};
