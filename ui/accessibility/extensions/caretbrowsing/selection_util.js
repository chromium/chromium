// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of functions for dealing with selections. */
class SelectionUtil {
  /**
   * Get the rectangle for a cursor position. This is tricky because
   * you can't get the bounding rectangle of an empty range, so this function
   * computes the rect by trying a range including one character earlier or
   * later than the cursor position.
   * @param {Cursor} cursor A single cursor position.
   * @return {{left: number, top: number, width: number, height: number}}
   *     The bounding rectangle of the cursor.
   */
  static getCursorRect(cursor) {
    let node = cursor.node;
    const index = cursor.index;
    const rect = {left: 0, top: 0, width: 1, height: 0};
    if (node.constructor == Text) {
      let left = index;
      let right = index;
      const max = node.data.length;
      const newRange = document.createRange();
      while (left > 0 || right < max) {
        if (left > 0) {
          left--;
          newRange.setStart(node, left);
          newRange.setEnd(node, index);
          const rangeRect = newRange.getBoundingClientRect();
          if (rangeRect && rangeRect.width && rangeRect.height) {
            rect.left = rangeRect.right;
            rect.top = rangeRect.top;
            rect.height = rangeRect.height;
            break;
          }
        }
        if (right < max) {
          right++;
          newRange.setStart(node, index);
          newRange.setEnd(node, right);
          const rangeRect = newRange.getBoundingClientRect();
          if (rangeRect && rangeRect.width && rangeRect.height) {
            rect.left = rangeRect.left;
            rect.top = rangeRect.top;
            rect.height = rangeRect.height;
            break;
          }
        }
      }
    } else {
      rect.height = node.offsetHeight;
      while (node !== null) {
        rect.left += node.offsetLeft;
        rect.top += node.offsetTop;
        node = node.offsetParent;
      }
    }
    rect.left += window.pageXOffset;
    rect.top += window.pageYOffset;
    return rect;
  }

  /**
   * Return true if the selection directionality is ambiguous, which happens
   * if, for example, the user double-clicks in the middle of a word to select
   * it. In that case, the selection should extend by the right edge if the
   * user presses right, and by the left edge if the user presses left.
   * @param {Selection} sel The selection.
   * @return {boolean} True if the selection directionality is ambiguous.
   */
  static isAmbiguous(sel) {
    return (
        sel.anchorNode != sel.baseNode || sel.anchorOffset != sel.baseOffset ||
        sel.focusNode != sel.extentNode || sel.focusOffset != sel.extentOffset);
  }

  /**
   * Create a Cursor from the anchor position of the selection, the
   * part that doesn't normally move.
   * @param {Selection} sel The selection.
   * @return {Cursor} A cursor pointing to the selection's anchor location.
   */
  static makeAnchorCursor(sel) {
    return new Cursor(
        sel.anchorNode, sel.anchorOffset,
        TraverseUtil.getNodeText(sel.anchorNode));
  }

  /**
   * Create a Cursor from the focus position of the selection.
   * @param {Selection} sel The selection.
   * @return {Cursor} A cursor pointing to the selection's focus location.
   */
  static makeFocusCursor(sel) {
    return new Cursor(
        sel.focusNode, sel.focusOffset,
        TraverseUtil.getNodeText(sel.focusNode));
  }

  /**
   * Create a Cursor from the left boundary of the selection - the boundary
   * closer to the start of the document.
   * @param {Selection} sel The selection.
   * @return {Cursor} A cursor pointing to the selection's left boundary.
   */
  static makeLeftCursor(sel) {
    const range = sel.rangeCount == 1 ? sel.getRangeAt(0) : null;
    if (range && range.endContainer == sel.anchorNode &&
        range.endOffset == sel.anchorOffset) {
      return SelectionUtil.makeFocusCursor(sel);
    } else {
      return SelectionUtil.makeAnchorCursor(sel);
    }
  }

  /**
   * Create a Cursor from the right boundary of the selection - the boundary
   * closer to the end of the document.
   * @param {Selection} sel The selection.
   * @return {Cursor} A cursor pointing to the selection's right boundary.
   */
  static makeRightCursor(sel) {
    const range = sel.rangeCount == 1 ? sel.getRangeAt(0) : null;
    if (range && range.endContainer == sel.anchorNode &&
        range.endOffset == sel.anchorOffset) {
      return SelectionUtil.makeAnchorCursor(sel);
    } else {
      return SelectionUtil.makeFocusCursor(sel);
    }
  }

  /**
   * Try to set the window's selection to be between the given start and end
   * cursors, and return whether or not it was successful.
   * @param {Cursor} start The start position.
   * @param {Cursor} end The end position.
   * @return {boolean} True if the selection was successfully set.
   */
  static setAndValidateSelection(start, end) {
    const sel = window.getSelection();
    sel.setBaseAndExtent(start.node, start.index, end.node, end.index);

    if (sel.rangeCount != 1) {
      return false;
    }

    return (
        sel.anchorNode == start.node && sel.anchorOffset == start.index &&
        sel.focusNode == end.node && sel.focusOffset == end.index);
  }

  /**
   * Note: the built-in function by the same name is unreliable.
   * @param {Selection} sel The selection.
   * @return {boolean} True if the start and end positions are the same.
   */
  static isCollapsed(sel) {
    return (
        sel.anchorOffset == sel.focusOffset && sel.anchorNode == sel.focusNode);
  }
}
