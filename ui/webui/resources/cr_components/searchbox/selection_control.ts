// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Action, AutocompleteMatch, AutocompleteResult, OmniboxPopupSelection} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

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

function getSelectionsForMatch(
    match: AutocompleteMatch, matchIndex: number): OmniboxPopupSelection[] {
  if (match.isHidden && !match.allowedToBeDefaultMatch) {
    return [];
  }
  const selections: OmniboxPopupSelection[] = [{
    line: matchIndex,
    state: SelectionLineState.kNormal,
    actionIndex: 0,
  }];
  if (match.keywordChipHint && match.keywordChipHint.length > 0) {
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

export function getNextSelection(
    from: OmniboxPopupSelection, direction: SelectionDirection,
    step: SelectionStep,
    available: OmniboxPopupSelection[]): OmniboxPopupSelection {
  if (available.length === 0) {
    return from;
  }
  const isNormal = (selection: OmniboxPopupSelection) =>
      selection.state === SelectionLineState.kNormal;
  let fromIndex = available.findIndex(s => selectionsEqual(from, s));
  if (fromIndex < 0 && from.state === SelectionLineState.kKeywordMode) {
    fromIndex = available.findIndex(
        s => selectionsEqual({...from, state: SelectionLineState.kNormal}, s));
  }

  const selectionsList = [...available];
  if (fromIndex < 0) {
    selectionsList.splice(0, 0, from);
    fromIndex = 0;
  }
  if (step === SelectionStep.kAllLines) {
    const normalIndex = direction === SelectionDirection.kBackward ?
        selectionsList.findIndex(isNormal) :
        selectionsList.findLastIndex(isNormal);
    return normalIndex < 0 ? from : selectionsList[normalIndex]!;
  }

  const remainder = (lhs: number, rhs: number) => ((lhs % rhs) + rhs) % rhs;
  for (let offset = 1; offset < selectionsList.length; offset++) {
    const offsetDirection =
        direction === SelectionDirection.kForward ? offset : -offset;
    const index = remainder(fromIndex + offsetDirection, selectionsList.length);
    const selection = selectionsList[index]!;
    if (step === SelectionStep.kStateOrLine || isNormal(selection)) {
      return selection;
    }
  }
  return from;
}
