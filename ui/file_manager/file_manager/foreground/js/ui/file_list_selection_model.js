// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ListSelectionModel} from './list_selection_model.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';

export class FileListSelectionModel extends ListSelectionModel {
  /** @param {number=} opt_length The number items in the selection. */
  constructor(opt_length) {
    super(opt_length);

    /**
     * Overwrite ListSelectionModel to allow lead item to be independent of the
     * current selected item(s).
     */
    this.independentLeadItem = true;

    /** @private @type {boolean} */
    this.isCheckSelectMode_ = false;

    this.addEventListener('change', this.onChangeEvent_.bind(this));
  }

  /**
   * Updates the check-select mode.
   * @param {boolean} enabled True if check-select mode should be enabled.
   */
  setCheckSelectMode(enabled) {
    this.isCheckSelectMode_ = enabled;
  }

  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'ListSelectionModel'.
  selectAll() {
    super.selectAll();
    // Force change event when selecting all but with only 1 item, to update the
    // UI with select mode.
    if (this.isCheckSelectMode_ && this.selectedIndexes.length === 1) {
      const e = new Event('change');
      // @ts-ignore: error TS2339: Property 'changes' does not exist on type
      // 'Event'.
      e.changes = [];
      this.dispatchEvent(e);

      // If force lead index when there is no lead, because doesn't make sense
      // to not have lead when there is selection.
      if (this.leadIndex < 0) {
        this.leadIndex = this.selectedIndexes[0];
      }
    }
  }

  /**
   * Gets the check-select mode.
   * @return {boolean} True if check-select mode is enabled.
   */
  getCheckSelectMode() {
    return this.isCheckSelectMode_;
  }

  /**
   * @override
   * Changes to single-select mode if all selected files get deleted.
   */
  // @ts-ignore: error TS7006: Parameter 'permutation' implicitly has an 'any'
  // type.
  adjustToReordering(permutation) {
    // Look at the old state.
    const oldSelectedItemsCount = this.selectedIndexes.length;
    // @ts-ignore: error TS6133: 'oldLeadIndex' is declared but its value is
    // never read.
    const oldLeadIndex = this.leadIndex;
    const newSelectedItemsCount =
        this.selectedIndexes.filter(i => permutation[i] != -1).length;
    // Call the superclass function.
    super.adjustToReordering(permutation);
    // Leave check-select mode if all items have been deleted.
    if (oldSelectedItemsCount && !newSelectedItemsCount && this.length_) {
      this.isCheckSelectMode_ = false;
    }
  }

  /**
   * Handles change event to update isCheckSelectMode_ BEFORE the change event
   * is dispatched to other listeners.
   * @param {!Event} event Event object of 'change' event.
   * @private
   */
  // @ts-ignore: error TS6133: 'event' is declared but its value is never read.
  onChangeEvent_(event) {
    // When the number of selected item is not one, update the check-select
    // mode. When the number of selected item is one, the mode depends on the
    // last keyboard/mouse operation. In this case, the mode is controlled from
    // outside. See filelist.handlePointerDownUp and filelist.handleKeyDown.
    const selectedIndexes = this.selectedIndexes;
    if (selectedIndexes.length === 0) {
      this.isCheckSelectMode_ = false;
    } else if (selectedIndexes.length >= 2) {
      this.isCheckSelectMode_ = true;
    }
  }
}

export class FileListSingleSelectionModel extends ListSingleSelectionModel {
  /**
   * Updates the check-select mode.
   * @param {boolean} enabled True if check-select mode should be enabled.
   */
  // @ts-ignore: error TS6133: 'enabled' is declared but its value is never
  // read.
  setCheckSelectMode(enabled) {
    // Do nothing, as check-select mode is invalid in single selection model.
  }

  /**
   * Gets the check-select mode.
   * @return {boolean} True if check-select mode is enabled.
   */
  getCheckSelectMode() {
    return false;
  }
}
