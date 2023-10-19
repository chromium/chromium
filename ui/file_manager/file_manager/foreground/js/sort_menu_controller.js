// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {queryRequiredElement} from '../../common/js/dom_utils.js';

import {FileListModel} from './file_list_model.js';
import {MultiMenuButton} from './ui/multi_menu_button.js';

export class SortMenuController {
  /**
   * @param {!MultiMenuButton} sortButton
   * @param {!FileListModel} fileListModel
   */
  constructor(sortButton, fileListModel) {
    /** @private @const @type {!FileListModel} */
    this.fileListModel_ = fileListModel;

    /** @private @const @type {!HTMLElement} */
    this.sortByNameButton_ =
        // @ts-ignore: error TS2339: Property 'menu' does not exist on type
        // 'MultiMenuButton'.
        queryRequiredElement('#sort-menu-sort-by-name', sortButton.menu);
    /** @private @const @type {!HTMLElement} */
    this.sortBySizeButton_ =
        // @ts-ignore: error TS2339: Property 'menu' does not exist on type
        // 'MultiMenuButton'.
        queryRequiredElement('#sort-menu-sort-by-size', sortButton.menu);
    /** @private @const @type {!HTMLElement} */
    this.sortByTypeButton_ =
        // @ts-ignore: error TS2339: Property 'menu' does not exist on type
        // 'MultiMenuButton'.
        queryRequiredElement('#sort-menu-sort-by-type', sortButton.menu);
    /** @private @const @type {!HTMLElement} */
    this.sortByDateButton_ =
        // @ts-ignore: error TS2339: Property 'menu' does not exist on type
        // 'MultiMenuButton'.
        queryRequiredElement('#sort-menu-sort-by-date', sortButton.menu);

    sortButton.addEventListener('menushow', this.updateCheckmark_.bind(this));
  }

  /**
   * Update checkmarks for each sort options.
   * @private
   */
  updateCheckmark_() {
    // @ts-ignore: error TS2339: Property 'field' does not exist on type
    // 'Object'.
    const sortField = this.fileListModel_.sortStatus.field;

    this.setCheckStatus_(this.sortByNameButton_, sortField === 'name');
    this.setCheckStatus_(this.sortBySizeButton_, sortField === 'size');
    this.setCheckStatus_(this.sortByTypeButton_, sortField === 'type');
    this.setCheckStatus_(
        this.sortByDateButton_, sortField === 'modificationTime');
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
