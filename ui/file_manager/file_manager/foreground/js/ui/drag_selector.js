// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class DragSelector {
  /**
   * Drag selector used on the file list or the grid table.
   */
  constructor() {
    /**
     * Target list of drag selection.
     * @type {cr.ui.List}
     * @private
     */
    this.target_ = null;

    /**
     * Border element of drag handle.
     * @type {Element}
     * @private
     */
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
   * @param {Event} event The mouse event.
   * @return {Object} Scrolled position.
   */
  static getScrolledPosition(element, event) {
    if (!element.cachedBounds) {
      element.cachedBounds = element.getBoundingClientRect();
      if (!element.cachedBounds) {
        return null;
      }
    }
    const rect = element.cachedBounds;
    return {
      x: event.clientX - rect.left + element.scrollLeft,
      y: event.clientY - rect.top + element.scrollTop
    };
  }

  /**
   * Starts drag selection by reacting dragstart event.
   * This function must be called from handlers of dragstart event.
   *
   * @this {DragSelector}
   * @param {cr.ui.List} list List where the drag selection starts.
   * @param {Event} event The dragstart event.
   */
  startDragSelection(list, event) {
    // Precondition check
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
    this.startX_ = startPos.x;
    this.startY_ = startPos.y;
    this.lastSelection_ = [];
    this.originalSelection_ = this.target_.selectionModel_.selectedIndexes;

    // Create and add the border element
    if (!this.border_) {
      this.border_ = this.target_.ownerDocument.createElement('div');
      this.border_.className = 'drag-selection-border';
    }
    this.border_.style.left = this.startX_ + 'px';
    this.border_.style.top = this.startY_ + 'px';
    this.border_.style.width = '0';
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
   * @param {Event} event The mousemove event.
   */
  onMouseMove_(event) {
    event = /** @type {MouseEvent} */ (event);
    // Get the selection bounds.
    const pos = DragSelector.getScrolledPosition(this.target_, event);
    const borderBounds = {
      left: Math.max(Math.min(this.startX_, pos.x), 0),
      top: Math.max(Math.min(this.startY_, pos.y), 0),
      right: Math.min(Math.max(this.startX_, pos.x), this.target_.scrollWidth),
      bottom: Math.min(Math.max(this.startY_, pos.y), this.target_.scrollHeight)
    };
    borderBounds.width = borderBounds.right - borderBounds.left;
    borderBounds.height = borderBounds.bottom - borderBounds.top;

    // Collect items within the selection rect.
    const currentSelection = (/** @type {DragTarget} */ (this.target_))
                                 .getHitElements(
                                     borderBounds.left, borderBounds.top,
                                     borderBounds.width, borderBounds.height);
    const pointedElements =
        (/** @type {DragTarget} */ (this.target_)).getHitElements(pos.x, pos.y);
    const leadIndex = pointedElements.length ? pointedElements[0] : -1;

    // Diff the selection between currentSelection and this.lastSelection_.
    const selectionFlag = [];
    for (let i = 0; i < this.lastSelection_.length; i++) {
      const index = this.lastSelection_[i];
      // Bit operator can be used for undefined value.
      selectionFlag[index] =
          selectionFlag[index] | DragSelector.SelectionFlag_.IN_LAST_SELECTION;
    }
    for (let i = 0; i < currentSelection.length; i++) {
      const index = currentSelection[i];
      // Bit operator can be used for undefined value.
      selectionFlag[index] = selectionFlag[index] |
          DragSelector.SelectionFlag_.IN_CURRENT_SELECTION;
    }

    // Update the selection
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

      if (flag == DragSelector.SelectionFlag_.IN_LAST_SELECTION) {
        // If the flag equals to IN_LAST_SELECTION,
        // then the item is included in lastSelection but not in
        // currentSelection. Revert the selection state to
        // this.originalSelection_.
        this.target_.selectionModel_.setIndexSelected(
            index, this.originalSelection_.indexOf(index) != -1);
      } else if (flag == DragSelector.SelectionFlag_.IN_CURRENT_SELECTION) {
        // If the flag equals to IN_CURRENT_SELECTION,
        // this is included in currentSelection but not in lastSelection.
        this.target_.selectionModel_.setIndexSelected(index, true);
      }
    }
    if (leadIndex != -1) {
      this.target_.selectionModel_.leadIndex = leadIndex;
      this.target_.selectionModel_.anchorIndex = leadIndex;
    }
    this.target_.selectionModel_.endChange();
    this.lastSelection_ = currentSelection;

    // Update the size of border
    this.border_.style.left = borderBounds.left + 'px';
    this.border_.style.top = borderBounds.top + 'px';
    this.border_.style.width = borderBounds.width + 'px';
    this.border_.style.height = borderBounds.height + 'px';
  }

  /**
   * Handle the mouseup event.
   * @private
   * @param {Event} event The mouseup event.
   */
  onMouseUp_(event) {
    event = /** @type {MouseEvent} */ (event);
    this.onMouseMove_(event);
    this.target_.removeChild(this.border_);
    this.target_.ownerDocument.removeEventListener(
        'mousemove', this.onMouseMoveBound_, true);
    this.target_.ownerDocument.removeEventListener(
        'mouseup', this.onMouseUpBound_, true);
    this.target_.cachedBounds = null;
    this.target_ = null;
    // The target may select an item by reacting to the mouseup event.
    // This suppress to the selecting behavior.
    event.stopPropagation();
  }
}

/**
 * Flag that shows whether the item is included in the selection or not.
 * @enum {number}
 * @private
 */
DragSelector.SelectionFlag_ = {
  IN_LAST_SELECTION: 1 << 0,
  IN_CURRENT_SELECTION: 1 << 1
};
