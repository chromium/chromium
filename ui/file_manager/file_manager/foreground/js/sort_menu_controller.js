// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SortMenuController {
  /**
   * @param {!cr.ui.MenuButton} sortButton
   * @param {!FilesToggleRipple} toggleRipple
   * @param {!FileListModel} fileListModel
   */
  constructor(sortButton, toggleRipple, fileListModel) {
    /** @private @const {!FilesToggleRipple} */
    this.toggleRipple_ = toggleRipple;

    /** @private @const {!FileListModel} */
    this.fileListModel_ = fileListModel;

    /** @private @const {!HTMLElement} */
    this.sortByNameButton_ =
        queryRequiredElement('#sort-menu-sort-by-name', sortButton.menu);
    /** @private @const {!HTMLElement} */
    this.sortBySizeButton_ =
        queryRequiredElement('#sort-menu-sort-by-size', sortButton.menu);
    /** @private @const {!HTMLElement} */
    this.sortByTypeButton_ =
        queryRequiredElement('#sort-menu-sort-by-type', sortButton.menu);
    /** @private @const {!HTMLElement} */
    this.sortByDateButton_ =
        queryRequiredElement('#sort-menu-sort-by-date', sortButton.menu);

    sortButton.addEventListener('menushow', this.updateCheckmark_.bind(this));
    sortButton.addEventListener('menuhide', this.onHideSortMenu_.bind(this));
  }

  /**
   * Update checkmarks for each sort options.
   * @private
   */
  updateCheckmark_() {
    this.toggleRipple_.activated = true;
    const sortField = this.fileListModel_.sortStatus.field;

    this.setCheckStatus_(this.sortByNameButton_, sortField === 'name');
    this.setCheckStatus_(this.sortBySizeButton_, sortField === 'size');
    this.setCheckStatus_(this.sortByTypeButton_, sortField === 'type');
    this.setCheckStatus_(
        this.sortByDateButton_, sortField === 'modificationTime');
  }

  /**
   * Handle hide event of sort menu button.
   * @private
   */
  onHideSortMenu_() {
    this.toggleRipple_.activated = false;
  }

  /**
   * Set attribute 'checked' for the menu item.
   * @param {!HTMLElement} menuItem
   * @param {boolean} checked True if the item should have 'checked' attribute.
   * @private
   */
  setCheckStatus_(menuItem, checked) {
    if (checked) {
      menuItem.setAttribute('checked', '');
    } else {
      menuItem.removeAttribute('checked');
    }
  }
}
