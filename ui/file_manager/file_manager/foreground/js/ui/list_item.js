// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PropertyKind, getPropertyDescriptor} from 'chrome://resources/ash/common/cr_deprecated.js';
import {define as crUiDefine} from '../../../common/js/ui.js';
// clang-format on

/**
 * Creates a new list item element.
 * @constructor
 * @extends {HTMLLIElement}
 */
export const ListItem = crUiDefine('li');

/**
 * The next id suffix to use when giving each item an unique id.
 * @type {number}
 * @private
 */
ListItem.nextUniqueIdSuffix_ = 0;

ListItem.prototype = {
  __proto__: HTMLLIElement.prototype,

  /**
   * Plain text label.
   * @type {string}
   */
  get label() {
    return this.textContent;
  },
  set label(label) {
    this.textContent = label;
  },

  /**
   * This item's index in the containing list.
   * @type {number}
   */
  listIndex_: -1,

  /**
   * Called when an element is decorated as a list item.
   */
  decorate() {
    this.setAttribute('role', 'listitem');
    if (!this.id) {
      this.id = 'listitem-' + ListItem.nextUniqueIdSuffix_++;
    }
  },

  /**
   * Called when the selection state of this element changes.
   */
  selectionChanged() {},
};

/**
 * Whether the item is selected. Setting this does not update the underlying
 * selection model. This is only used for display purpose.
 * @type {boolean}
 */
ListItem.prototype.selected;
Object.defineProperty(
    ListItem.prototype, 'selected',
    getPropertyDescriptor('selected', PropertyKind.BOOL_ATTR, function() {
      this.selectionChanged();
    }));

/**
 * Whether the item is the lead in a selection. Setting this does not update
 * the underlying selection model. This is only used for display purpose.
 * @type {boolean}
 */
ListItem.prototype.lead;
Object.defineProperty(
    ListItem.prototype, 'lead',
    getPropertyDescriptor('lead', PropertyKind.BOOL_ATTR));

/**
 * This item's index in the containing list.
 * type {number}
 */
ListItem.prototype.listIndex;
Object.defineProperty(
    ListItem.prototype, 'listIndex', getPropertyDescriptor('listIndex'));
