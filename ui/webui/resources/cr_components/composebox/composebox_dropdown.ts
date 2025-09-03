// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox_match.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_dropdown.css.js';
import {getHtml} from './composebox_dropdown.html.js';

// The '%' operator in JS returns negative numbers. This workaround avoids that.
const remainder = (lhs: number, rhs: number) => ((lhs % rhs) + rhs) % rhs;

// TODO(crbug.com/439616869): Provide an API for the embedder (i.e., <cr-composebox>)
// to change the selection.
// A dropdown element that contains autocomplete matches.
export class ComposeboxDropdownElement extends CrLitElement {
  static get is() {
    return 'ntp-composebox-dropdown';
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
    };
  }

  accessor result: AutocompleteResult|null = null;
  accessor selectedMatchIndex: number;

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
        this.shadowRoot.querySelectorAll('ntp-composebox-match');
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
    // The value of -1 for |this.selectedMatchIndex| indicates no selection.
    // Therefore subtract one from the maximum of its value and 0.
    const previous = Math.max(this.selectedMatchIndex, 0) - 1;
    this.selectedMatchIndex =
        remainder(previous, this.result?.matches?.length!);
  }

  /** Selects the last match. */
  selectLast() {
    this.selectedMatchIndex = this.result?.matches?.length! - 1;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one or no match is currently selected.
   */
  selectNext() {
    const next = this.selectedMatchIndex + 1;
    this.selectedMatchIndex = remainder(next, this.result?.matches?.length!);
  }

  /**
   * @returns Index of the match in the autocomplete result. Passed to the match
   *     so it knows its position in the list of matches.
   */
  protected matchIndex_(match: AutocompleteMatch): number {
    return this.result?.matches?.indexOf(match) ?? -1;
  }

  protected isSelected_(match: AutocompleteMatch): boolean {
    return this.matchIndex_(match) === this.selectedMatchIndex;
  }

  /**
   * Returns whether the given index corresponds to the last match.
   */
  protected isLastMatch_(index: number): boolean {
    return index === this.result?.matches?.length! - 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox-dropdown': ComposeboxDropdownElement;
  }
}

customElements.define(ComposeboxDropdownElement.is, ComposeboxDropdownElement);
