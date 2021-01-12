// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';

/** @extends {HTMLElement} */
export class CrMenuSelector extends HTMLElement {
  static get is() {
    return 'cr-menu-selector';
  }

  constructor() {
    super();

    /** @private {!FocusOutlineManager} */
    this.focusOutlineManager_;

    this.addEventListener(
        'focusin', e => this.onFocusin_(/** @type {!FocusEvent} */ (e)));
    this.addEventListener(
        'keydown', e => this.onKeydown_(/** @type {!KeyboardEvent} */ (e)));
  }

  connectedCallback() {
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
    this.setAttribute('role', 'menu');
  }

  /**
   * @return {!Array<!HTMLElement>}
   * @private
   */
  getItems_() {
    return /** @type {!Array<!HTMLElement>} */ (
        Array.from(this.querySelectorAll(
            '[role=menuitem]:not([disabled]):not([hidden])')));
  }

  /**
   * @param {!FocusEvent} e
   * @private
   */
  onFocusin_(e) {
    // If the focus was moved by keyboard and is coming in from a relatedTarget
    // that is not within this menu, move the focus to the first menu item. This
    // ensures that the first menu item is always the first focused item when
    // focusing into the menu. A null relatedTarget means the focus was moved
    // from outside the WebContents.
    const focusMovedWithKeyboard = this.focusOutlineManager_.visible;
    const focusMovedFromOutside = e.relatedTarget === null ||
        !this.contains(/** @type {!HTMLElement} */ (e.relatedTarget));
    if (focusMovedWithKeyboard && focusMovedFromOutside) {
      this.getItems_()[0].focus();
    }
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    const items = this.getItems_();
    assert(items.length >= 1);
    const currentFocusedIndex = items.indexOf(
        /** @type {!HTMLElement} */ (this.querySelector(':focus')));

    let newFocusedIndex = currentFocusedIndex;
    switch (event.key) {
      case 'Tab':
        if (event.shiftKey) {
          // If pressing Shift+Tab, immediately focus the first element so that
          // when the event is finished processing, the browser automatically
          // focuses the previous focusable element outside of the menu.
          items[0].focus();
        } else {
          // If pressing Tab, immediately focus the last element so that when
          // the event is finished processing, the browser automatically focuses
          // the next focusable element outside of the menu.
          items[items.length - 1].focus();
        }
        return;
      case 'ArrowDown':
        newFocusedIndex = (currentFocusedIndex + 1) % items.length;
        break;
      case 'ArrowUp':
        newFocusedIndex =
            (currentFocusedIndex + items.length - 1) % items.length;
        break;
      case 'Home':
        newFocusedIndex = 0;
        break;
      case 'End':
        newFocusedIndex = items.length - 1;
        break;
    }

    if (newFocusedIndex === currentFocusedIndex) {
      return;
    }

    event.preventDefault();
    items[newFocusedIndex].focus();
  }
}

customElements.define(CrMenuSelector.is, CrMenuSelector);