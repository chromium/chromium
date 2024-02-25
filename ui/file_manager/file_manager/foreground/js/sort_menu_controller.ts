// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {queryRequiredElement} from '../../common/js/dom_utils.js';

import type {FileListModel} from './file_list_model.js';
import type {MultiMenuButton} from './ui/multi_menu_button.js';

export class SortMenuController {
  private sortByNameButton_: HTMLElement;
  private sortBySizeButton_: HTMLElement;
  private sortByTypeButton_: HTMLElement;
  private sortByDateButton_: HTMLElement;

  constructor(
      sortButton: MultiMenuButton, private fileListModel_: FileListModel) {
    const menu = sortButton.menu;
    assert(menu);
    this.sortByNameButton_ =
        queryRequiredElement('#sort-menu-sort-by-name', menu);
    this.sortBySizeButton_ =
        queryRequiredElement('#sort-menu-sort-by-size', menu);
    this.sortByTypeButton_ =
        queryRequiredElement('#sort-menu-sort-by-type', menu);
    this.sortByDateButton_ =
        queryRequiredElement('#sort-menu-sort-by-date', menu);

    sortButton.addEventListener('menushow', this.updateCheckmark_.bind(this));
  }

  /**
   * Update checkmarks for each sort options.
   */
  private updateCheckmark_() {
    const field = this.fileListModel_.sortStatus.field;
    const sortField = field;

    this.setCheckStatus_(this.sortByNameButton_, sortField === 'name');
    this.setCheckStatus_(this.sortBySizeButton_, sortField === 'size');
    this.setCheckStatus_(this.sortByTypeButton_, sortField === 'type');
    this.setCheckStatus_(
        this.sortByDateButton_, sortField === 'modificationTime');
  }

  /**
   * Set attribute 'checked' for the menu item.
   */
  private setCheckStatus_(menuItem: HTMLElement, checked: boolean) {
    menuItem.toggleAttribute('checked', checked);
  }
}
