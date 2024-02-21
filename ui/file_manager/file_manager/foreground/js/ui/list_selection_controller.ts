// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ListSelectionModel} from './list_selection_model.js';
import type {ListSingleSelectionModel} from './list_single_selection_model.js';

/**
 * The selection controller that is to be used with lists. This is implemented
 * for vertical lists but changing the behavior for horizontal lists or icon
 * views is a matter of overriding `getIndexBefore()`, `getIndexAfter()`,
 * `getIndexAbove()` as well as `getIndexBelow()`.
 */
export class ListSelectionController {
  /**
   * @param selectionModel The selection model to interact with.
   */
  constructor(private selectionModel_: ListSelectionModel|
              ListSingleSelectionModel) {}


  /**
   * The selection model we are interacting with.
   */
  get selectionModel(): ListSelectionModel|ListSingleSelectionModel {
    return this.selectionModel_;
  }

  /**
   * Returns the index below (y axis) the given element.
   * @param index The index to get the index below.
   * @return The index below or -1 if not found.
   */
  getIndexBelow(index: number): number {
    if (index === this.getLastIndex()) {
      return -1;
    }
    return index + 1;
  }

  /**
   * Returns the index above (y axis) the given element.
   * @param index The index to get the index above.
   * @return The index below or -1 if not found.
   */
  getIndexAbove(index: number): number {
    return index - 1;
  }

  /**
   * Returns the index before (x axis) the given element. This returns -1
   * by default but override this for icon view and horizontal selection
   * models.
   *
   * @param _index The index to get the index before.
   */
  getIndexBefore(_index: number): number {
    return -1;
  }

  /**
   * Returns the index after (x axis) the given element. This returns -1
   * by default but override this for icon view and horizontal selection
   * models.
   *
   * @param index The index to get the index after.
   */
  getIndexAfter(_index: number): number {
    return -1;
  }

  /**
   * Returns the next list index. This is the next logical and should not
   * depend on any kind of layout of the list.
   * @param index The index to get the next index for.
   * @return The next index or -1 if not found.
   */
  getNextIndex(index: number): number {
    if (index === this.getLastIndex()) {
      return -1;
    }
    return index + 1;
  }

  /**
   * Returns the previous list index. This is the previous logical and should
   * not depend on any kind of layout of the list.
   * @param index The index to get the previous index for.
   * @return The previous index or -1 if not found.
   */
  getPreviousIndex(index: number): number {
    return index - 1;
  }

  /**
   * @return The first index.
   */
  getFirstIndex(): number {
    return 0;
  }

  /**
   * @return The last index.
   */
  getLastIndex(): number {
    return this.selectionModel.length - 1;
  }

  /**
   * Called by the view when the user does a mousedown or mouseup on the
   * list.
   * @param e The browser mouse event.
   * @param index The index that was under the mouse pointer, -1 if none.
   */
  handlePointerDownUp(e: MouseEvent, index: number) {
    const sm = this.selectionModel;
    const anchorIndex = sm.anchorIndex;
    const isDown = (e.type === 'mousedown');

    sm.beginChange();

    if (index === -1) {
      // On CrOS we always clear the selection if the user clicks a blank area.
      sm.leadIndex = sm.anchorIndex = -1;
      sm.unselectAll();
    } else {
      if (sm.multiple && (e.ctrlKey && !e.shiftKey)) {
        // Selection is handled at mouseUp on windows/linux, mouseDown on mac.
        if (!isDown) {
          // Toggle the current one and make it anchor index.
          sm.setIndexSelected(index, !sm.getIndexSelected(index));
          sm.leadIndex = index;
          sm.anchorIndex = index;
        }
      } else if (e.shiftKey && anchorIndex !== -1 && anchorIndex !== index) {
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
        const isRightClick = e.button === 2;

        // If the index is selected this is handled in mouseup.
        const indexSelected = sm.getIndexSelected(index);
        if ((indexSelected && !isDown || !indexSelected && isDown) &&
            !(indexSelected && isRightClick)) {
          sm.selectedIndex = index;
        }
      }
    }

    sm.endChange();
  }

  /**
   * Called by the view when it receives either a touchstart, touchmove,
   * touchend, or touchcancel event.
   * Sub-classes may override this function to handle touch events separately
   * from mouse events, instead of waiting for emulated mouse events sent
   * after the touch events.
   * @param _e The event.
   * @param _index The index that was under the touched point, -1 if none.
   */
  handleTouchEvents(_e: Event, _index: number) {
    // Do nothing.
  }

  /**
   * Called by the view when it receives a keydown event.
   * @param e The keydown event.
   */
  handleKeyDown(e: KeyboardEvent) {
    const target = e.target as HTMLElement;
    const tagName = target.tagName;
    // If focus is in an input field of some kind, only handle navigation keys
    // that aren't likely to conflict with input interaction (e.g., text
    // editing, or changing the value of a checkbox or select).
    if (tagName === 'INPUT') {
      const inputType = (target as HTMLInputElement).type;
      // Just protect space (for toggling) for checkbox and radio.
      if (inputType === 'checkbox' || inputType === 'radio') {
        if (e.key === ' ') {
          return;
        }
        // Protect all but the most basic navigation commands in anything
        // else.
      } else if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') {
        return;
      }
    }
    // Similarly, don't interfere with select element handling.
    if (tagName === 'SELECT') {
      return;
    }

    const sm = this.selectionModel;
    let newIndex = -1;
    const leadIndex = sm.leadIndex;
    let prevent = true;

    // Ctrl/Meta+A
    if (sm.multiple && e.keyCode === 65 && e.ctrlKey) {
      sm.selectAll();
      e.preventDefault();
      return;
    }

    if (e.key === ' ') {
      if (leadIndex !== -1) {
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
        newIndex = leadIndex === -1 ? this.getLastIndex() :
                                      this.getIndexAbove(leadIndex);
        break;
      case 'ArrowDown':
        newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                      this.getIndexBelow(leadIndex);
        break;
      case 'ArrowLeft':
      case 'MediaPreviousTrack':
        newIndex = leadIndex === -1 ? this.getLastIndex() :
                                      this.getIndexBefore(leadIndex);
        break;
      case 'ArrowRight':
      case 'MediaNextTrack':
        newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                      this.getIndexAfter(leadIndex);
        break;
      default:
        prevent = false;
    }

    if (newIndex !== -1) {
      sm.beginChange();

      sm.leadIndex = newIndex;
      if (e.shiftKey) {
        const anchorIndex = sm.anchorIndex;
        if (sm.multiple) {
          sm.unselectAll();
        }
        if (anchorIndex === -1) {
          sm.setIndexSelected(newIndex, true);
          sm.anchorIndex = newIndex;
        } else {
          sm.selectRange(anchorIndex, newIndex);
        }
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
}
