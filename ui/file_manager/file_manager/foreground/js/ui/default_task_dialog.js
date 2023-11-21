// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {FileManagerDialogBase} from './file_manager_dialog_base.js';
import {createList, List} from './list.js';
import {createListItem} from './list_item.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';


/**
 * DefaultTaskDialog contains a message, a list box, an ok button, and a
 * cancel button.
 * This dialog should be used as task picker for file operations.
 */
export class DefaultTaskDialog extends FileManagerDialogBase {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.id = 'default-task-dialog';

    this.list_ = createList();
    this.list_.id = 'default-tasks-list';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.insertBefore(this.list_, this.text.nextSibling);

    // @ts-ignore: error TS2322: Type 'ListSingleSelectionModel' is not
    // assignable to type 'ListSelectionModel'.
    this.selectionModel_ = this.list_.selectionModel =
        new ListSingleSelectionModel();
    this.dataModel_ = this.list_.dataModel = new ArrayDataModel([]);

    // List has max-height defined at css, so that list grows automatically,
    // but doesn't exceed predefined size.
    // @ts-ignore: error TS2339: Property 'autoExpands' does not exist on type
    // 'List'.
    this.list_.autoExpands = true;
    // @ts-ignore: error TS2339: Property 'activateItemAtIndex' does not exist
    // on type 'List'.
    this.list_.activateItemAtIndex = this.activateItemAtIndex_.bind(this);
    // Use 'click' instead of 'change' for keyboard users.
    this.list_.addEventListener('click', this.onSelected_.bind(this));
    this.list_.addEventListener('change', this.onListChange_.bind(this));

    /**
     * RequestAnimationFrame id used for throllting the list scroll event
     * listener.
     * @private @type {?number}
     */
    this.listScrollRaf_ = null;
    this.list_.addEventListener(
        'scroll', this.onListScroll_.bind(this), {passive: true});

    this.initialFocusElement_ = this.list_;

    /** @private @type {?function(*):void} */
    this.onSelectedItemCallback_ = null;

    const self = this;

    // Binding stuff doesn't work with constructors, so we have to create
    // closure here.
    // @ts-ignore: error TS7006: Parameter 'item' implicitly has an 'any' type.
    this.list_.itemConstructor = function(item) {
      return self.renderItem(item);
    };
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onListScroll_(event) {
    if (this.listScrollRaf_ &&
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        !this.frame.classList.contains('scrollable-list')) {
      return;
    }

    this.listScrollRaf_ = window.requestAnimationFrame(() => {
      const atTheBottom =
          Math.abs(
              this.list_.scrollHeight - this.list_.clientHeight -
              this.list_.scrollTop) < 1;
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.frame.classList.toggle('bottom-shadow', !atTheBottom);

      this.listScrollRaf_ = null;
    });
  }

  /**
   * Renders item for list.
   * @param {Object} item Item to render.
   */
  renderItem(item) {
    const result = createListItem();

    // Task label.
    const labelSpan = this.document_.createElement('span');
    labelSpan.classList.add('label');
    // @ts-ignore: error TS2339: Property 'label' does not exist on type
    // 'Object'.
    labelSpan.textContent = item.label;

    // Task file type icon.
    /** @type {!HTMLElement}*/
    const iconDiv = this.document_.createElement('div');
    iconDiv.classList.add('icon');

    // @ts-ignore: error TS2339: Property 'iconType' does not exist on type
    // 'Object'.
    if (item.iconType) {
      // @ts-ignore: error TS2339: Property 'iconType' does not exist on type
      // 'Object'.
      iconDiv.setAttribute('file-type-icon', item.iconType);
      // @ts-ignore: error TS2339: Property 'iconUrl' does not exist on type
      // 'Object'.
    } else if (item.iconUrl) {
      // @ts-ignore: error TS2339: Property 'iconUrl' does not exist on type
      // 'Object'.
      iconDiv.style.backgroundImage = 'url(' + item.iconUrl + ')';
    }

    // @ts-ignore: error TS2339: Property 'class' does not exist on type
    // 'Object'.
    if (item.class) {
      // @ts-ignore: error TS2339: Property 'class' does not exist on type
      // 'Object'.
      iconDiv.classList.add(item.class);
    }

    result.appendChild(labelSpan);
    result.appendChild(iconDiv);
    // A11y - make it focusable and readable.
    result.setAttribute('tabindex', '-1');

    return result;
  }

  /**
   * Shows dialog.
   *
   * @param {string} title Title in dialog caption.
   * @param {string} message Message in dialog caption.
   * @param {Array<Object>} items Items to render in the list.
   * @param {number} defaultIndex Item to select by default.
   * @param {function(*):void} onSelectedItem Callback which is called when an
   *     item is selected.
   */
  showDefaultTaskDialog(title, message, items, defaultIndex, onSelectedItem) {
    this.onSelectedItemCallback_ = onSelectedItem;

    const show = super.showTitleAndTextDialog(title, message);

    if (!show) {
      console.warn('DefaultTaskDialog can\'t be shown.');
      return;
    }

    if (!message) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.text.setAttribute('hidden', 'hidden');
    } else {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.text.removeAttribute('hidden');
    }

    // @ts-ignore: error TS2339: Property 'startBatchUpdates' does not exist on
    // type 'List'.
    this.list_.startBatchUpdates();
    // @ts-ignore: error TS2555: Expected at least 3 arguments, but got 2.
    this.dataModel_.splice(0, this.dataModel_.length);
    for (let i = 0; i < items.length; i++) {
      this.dataModel_.push(items[i]);
    }
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.classList.toggle('scrollable-list', items.length > 6);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.classList.toggle('bottom-shadow', items.length > 6);
    this.selectionModel_.selectedIndex = defaultIndex;
    // @ts-ignore: error TS2339: Property 'endBatchUpdates' does not exist on
    // type 'List'.
    this.list_.endBatchUpdates();
  }

  /**
   * List activation handler. Closes dialog and calls 'ok' callback.
   * @param {number} index Activated index.
   */
  activateItemAtIndex_(index) {
    this.hide();
    // @ts-ignore: error TS2721: Cannot invoke an object which is possibly
    // 'null'.
    this.onSelectedItemCallback_(this.dataModel_.item(index));
  }

  /**
   * Closes dialog and invokes callback with currently-selected item.
   */
  onSelected_() {
    if (this.selectionModel_.selectedIndex !== -1) {
      this.activateItemAtIndex_(this.selectionModel_.selectedIndex);
    }
  }

  /**
   * Called when List triggers a change event, which means user
   * focused a new item on the list. Used here to issue .focus() on
   * currently active item so ChromeVox can read it out.
   * @param {!Event} event triggered by List.
   */
  onListChange_(event) {
    const list = /** @type {List} */ (event.target);
    const activeItem =
        // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist
        // on type 'List'. Did you mean 'selectionModel'?
        list.getListItemByIndex(list.selectionModel_.selectedIndex);
    if (activeItem) {
      activeItem.focus();
    }
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onContainerKeyDown(event) {
    // Handle Escape.
    if (event.keyCode == 27) {
      this.hide();
      event.preventDefault();
    } else if (event.keyCode == 32 || event.keyCode == 13) {
      this.onSelected_();
      event.preventDefault();
    }
  }
}
