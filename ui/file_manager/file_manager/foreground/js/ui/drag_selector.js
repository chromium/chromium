// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {List} from './list.js';

export class DragSelector {
  /**
   * Drag selector used on the file list or the grid table.
   */
  constructor() {
    /**
     * Target list of drag selection.
     * @type {List}
     * @private
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'List'.
    this.target_ = null;

    /**
     * Border element of drag handle.
     * @type {Element}
     * @private
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'Element'.
    this.border_ = null;

    /**
     * Start point of dragging.
     * @type {number?}
     * @private
     */
    this.startX_ = null;

    /**
     * Start point of dragging.
     * @type {number?}
     * @private
     */
    this.startY_ = null;

    /**
     * Indexes of selected items by dragging at the last update.
     * @type {Array<number>!}
     * @private
     */
    this.lastSelection_ = [];

    /**
     * Indexes of selected items at the start of dragging.
     * @type {Array<number>!}
     * @private
     */
    this.originalSelection_ = [];

    // Bind handlers to make them removable.
    this.onMouseMoveBound_ = this.onMouseMove_.bind(this);
    this.onMouseUpBound_ = this.onMouseUp_.bind(this);
  }

  /**
   * Obtains the scrolled position in the element of mouse pointer from the
   * mouse event.
   *
   * @param {HTMLElement} element Element that has the scroll bars.
   * @param {MouseEvent} event The mouse event.
   * @return {?{x:number, y:number}} Scrolled position.
   */
  static getScrolledPosition(element, event) {
    // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on type
    // 'HTMLElement'.
    if (!element.cachedBounds) {
      // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on
      // type 'HTMLElement'.
      element.cachedBounds = element.getBoundingClientRect();
      // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on
      // type 'HTMLElement'.
      if (!element.cachedBounds) {
        return null;
      }
    }
    // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on type
    // 'HTMLElement'.
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
   * @this {DragSelector}
   * @param {List} list List where the drag selection starts.
   * @param {MouseEvent} event The dragstart event.
   */
  startDragSelection(list, event) {
    // Precondition check
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type 'List'. Did you mean 'selectionModel'?
    if (!list.selectionModel_.multiple || this.target_) {
      return;
    }

    // Set the target of the drag selection
    this.target_ = list;

    // Save the start state.
    const startPos = DragSelector.getScrolledPosition(list, event);
    if (!startPos) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'x' does not exist on type 'Object'.
    this.startX_ = startPos.x;
    // @ts-ignore: error TS2339: Property 'y' does not exist on type 'Object'.
    this.startY_ = startPos.y;
    this.lastSelection_ = [];
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type 'List'. Did you mean 'selectionModel'?
    this.originalSelection_ = this.target_.selectionModel_.selectedIndexes;

    // Create and add the border element
    if (!this.border_) {
      this.border_ = this.target_.ownerDocument.createElement('div');
      this.border_.className = 'drag-selection-border';
    }
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.left = this.startX_ + 'px';
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.top = this.startY_ + 'px';
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.width = '0';
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.height = '0';
    list.appendChild(this.border_);

    // Register event handlers.
    // The handlers are bounded at the constructor.
    this.target_.ownerDocument.addEventListener(
        'mousemove', this.onMouseMoveBound_, true);
    this.target_.ownerDocument.addEventListener(
        'mouseup', this.onMouseUpBound_, true);
  }

  /**
   * Handles the mousemove event.
   * @private
   * @param {MouseEvent} event The mousemove event.
   */
  onMouseMove_(event) {
    event = /** @type {MouseEvent} */ (event);
    // Get the selection bounds.
    const pos = DragSelector.getScrolledPosition(this.target_, event);
    const borderBounds = {
      // @ts-ignore: error TS2339: Property 'x' does not exist on type 'Object'.
      left: Math.max(Math.min(this.startX_, pos.x), 0),
      // @ts-ignore: error TS2339: Property 'y' does not exist on type 'Object'.
      top: Math.max(Math.min(this.startY_, pos.y), 0),
      // @ts-ignore: error TS2339: Property 'x' does not exist on type 'Object'.
      right: Math.min(Math.max(this.startX_, pos.x), this.target_.scrollWidth),
      bottom: Math.min(
          // @ts-ignore: error TS2339: Property 'y' does not exist on type
          // 'Object'.
          Math.max(this.startY_, pos.y), this.target_.scrollHeight),
    };
    // @ts-ignore: error TS2339: Property 'width' does not exist on type '{
    // left: number; top: number; right: number; bottom: number; }'.
    borderBounds.width = borderBounds.right - borderBounds.left;
    // @ts-ignore: error TS2339: Property 'height' does not exist on type '{
    // left: number; top: number; right: number; bottom: number; }'.
    borderBounds.height = borderBounds.bottom - borderBounds.top;

    // Collect items within the selection rect.
    const currentSelection =
        // @ts-ignore: error TS2304: Cannot find name 'DragTarget'.
        (/** @type {DragTarget} */ (this.target_))
            .getHitElements(
                borderBounds.left, borderBounds.top,
                // @ts-ignore: error TS2339: Property 'height' does not exist on
                // type '{ left: number; top: number; right: number; bottom:
                // number; }'.
                borderBounds.width, borderBounds.height);
    const pointedElements =
        // @ts-ignore: error TS2339: Property 'y' does not exist on type
        // 'Object'.
        (/** @type {DragTarget} */ (this.target_)).getHitElements(pos.x, pos.y);
    const leadIndex = pointedElements.length ? pointedElements[0] : -1;

    // Diff the selection between currentSelection and this.lastSelection_.
    // @ts-ignore: error TS7034: Variable 'selectionFlag' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
    const selectionFlag = [];
    for (let i = 0; i < this.lastSelection_.length; i++) {
      const index = this.lastSelection_[i];
      // Bit operator can be used for undefined value.
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      selectionFlag[index] =
          // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an
          // index type.
          selectionFlag[index] | SelectionFlag_.IN_LAST_SELECTION;
    }
    for (let i = 0; i < currentSelection.length; i++) {
      const index = currentSelection[i];
      // Bit operator can be used for undefined value.
      selectionFlag[index] =
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          selectionFlag[index] | SelectionFlag_.IN_CURRENT_SELECTION;
    }

    // Update the selection
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type 'List'. Did you mean 'selectionModel'?
    this.target_.selectionModel_.beginChange();
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

      if (flag == SelectionFlag_.IN_LAST_SELECTION) {
        // If the flag equals to IN_LAST_SELECTION,
        // then the item is included in lastSelection but not in
        // currentSelection. Revert the selection state to
        // this.originalSelection_.
        // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist
        // on type 'List'. Did you mean 'selectionModel'?
        this.target_.selectionModel_.setIndexSelected(
            index, this.originalSelection_.indexOf(index) != -1);
      } else if (flag == SelectionFlag_.IN_CURRENT_SELECTION) {
        // If the flag equals to IN_CURRENT_SELECTION,
        // this is included in currentSelection but not in lastSelection.
        // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist
        // on type 'List'. Did you mean 'selectionModel'?
        this.target_.selectionModel_.setIndexSelected(index, true);
      }
    }
    if (leadIndex != -1) {
      // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
      // type 'List'. Did you mean 'selectionModel'?
      this.target_.selectionModel_.leadIndex = leadIndex;
      // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
      // type 'List'. Did you mean 'selectionModel'?
      this.target_.selectionModel_.anchorIndex = leadIndex;
    }
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type 'List'. Did you mean 'selectionModel'?
    this.target_.selectionModel_.endChange();
    this.lastSelection_ = currentSelection;

    // Update the size of border
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.left = borderBounds.left + 'px';
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.border_.style.top = borderBounds.top + 'px';
    // @ts-ignore: error TS2339: Property 'width' does not exist on type '{
    // left: number; top: number; right: number; bottom: number; }'.
    this.border_.style.width = borderBounds.width + 'px';
    // @ts-ignore: error TS2339: Property 'height' does not exist on type '{
    // left: number; top: number; right: number; bottom: number; }'.
    this.border_.style.height = borderBounds.height + 'px';
  }

  /**
   * Handle the mouseup event.
   * @private
   * @param {MouseEvent} event The mouseup event.
   */
  onMouseUp_(event) {
    this.onMouseMove_(event);
    this.target_.removeChild(this.border_);
    this.target_.ownerDocument.removeEventListener(
        'mousemove', this.onMouseMoveBound_, true);
    this.target_.ownerDocument.removeEventListener(
        'mouseup', this.onMouseUpBound_, true);
    // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on type
    // 'List'.
    this.target_.cachedBounds = null;
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'List'.
    this.target_ = null;
    // The target may select an item by reacting to the mouseup event.
    // This suppress to the selecting behavior.
    event.stopPropagation();
  }
}

/**
 * Flag that shows whether the item is included in the selection or not.
 * @enum {number}
 */
const SelectionFlag_ = {
  IN_LAST_SELECTION: 1 << 0,
  IN_CURRENT_SELECTION: 1 << 1,
};
