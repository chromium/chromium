// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * Creates a selection controller that is to be used with lists. This is
   * implemented for vertical lists but changing the behavior for horizontal
   * lists or icon views is a matter of overriding {@code getIndexBefore},
   * {@code getIndexAfter}, {@code getIndexAbove} as well as
   * {@code getIndexBelow}.
   *
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   *
   * @constructor
   */
  function ListSelectionController(selectionModel) {
    this.selectionModel_ = selectionModel;
  }

  ListSelectionController.prototype = {

    /**
     * The selection model we are interacting with.
     * @type {cr.ui.ListSelectionModel}
     */
    get selectionModel() {
      return this.selectionModel_;
    },

    /**
     * Returns the index below (y axis) the given element.
     * @param {number} index The index to get the index below.
     * @return {number} The index below or -1 if not found.
     */
    getIndexBelow: function(index) {
      if (index == this.getLastIndex()) {
        return -1;
      }
      return index + 1;
    },

    /**
     * Returns the index above (y axis) the given element.
     * @param {number} index The index to get the index above.
     * @return {number} The index below or -1 if not found.
     */
    getIndexAbove: function(index) {
      return index - 1;
    },

    /**
     * Returns the index before (x axis) the given element. This returns -1
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {number} index The index to get the index before.
     * @return {number} The index before or -1 if not found.
     */
    getIndexBefore: function(index) {
      return -1;
    },

    /**
     * Returns the index after (x axis) the given element. This returns -1
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {number} index The index to get the index after.
     * @return {number} The index after or -1 if not found.
     */
    getIndexAfter: function(index) {
      return -1;
    },

    /**
     * Returns the next list index. This is the next logical and should not
     * depend on any kind of layout of the list.
     * @param {number} index The index to get the next index for.
     * @return {number} The next index or -1 if not found.
     */
    getNextIndex: function(index) {
      if (index == this.getLastIndex()) {
        return -1;
      }
      return index + 1;
    },

    /**
     * Returns the prevous list index. This is the previous logical and should
     * not depend on any kind of layout of the list.
     * @param {number} index The index to get the previous index for.
     * @return {number} The previous index or -1 if not found.
     */
    getPreviousIndex: function(index) {
      return index - 1;
    },

    /**
     * @return {number} The first index.
     */
    getFirstIndex: function() {
      return 0;
    },

    /**
     * @return {number} The last index.
     */
    getLastIndex: function() {
      return this.selectionModel.length - 1;
    },

    /**
     * Called by the view when the user does a mousedown or mouseup on the
     * list.
     * @param {!Event} e The browser mouse event.
     * @param {number} index The index that was under the mouse pointer, -1 if
     *     none.
     */
    handlePointerDownUp: function(e, index) {
      const sm = this.selectionModel;
      const anchorIndex = sm.anchorIndex;
      const isDown = (e.type == 'mousedown');

      sm.beginChange();

      if (index == -1) {
        // On Mac we always clear the selection if the user clicks a blank area.
        // On Windows, we only clear the selection if neither Shift nor Ctrl are
        // pressed.
        if (cr.isMac || cr.isChromeOS) {
          sm.leadIndex = sm.anchorIndex = -1;
          sm.unselectAll();
        } else if (!isDown && !e.shiftKey && !e.ctrlKey) {
          // Keep anchor and lead indexes. Note that this is intentionally
          // different than on the Mac.
          if (sm.multiple) {
            sm.unselectAll();
          }
        }
      } else {
        if (sm.multiple &&
            (cr.isMac ? e.metaKey : (e.ctrlKey && !e.shiftKey))) {
          // Selection is handled at mouseUp on windows/linux, mouseDown on mac.
          if (cr.isMac ? isDown : !isDown) {
            // Toggle the current one and make it anchor index.
            sm.setIndexSelected(index, !sm.getIndexSelected(index));
            sm.leadIndex = index;
            sm.anchorIndex = index;
          }
        } else if (e.shiftKey && anchorIndex != -1 && anchorIndex != index) {
          // Shift is done in mousedown.
          if (isDown) {
            sm.unselectAll();
            sm.leadIndex = index;
            if (sm.multiple) {
              sm.selectRange(anchorIndex, index);
            } else {
              sm.setIndexSelected(index, true);
            }
          }
        } else {
          // Right click for a context menu needs to not clear the selection.
          const isRightClick = e.button == 2;

          // If the index is selected this is handled in mouseup.
          const indexSelected = sm.getIndexSelected(index);
          if ((indexSelected && !isDown || !indexSelected && isDown) &&
              !(indexSelected && isRightClick)) {
            sm.selectedIndex = index;
          }
        }
      }

      sm.endChange();
    },

    /**
     * Called by the view when it receives either a touchstart, touchmove,
     * touchend, or touchcancel event.
     * Sub-classes may override this function to handle touch events separately
     * from mouse events, instead of waiting for emulated mouse events sent
     * after the touch events.
     * @param {Event} e The event.
     * @param {number} index The index that was under the touched point, -1 if
     *     none.
     */
    handleTouchEvents: function(e, index) {
      // Do nothing.
    },

    /**
     * Called by the view when it receives a keydown event.
     * @param {Event} e The keydown event.
     */
    handleKeyDown: function(e) {
      const tagName = e.target.tagName;
      // If focus is in an input field of some kind, only handle navigation keys
      // that aren't likely to conflict with input interaction (e.g., text
      // editing, or changing the value of a checkbox or select).
      if (tagName == 'INPUT') {
        const inputType = e.target.type;
        // Just protect space (for toggling) for checkbox and radio.
        if (inputType == 'checkbox' || inputType == 'radio') {
          if (e.key == ' ') {
            return;
          }
          // Protect all but the most basic navigation commands in anything
          // else.
        } else if (e.key != 'ArrowUp' && e.key != 'ArrowDown') {
          return;
        }
      }
      // Similarly, don't interfere with select element handling.
      if (tagName == 'SELECT') {
        return;
      }

      const sm = this.selectionModel;
      let newIndex = -1;
      const leadIndex = sm.leadIndex;
      let prevent = true;

      // Ctrl/Meta+A
      if (sm.multiple && e.keyCode == 65 &&
          (cr.isMac && e.metaKey || !cr.isMac && e.ctrlKey)) {
        sm.selectAll();
        e.preventDefault();
        return;
      }

      if (e.key == ' ') {
        if (leadIndex != -1) {
          const selected = sm.getIndexSelected(leadIndex);
          if (e.ctrlKey || !selected) {
            sm.setIndexSelected(leadIndex, !selected || !sm.multiple);
            return;
          }
        }
      }

      switch (e.key) {
        case 'Home':
          newIndex = this.getFirstIndex();
          break;
        case 'End':
          newIndex = this.getLastIndex();
          break;
        case 'ArrowUp':
          newIndex = leadIndex == -1 ? this.getLastIndex() :
                                       this.getIndexAbove(leadIndex);
          break;
        case 'ArrowDown':
          newIndex = leadIndex == -1 ? this.getFirstIndex() :
                                       this.getIndexBelow(leadIndex);
          break;
        case 'ArrowLeft':
        case 'MediaPreviousTrack':
          newIndex = leadIndex == -1 ? this.getLastIndex() :
                                       this.getIndexBefore(leadIndex);
          break;
        case 'ArrowRight':
        case 'MediaNextTrack':
          newIndex = leadIndex == -1 ? this.getFirstIndex() :
                                       this.getIndexAfter(leadIndex);
          break;
        default:
          prevent = false;
      }

      if (newIndex != -1) {
        sm.beginChange();

        sm.leadIndex = newIndex;
        if (e.shiftKey) {
          const anchorIndex = sm.anchorIndex;
          if (sm.multiple) {
            sm.unselectAll();
          }
          if (anchorIndex == -1) {
            sm.setIndexSelected(newIndex, true);
            sm.anchorIndex = newIndex;
          } else {
            sm.selectRange(anchorIndex, newIndex);
          }
        } else if (e.ctrlKey && !cr.isMac && !cr.isChromeOS) {
          // Setting the lead index is done above.
          // Mac does not allow you to change the lead.
        } else {
          if (sm.multiple) {
            sm.unselectAll();
          }
          sm.setIndexSelected(newIndex, true);
          sm.anchorIndex = newIndex;
        }

        sm.endChange();

        if (prevent) {
          e.preventDefault();
        }
      }
    }
  };

  return {ListSelectionController: ListSelectionController};
});
