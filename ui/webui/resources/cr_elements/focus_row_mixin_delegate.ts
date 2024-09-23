// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FocusRow} from '//resources/js/focus_row.js';
import type {FocusRowDelegate} from '//resources/js/focus_row.js';

interface ListItem {
  lastFocused: HTMLElement|null;
  overrideCustomEquivalent?: boolean;
  getCustomEquivalent?: (el: HTMLElement) => HTMLElement | null;
}

export class FocusRowMixinDelegate implements FocusRowDelegate {
  private listItem_: ListItem;

  constructor(listItem: ListItem) {
    this.listItem_ = listItem;
  }

  /**
   * This function gets called when the [focus-row-control] element receives
   * the focus event.
   */
  onFocus(_row: FocusRow, e: Event) {
    const element = e.composedPath()[0]! as HTMLElement;
    const focusableElement = FocusRow.getFocusableElement(element);
    if (element !== focusableElement) {
      focusableElement.focus();
    }
    this.listItem_.lastFocused = focusableElement;
  }

  /**
   * @param row The row that detected a keydown.
   * @return Whether the event was handled.
   */
  onKeydown(_row: FocusRow, e: KeyboardEvent): boolean {
    // Prevent iron-list from changing the focus on enter.
    if (e.key === 'Enter') {
      e.stopPropagation();
    }

    return false;
  }

  getCustomEquivalent(sampleElement: HTMLElement): HTMLElement|null {
    return this.listItem_.overrideCustomEquivalent ?
        this.listItem_.getCustomEquivalent!(sampleElement) :
        null;
  }
}
