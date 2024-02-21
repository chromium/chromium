// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';
import type {DropdownItem} from '../task_controller.js';

import {FileManagerDialogBase} from './file_manager_dialog_base.js';
import type {List} from './list.js';
import {createList} from './list.js';
import type {ListItem} from './list_item.js';
import {createListItem} from './list_item.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';


/**
 * DefaultTaskDialog contains a message, a list box, an ok button, and a
 * cancel button.
 * This dialog should be used as task picker for file operations.
 */
export class DefaultTaskDialog extends FileManagerDialogBase {
  private readonly list_: List;
  private readonly selectionModel_: ListSingleSelectionModel;
  private readonly dataModel_: ArrayDataModel<DropdownItem>;
  private listScrollRaf_: number|null = null;
  private onSelectedItemCallback_: ((item: DropdownItem) => void)|null = null;

  /**
   * The `parentNode` must be the parent element for this dialog.
   */
  constructor(parentNode: HTMLElement) {
    super(parentNode);

    this.frame.id = 'default-task-dialog';

    this.list_ = createList();
    this.list_.id = 'default-tasks-list';
    this.frame.insertBefore(this.list_, this.text.nextSibling);

    this.selectionModel_ = this.list_.selectionModel =
        new ListSingleSelectionModel();
    this.dataModel_ = this.list_.dataModel =
        new ArrayDataModel<DropdownItem>([]);

    // List has max-height defined at css, so that list grows automatically,
    // but doesn't exceed predefined size.
    this.list_.autoExpands = true;
    this.list_.activateItemAtIndex = this.activateItemAtIndex_.bind(this);
    // Use 'click' instead of 'change' for keyboard users.
    this.list_.addEventListener('click', this.onSelected_.bind(this));
    this.list_.addEventListener('change', this.onListChange_.bind(this));

    this.list_.addEventListener(
        'scroll', this.onListScroll_.bind(this), {passive: true});

    this.initialFocusElement_ = this.list_;

    // Binding stuff doesn't work with constructors, so we have to create
    // closure here.
    this.list_.itemConstructor = (item: DropdownItem): ListItem => {
      return this.renderItem(item);
    };
  }

  private onListScroll_() {
    if (this.listScrollRaf_ &&
        !this.frame.classList.contains('scrollable-list')) {
      return;
    }

    // RequestAnimationFrame id used for throttling the list scroll event
    // listener.
    this.listScrollRaf_ = window.requestAnimationFrame(() => {
      const atTheBottom =
          Math.abs(
              this.list_.scrollHeight - this.list_.clientHeight -
              this.list_.scrollTop) < 1;
      this.frame.classList.toggle('bottom-shadow', !atTheBottom);

      this.listScrollRaf_ = null;
    });
  }

  /**
   * Renders item for list.
   * @param item Item to render.
   */
  renderItem(item: DropdownItem): ListItem {
    const result = createListItem();

    // Task label.
    const labelSpan = this.document_.createElement('span');
    labelSpan.classList.add('label');
    labelSpan.textContent = item.label;

    // Task file type icon.
    const iconDiv = this.document_.createElement('div');
    iconDiv.classList.add('icon');

    if (item.iconType) {
      iconDiv.setAttribute('file-type-icon', item.iconType);
    } else if (item.iconUrl) {
      iconDiv.style.backgroundImage = 'url(' + item.iconUrl + ')';
    }

    if (item.class) {
      iconDiv.classList.add(item.class);
    }

    result.appendChild(labelSpan);
    result.appendChild(iconDiv);
    // A11y - make it focusable and readable.
    result.setAttribute('tabindex', '-1');

    return result;
  }

  /**
   * Shows dialog. The `title` parameter specifies the title with which the
   * dialog is shown. The `message` is the message shown in the body of the
   * dialog. The `items` are items that describe actions available on the
   * selected file. The `defaultIndex` indicates the index of the item to be
   * selected by default. The `onSelectedItem` is the callback to be called once
   * the user clicks one of the `items`.
   */
  showDefaultTaskDialog(
      title: string, message: string, items: DropdownItem[],
      defaultIndex: number, onSelectedItem: (item: DropdownItem) => void) {
    this.onSelectedItemCallback_ = onSelectedItem;

    const show = super.showTitleAndTextDialog(title, message);

    if (!show) {
      console.warn('DefaultTaskDialog can\'t be shown.');
      return;
    }

    if (!message) {
      this.text.setAttribute('hidden', 'hidden');
    } else {
      this.text.removeAttribute('hidden');
    }

    this.list_.startBatchUpdates();
    this.dataModel_.splice(0, this.dataModel_.length);
    for (const item of items) {
      this.dataModel_.push(item);
    }
    this.frame.classList.toggle('scrollable-list', items.length > 6);
    this.frame.classList.toggle('bottom-shadow', items.length > 6);
    this.selectionModel_.selectedIndex = defaultIndex;
    this.list_.endBatchUpdates();
  }

  /**
   * List activation handler. Closes dialog and calls 'ok' callback.
   * @param index Activated index.
   */
  private activateItemAtIndex_(index: number) {
    this.hide();
    this.onSelectedItemCallback_?.(this.dataModel_.item(index)!);
  }

  /**
   * Closes dialog and invokes callback with currently-selected item.
   */
  private onSelected_() {
    if (this.selectionModel_.selectedIndex !== -1) {
      this.activateItemAtIndex_(this.selectionModel_.selectedIndex);
    }
  }

  /**
   * Called when List triggers a change event, which means user
   * focused a new item on the list. Used here to issue .focus() on
   * currently active item so ChromeVox can read it out.
   * @param event triggered by List.
   */
  private onListChange_(event: Event) {
    // TODO(b:289003444): Remove after M122 if this never fails.
    assert(event.target === this.list_);
    const activeItem =
        this.list_.getListItemByIndex(this.list_.selectionModel!.selectedIndex);
    if (activeItem) {
      activeItem.focus();
    }
  }

  override onContainerKeyDown(event: KeyboardEvent) {
    // Handle Escape.
    if (event.keyCode === 27) {
      this.hide();
      event.preventDefault();
    } else if (event.keyCode === 32 || event.keyCode === 13) {
      this.onSelected_();
      event.preventDefault();
    }
  }
}
