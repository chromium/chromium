// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {List} from './list.js';

interface DragState {
  /**
   * Target list of drag selection.
   */
  target: List;

  /**
   * Start point of dragging.
   */
  startX: number;

  /**
   * Start point of dragging.
   */
  startY: number;
}

/**
 * Drag selector used on the file list or the grid table.
 */
export class DragSelector {
  /**
   * Stores the current state of the drag selection. Only has a value while the
   * mouse button is down.
   */
  private state_: DragState|null = null;

  /**
   * Border element of drag handle.
   */
  private border_: HTMLDivElement|null = null;

  /**
   * Indexes of selected items by dragging at the last update.
   */
  private lastSelection_: number[] = [];

  /**
   * Indexes of selected items at the start of dragging.
   */
  private originalSelection_: number[] = [];

  // Bind handlers to make them removable.
  private onMouseMoveBound_ = this.onMouseMove_.bind(this);
  private onMouseUpBound_ = this.onMouseUp_.bind(this);

  /**
   * Obtains the scrolled position in the element of mouse pointer from the
   * mouse event.
   *
   * @param element Element that has the scroll bars.
   * @param event The mouse event.
   * @return Scrolled position.
   */
  static getScrolledPosition(element: List, event: MouseEvent):
      {x: number, y: number} {
    if (!element.cachedBounds) {
      element.cachedBounds = element.getBoundingClientRect();
    }
    const rect = element.cachedBounds;
    return {
      x: event.clientX - rect.left + element.scrollLeft,
      y: event.clientY - rect.top + element.scrollTop,
    };
  }

  /**
   * Starts drag selection by reacting dragstart event.
   * This function must be called from handlers of dragstart event.
   *
   * @param list List where the drag selection starts.
   * @param event The dragstart event.
   */
  startDragSelection(list: List, event: MouseEvent) {
    // Precondition check
    if (!list.selectionModel || !list.selectionModel.multiple || this.state_) {
      return;
    }

    // Save the start state.
    const startPos = DragSelector.getScrolledPosition(list, event);
    if (!startPos) {
      return;
    }

    const state = this.state_ = {
      // Set the target of the drag selection
      target: list,
      startX: startPos.x,
      startY: startPos.y,
    };
    this.lastSelection_ = [];
    this.originalSelection_ = list.selectionModel.selectedIndexes;

    // Create and add the border element
    if (!this.border_) {
      this.border_ = state.target.ownerDocument.createElement('div');
      this.border_.className = 'drag-selection-border';
    }
    this.border_.style.left = state.startX + 'px';
    this.border_.style.top = state.startY + 'px';
    this.border_.style.width = '0';
    this.border_.style.height = '0';
    list.appendChild(this.border_);

    // Register event handlers.
    // The handlers are bounded at the constructor.
    state.target.ownerDocument.addEventListener(
        'mousemove', this.onMouseMoveBound_, true);
    state.target.ownerDocument.addEventListener(
        'mouseup', this.onMouseUpBound_, true);
  }

  /**
   * Handles the mousemove event.
   * @param event The mousemove event.
   */
  private onMouseMove_(event: MouseEvent) {
    assert(this.state_);
    const state = this.state_;
    // Get the selection bounds.
    const pos = DragSelector.getScrolledPosition(state.target, event);
    const borderBounds = {
      left: Math.max(Math.min(state.startX, pos.x), 0),
      top: Math.max(Math.min(state.startY, pos.y), 0),
      right: Math.min(Math.max(state.startX, pos.x), state.target.scrollWidth),
      bottom:
          Math.min(Math.max(state.startY, pos.y), state.target.scrollHeight),
      width: 0,
      height: 0,
    };
    borderBounds.width = borderBounds.right - borderBounds.left;
    borderBounds.height = borderBounds.bottom - borderBounds.top;

    // Collect items within the selection rect.
    const currentSelection = state.target.getHitElements(
        borderBounds.left, borderBounds.top, borderBounds.width,
        borderBounds.height);
    const pointedElements = state.target.getHitElements(pos.x, pos.y);
    const leadIndex =
        pointedElements[0] !== undefined ? pointedElements[0] : -1;

    // Diff the selection between currentSelection and this.lastSelection_.
    const selectionFlag: number[] = [];
    for (const index of this.lastSelection_) {
      // Bit operator can be used for undefined value.
      selectionFlag[index] =
          (selectionFlag[index] || 0) | SelectionFlag.IN_LAST_SELECTION;
    }
    for (const index of currentSelection) {
      // Bit operator can be used for undefined value.
      selectionFlag[index] =
          (selectionFlag[index] || 0) | SelectionFlag.IN_CURRENT_SELECTION;
    }

    // Update the selection
    const selectionModel = state.target.selectionModel!;
    selectionModel.beginChange();
    for (const name in selectionFlag) {
      const index = parseInt(name, 10);
      const flag = selectionFlag[index];
      // The flag may be one of following:
      // - IN_LAST_SELECTION | IN_CURRENT_SELECTION
      // - IN_LAST_SELECTION
      // - IN_CURRENT_SELECTION
      // - undefined

      // If the flag equals to (IN_LAST_SELECTION | IN_CURRENT_SELECTION),
      // this is included in both the last selection and the current selection.
      // We have nothing to do for this item.

      if (flag === SelectionFlag.IN_LAST_SELECTION) {
        // If the flag equals to IN_LAST_SELECTION,
        // then the item is included in lastSelection but not in
        // currentSelection. Revert the selection state to
        // this.originalSelection_.
        selectionModel.setIndexSelected(
            index, this.originalSelection_.indexOf(index) !== -1);
      } else if (flag === SelectionFlag.IN_CURRENT_SELECTION) {
        // If the flag equals to IN_CURRENT_SELECTION,
        // this is included in currentSelection but not in lastSelection.
        selectionModel.setIndexSelected(index, true);
      }
    }
    if (leadIndex !== -1) {
      selectionModel.leadIndex = leadIndex;
      selectionModel.anchorIndex = leadIndex;
    }
    selectionModel.endChange();
    this.lastSelection_ = currentSelection;

    // Update the size of border
    assert(this.border_);
    this.border_.style.left = borderBounds.left + 'px';
    this.border_.style.top = borderBounds.top + 'px';
    this.border_.style.width = borderBounds.width + 'px';
    this.border_.style.height = borderBounds.height + 'px';
  }

  /**
   * Handle the mouseup event.
   * @param event The mouseup event.
   */
  private onMouseUp_(event: MouseEvent) {
    assert(this.border_);
    assert(this.state_);
    this.onMouseMove_(event);
    this.state_.target.removeChild(this.border_);
    this.state_.target.ownerDocument.removeEventListener(
        'mousemove', this.onMouseMoveBound_, true);
    this.state_.target.ownerDocument.removeEventListener(
        'mouseup', this.onMouseUpBound_, true);
    this.state_.target.cachedBounds = null;
    this.state_ = null;
    // The target may select an item by reacting to the mouseup event.
    // This suppress to the selecting behavior.
    event.stopPropagation();
  }
}

/**
 * Flag that shows whether the item is included in the selection or not.
 */
enum SelectionFlag {
  IN_LAST_SELECTION = 1 << 0,
  IN_CURRENT_SELECTION = 1 << 1,
}
