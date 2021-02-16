// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assert} from 'chrome://resources/js/assert.m.js'
// #import {FocusRow, FocusRowDelegate} from 'chrome://resources/js/cr/ui/focus_row.m.js';
// clang-format on

cr.define('cr.ui', function() {
  /**
   * A class to manage grid of focusable elements in a 2D grid. For example,
   * given this grid:
   *
   *   focusable  [focused]  focusable  (row: 0, col: 1)
   *   focusable  focusable  focusable
   *   focusable  focusable  focusable
   *
   * Pressing the down arrow would result in the focus moving down 1 row and
   * keeping the same column:
   *
   *   focusable  focusable  focusable
   *   focusable  [focused]  focusable  (row: 1, col: 1)
   *   focusable  focusable  focusable
   *
   * And pressing right or tab at this point would move the focus to:
   *
   *   focusable  focusable  focusable
   *   focusable  focusable  [focused]  (row: 1, col: 2)
   *   focusable  focusable  focusable
   *
   * @implements {cr.ui.FocusRowDelegate}
   */
  /* #export */ class FocusGrid {
    constructor() {
      /** @type {!Array<!cr.ui.FocusRow>} */
      this.rows = [];

      /** @private {boolean} */
      this.ignoreFocusChange_ = false;

      /** @private {?EventTarget} */
      this.lastFocused_ = null;
    }

    /** @override */
    onFocus(row, e) {
      if (this.ignoreFocusChange_) {
        this.ignoreFocusChange_ = false;
      } else {
        this.lastFocused_ = e.currentTarget;
      }

      this.rows.forEach(function(r) {
        r.makeActive(r === row);
      });
    }

    /** @override */
    onKeydown(row, e) {
      const rowIndex = this.rows.indexOf(row);
      assert(rowIndex >= 0);

      let newRow = -1;

      if (e.key === 'ArrowUp') {
        newRow = rowIndex - 1;
      } else if (e.key === 'ArrowDown') {
        newRow = rowIndex + 1;
      } else if (e.key === 'PageUp') {
        newRow = 0;
      } else if (e.key === 'PageDown') {
        newRow = this.rows.length - 1;
      }

      const rowToFocus = this.rows[newRow];
      if (rowToFocus) {
        this.ignoreFocusChange_ = true;
        rowToFocus
            .getEquivalentElement(
                /** @type {!Element} */ (this.lastFocused_))
            .focus();
        e.preventDefault();
        return true;
      }

      return false;
    }

    /** @override */
    getCustomEquivalent(sampleElement) {
      return null;
    }

    /**
     * Unregisters event handlers and removes all |this.rows|.
     */
    destroy() {
      this.rows.forEach(function(row) {
        row.destroy();
      });
      this.rows.length = 0;
    }

    /**
     * @param {!Element} target A target item to find in this grid.
     * @return {number} The row index. -1 if not found.
     */
    getRowIndexForTarget(target) {
      for (let i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].getElements().indexOf(target) >= 0) {
          return i;
        }
      }
      return -1;
    }

    /**
     * @param {Element} root An element to search for.
     * @return {?cr.ui.FocusRow} The row with root of |root| or null.
     */
    getRowForRoot(root) {
      for (let i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].root === root) {
          return this.rows[i];
        }
      }
      return null;
    }

    /**
     * Adds |row| to the end of this list.
     * @param {!cr.ui.FocusRow} row The row that needs to be added to this grid.
     */
    addRow(row) {
      this.addRowBefore(row, null);
    }

    /**
     * Adds |row| before |nextRow|. If |nextRow| is not in the list or it's
     * null, |row| is added to the end.
     * @param {!cr.ui.FocusRow} row The row that needs to be added to this grid.
     * @param {cr.ui.FocusRow} nextRow The row that should follow |row|.
     */
    addRowBefore(row, nextRow) {
      row.delegate = row.delegate || this;

      const nextRowIndex = nextRow ? this.rows.indexOf(nextRow) : -1;
      if (nextRowIndex === -1) {
        this.rows.push(row);
      } else {
        this.rows.splice(nextRowIndex, 0, row);
      }
    }

    /**
     * Removes a row from the focus row. No-op if row is not in the grid.
     * @param {cr.ui.FocusRow} row The row that needs to be removed.
     */
    removeRow(row) {
      const nextRowIndex = row ? this.rows.indexOf(row) : -1;
      if (nextRowIndex > -1) {
        this.rows.splice(nextRowIndex, 1);
      }
    }

    /**
     * Makes sure that at least one row is active. Should be called once, after
     * adding all rows to FocusGrid.
     * @param {number=} preferredRow The row to select if no other row is
     *     active. Selects the first item if this is beyond the range of the
     *     grid.
     */
    ensureRowActive(preferredRow) {
      if (this.rows.length === 0) {
        return;
      }

      for (let i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].isActive()) {
          return;
        }
      }

      (this.rows[preferredRow || 0] || this.rows[0]).makeActive(true);
    }
  }

  // #cr_define_end
  console.warn('crbug/1173575, non-JS module files deprecated.');
  return {
    FocusGrid: FocusGrid,
  };
});
