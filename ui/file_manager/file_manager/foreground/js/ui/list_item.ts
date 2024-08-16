// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {boolAttrSetter, crInjectTypeAndInit, jsSetter} from '../../../common/js/cr_ui.js';

/** The next id suffix to use when giving each item an unique id. */
let nextUniqueIdSuffix = 0;

/** Creates a new list item element. */
export function createListItem(): ListItem {
  const el = document.createElement('li');
  crInjectTypeAndInit(el, ListItem);
  return el as ListItem;
}

export class ListItem extends HTMLLIElement {
  /** This item's index in the containing list. */
  private listIndex_: number = -1;

  /** Plain text label. */
  get label(): string {
    return this.textContent || '';
  }

  set label(label) {
    this.textContent = label;
  }

  /** This item's index in the containing list. */
  get listIndex() {
    return this.listIndex_;
  }

  set listIndex(value: number) {
    jsSetter(this, 'listIndex', value);
  }

  /**
   * Whether the item is the lead in a selection. Setting this does not update
   * the underlying selection model. This is only used for display purpose.
   */
  get lead(): boolean {
    return this.hasAttribute('lead');
  }

  set lead(value: boolean) {
    boolAttrSetter(this, 'lead', value);
  }

  /**
   * Whether the item is selected. Setting this does not update the underlying
   * selection model. This is only used for display purpose.
   */
  get selected(): boolean {
    return this.hasAttribute('selected');
  }

  set selected(value: boolean) {
    boolAttrSetter(this, 'selected', value);
    this.setAttribute('aria-selected', String(value));
  }

  /** Called when an element is decorated as a list item. */
  initialize() {
    this.listIndex_ = -1;

    this.setAttribute('role', 'listitem');
    if (!this.id) {
      this.id = 'listitem-' + nextUniqueIdSuffix++;
    }
  }
}
