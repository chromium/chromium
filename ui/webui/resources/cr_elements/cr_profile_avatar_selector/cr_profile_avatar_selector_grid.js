// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector-grid' is an accessible control for
 * profile avatar icons that allows keyboard navigation with all arrow keys.
 */

Polymer({
  is: 'cr-profile-avatar-selector-grid',

  properties: {
    ignoreModifiedKeyEvents: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    keydown: 'onKeyDown_',
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    const items = this.querySelectorAll('.avatar');
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        this.moveFocusRow_(items, e.key);
        e.preventDefault();
        return;
      case 'ArrowLeft':
      case 'ArrowRight':
        // Ignores keys likely to be browse shortcuts (like Alt+Left for back).
        if (this.ignoreModifiedKeyEvents && hasKeyModifiers(e)) {
          return;
        }

        this.moveFocusRow_(items, e.key);
        e.preventDefault();
        return;
    }
  },

  /**
   * Moves focus up/down/left/right according to the given direction. Wraps
   * around as necessary.
   * @param {!NodeList<!Element>} items
   * @param {string} direction Must be on of 'ArrowLeft', 'ArrowRight',
   *     'ArrowUp', 'ArrowDown'.
   * @private
   */
  moveFocusRow_(items, direction) {
    let offset =
        (direction === 'ArrowDown' || direction === 'ArrowRight') ? 1 : -1;
    const style = getComputedStyle(this);
    const avatarSpacing =
        parseInt(style.getPropertyValue('--avatar-spacing'), 10);
    const avatarSize = parseInt(style.getPropertyValue('--avatar-size'), 10);
    const rowSize = Math.floor(this.clientWidth / (avatarSpacing + avatarSize));
    const rows = Math.ceil(items.length / rowSize);
    const gridSize = rows * rowSize;

    const focusIndex = Array.prototype.slice.call(items).findIndex(item => {
      return this.parentNode.activeElement === item;
    });

    let nextItem = null;
    if (direction === 'ArrowDown' || direction === 'ArrowUp') {
      for (let i = offset; Math.abs(i) <= rows; i += offset) {
        nextItem = items[(focusIndex + i * rowSize + gridSize) % gridSize];
        if (nextItem) {
          break;
        }
        // This codepath can be hit when |gridSize| is larger than
        // |items.length|, which means that there are empty grid spots at the
        // end.
      }
    } else {
      if (style.direction === 'rtl') {
        offset *= -1;
      }
      let nextIndex = (focusIndex + offset) % items.length;
      if (nextIndex < 0) {
        nextIndex = items.length - 1;
      }
      nextItem = items[nextIndex];
    }

    nextItem.focus();
    assert(this.parentNode.activeElement === nextItem);
  }
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
