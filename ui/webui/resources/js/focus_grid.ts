// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from './assert.js';
import type {FocusRow, FocusRowDelegate} from './focus_row.js';
// clang-format on

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
 */
export class FocusGrid implements FocusRowDelegate {
  rows: FocusRow[] = [];
  private ignoreFocusChange_: boolean = false;
  private lastFocused_: EventTarget|null = null;

  onFocus(row: FocusRow, e: Event) {
    if (this.ignoreFocusChange_) {
      this.ignoreFocusChange_ = false;
    } else {
      this.lastFocused_ = e.currentTarget;
    }

    this.rows.forEach(function(r) {
      r.makeActive(r === row);
    });
  }

  onKeydown(row: FocusRow, e: KeyboardEvent) {
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
      rowToFocus.getEquivalentElement(this.lastFocused_ as HTMLElement).focus();
      e.preventDefault();
      return true;
    }

    return false;
  }

  getCustomEquivalent(_sampleElement: HTMLElement) {
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
   * @param target A target item to find in this grid.
   * @return The row index. -1 if not found.
   */
  getRowIndexForTarget(target: HTMLElement): number {
    for (let i = 0; i < this.rows.length; ++i) {
      if (this.rows[i]!.getElements().indexOf(target) >= 0) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @param root An element to search for.
   * @return The row with root of |root| or null.
   */
  getRowForRoot(root: HTMLElement): FocusRow|null {
    for (let i = 0; i < this.rows.length; ++i) {
      if (this.rows[i]!.root === root) {
        return this.rows[i]!;
      }
    }
    return null;
  }

  /**
   * Adds |row| to the end of this list.
   * @param row The row that needs to be added to this grid.
   */
  addRow(row: FocusRow) {
    this.addRowBefore(row, null);
  }

  /**
   * Adds |row| before |nextRow|. If |nextRow| is not in the list or it's
   * null, |row| is added to the end.
   * @param row The row that needs to be added to this grid.
   * @param nextRow The row that should follow |row|.
   */
  addRowBefore(row: FocusRow, nextRow: FocusRow|null) {
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
   * @param row The row that needs to be removed.
   */
  removeRow(row: FocusRow|null) {
    const nextRowIndex = row ? this.rows.indexOf(row) : -1;
    if (nextRowIndex > -1) {
      this.rows.splice(nextRowIndex, 1);
    }
  }

  /**
   * Makes sure that at least one row is active. Should be called once, after
   * adding all rows to FocusGrid.
   * @param preferredRow The row to select if no other row is
   *     active. Selects the first item if this is beyond the range of the
   *     grid.
   */
  ensureRowActive(preferredRow?: number) {
    if (this.rows.length === 0) {
      return;
    }

    for (let i = 0; i < this.rows.length; ++i) {
      if (this.rows[i]!.isActive()) {
        return;
      }
    }

    (this.rows[preferredRow || 0] || this.rows[0]!).makeActive(true);
  }
}
