// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ListSelectionModel} from './list_selection_model.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';

export class FileListSelectionModel extends ListSelectionModel {
  private isCheckSelectMode_: boolean = false;

  /** @param length The number items in the selection. */
  constructor(length?: number) {
    super(length);

    /**
     * Overwrite ListSelectionModel to allow lead item to be independent of the
     * current selected item(s).
     */
    this.independentLeadItem = true;


    this.addEventListener('change', this.onChangeEvent_.bind(this));
  }

  /**
   * Updates the check-select mode.
   * @param enabled True if check-select mode should be enabled.
   */
  setCheckSelectMode(enabled: boolean) {
    this.isCheckSelectMode_ = enabled;
  }

  override selectAll() {
    super.selectAll();
    // Force change event when selecting all but with only 1 item, to update the
    // UI with select mode.
    if (this.isCheckSelectMode_ && this.selectedIndexes.length === 1) {
      const e = new CustomEvent('change', {detail: {changes: []}});
      this.dispatchEvent(e);

      // If force lead index when there is no lead, because doesn't make sense
      // to not have lead when there is selection.
      if (this.leadIndex < 0) {
        this.leadIndex = this.selectedIndexes[0]!;
      }
    }
  }

  /**
   * Gets the check-select mode.
   * @return True if check-select mode is enabled.
   */
  getCheckSelectMode(): boolean {
    return this.isCheckSelectMode_;
  }

  /**
   * Changes to single-select mode if all selected files get deleted.
   */
  override adjustToReordering(permutation: number[]) {
    // Look at the old state.
    const oldSelectedItemsCount = this.selectedIndexes.length;
    const newSelectedItemsCount =
        this.selectedIndexes.filter(i => permutation[i] !== -1).length;
    // Call the superclass function.
    super.adjustToReordering(permutation);
    // Leave check-select mode if all items have been deleted.
    if (oldSelectedItemsCount && !newSelectedItemsCount && this.length) {
      this.isCheckSelectMode_ = false;
    }
  }

  /**
   * Handles change event to update isCheckSelectMode_ BEFORE the change event
   * is dispatched to other listeners.
   * @param event Event object of 'change' event.
   */
  private onChangeEvent_(_event: Event) {
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
  independentLeadItem: boolean = false;
  /**
   * Updates the check-select mode.
   * @param enabled True if check-select mode should be enabled.
   */
  setCheckSelectMode(_enabled: boolean) {
    // Do nothing, as check-select mode is invalid in single selection model.
  }

  /**
   * Gets the check-select mode.
   * @return True if check-select mode is enabled.
   */
  getCheckSelectMode(): boolean {
    return false;
  }
}
