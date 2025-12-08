// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox_match.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_dropdown.css.js';
import {getHtml} from './composebox_dropdown.html.js';

// The '%' operator in JS returns negative numbers. This workaround avoids that.
function remainder(lhs: number, rhs: number) {
  return ((lhs % rhs) + rhs) % rhs;
}

// TODO(crbug.com/439616869): Provide an API for the embedder (i.e.,
// <cr-composebox>) to change the selection. A dropdown element that contains
// autocomplete matches.
export class ComposeboxDropdownElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-dropdown';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      result: {
        type: Object,
      },
      selectedMatchIndex: {
        type: Number,
        notify: true,
      },
      lastQueriedInput: {
        type: String,
        notify: true,
      },
      maxSuggestions: {
        type: Number,
      },
    };
  }

  accessor result: AutocompleteResult|null = null;
  accessor selectedMatchIndex: number = -1;
  accessor lastQueriedInput: string = '';
  // Limits the number of suggestions shown in the dropdown. This allows
  // the embedder to limit the number of suggestions based on available
  // height. A value of 0 indicates that no suggestions should be shown.
  // A value of null or -1 indicates that all suggestions should be shown.
  accessor maxSuggestions: number|null = null;

  //============================================================================
  // Public methods
  //============================================================================

  /** Unselects the currently selected match, if any. */
  unselect() {
    this.selectedMatchIndex = -1;
  }

  /** Focuses the selected match, if any. */
  focusSelected() {
    const selectableMatchElements =
        this.shadowRoot.querySelectorAll('cr-composebox-match');
    selectableMatchElements[this.selectedMatchIndex]?.focus();
  }

  /** Selects the first match. */
  selectFirst() {
    this.selectedMatchIndex = 0;
  }

  /** Selects the match at the given index. */
  selectIndex(index: number) {
    this.selectedMatchIndex = index;
  }

  /**
   * Selects the previous match with respect to the currently selected one.
   * Selects the last match if the first one or no match is currently selected.
   */
  selectPrevious() {
    if (!this.result) {
      this.selectedMatchIndex = -1;
      return;
    }

    let previous: number;
    const isTypedSuggest = this.lastQueriedInput.trim().length > 0;
    const maxVisibleIndex = this.getMaxVisibleIndex_();
    if (isTypedSuggest && this.selectedMatchIndex === 1) {
      // Since we're hiding the first match, if we're on the second match (first
      // shown match) and we're selecting the previous match, go to the last
      // match in the result.
      previous = maxVisibleIndex;
    } else {
      // The value of -1 for |this.selectedMatchIndex| indicates no selection.
      // Therefore subtract one from the maximum of its value and 0.
      previous = Math.max(this.selectedMatchIndex, 0) - 1;
    }

    this.selectedMatchIndex = remainder(previous, maxVisibleIndex + 1);
  }

  /** Selects the last match. */
  selectLast() {
    this.selectedMatchIndex = this.result ? this.getMaxVisibleIndex_() : -1;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one or no match is currently selected.
   */
  selectNext() {
    if (!this.result) {
      this.selectedMatchIndex = -1;
      return;
    }

    let next;
    const isTypedSuggest = this.lastQueriedInput.trim().length > 0;
    const maxVisibleIndex = this.getMaxVisibleIndex_();
    if (isTypedSuggest && this.selectedMatchIndex === maxVisibleIndex) {
      // Since we're hiding the first match, if we're on the last match and
      // we're selecting the next match, go to the second match (the first shown
      // match).
      next = 1;
    } else {
      next = this.selectedMatchIndex + 1;
    }

    this.selectedMatchIndex = remainder(next, maxVisibleIndex + 1);
  }

  /**
   * @returns Index of the match in the autocomplete result. Passed to the match
   *     so it knows its position in the list of matches.
   */
  protected matchIndex_(match: AutocompleteMatch): number {
    return this.result?.matches.indexOf(match) ?? -1;
  }

  protected isSelected_(match: AutocompleteMatch): boolean {
    return this.matchIndex_(match) === this.selectedMatchIndex;
  }

  /** Returns the maximum index of the visible matches. */
  protected getMaxVisibleIndex_(): number {
    if (!this.result) {
      return -1;
    }

    // Max suggestions is not set, so show all.
    if (!this.maxSuggestions || this.maxSuggestions < 0) {
      return this.result.matches.length - 1;
    }

    // If typeahead is enabled, and the verbatim match is hidden, the
    // maxSuggestions is 1 greater since the verbatim match is not actually
    // visible.
    let maxVisibleSuggestionIndex =
        this.maxSuggestions ? this.maxSuggestions : 0;
    if (!this.hideVerbatimMatch_(0)) {
      // If there is no verbatim match, then the max visible suggestion index is
      // one less than the max suggestions to account for indexing by zero. If
      // the verbatim match is hidden, the the visible matches start at index 1
      // so do not decrement the max visible suggestion index.
      maxVisibleSuggestionIndex--;
    }
    return Math.min(maxVisibleSuggestionIndex, this.result.matches.length - 1);
  }

  /**
   * Returns whether the given index corresponds to the last match.
   */
  protected isLastMatch_(index: number): boolean {
    assert(this.result);
    return index === this.getMaxVisibleIndex_();
  }

  /**
   * Hides the match if its a verbatim match. This match should be hidden
   * for all typed suggestions. It will still be "selected" when the
   * autocomplete result changes, and the user can still navigate to this
   * verbatim match by navigating to the input text. Zero suggest does not
   * have verbatim matches.
   */
  protected hideVerbatimMatch_(index: number): boolean {
    assert(this.result);
    if (!this.result.input) {
      return false;
    }
    return index === 0;
  }

  protected computeAriaLabel_(match: AutocompleteMatch): string {
    return match.a11yLabel;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-dropdown': ComposeboxDropdownElement;
  }
}

customElements.define(ComposeboxDropdownElement.is, ComposeboxDropdownElement);
