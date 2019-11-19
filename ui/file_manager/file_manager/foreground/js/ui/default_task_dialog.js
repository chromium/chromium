// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * DefaultTaskDialog contains a message, a list box, an ok button, and a
 * cancel button.
 * This dialog should be used as task picker for file operations.
 */
cr.define('cr.filebrowser', () => {
  /**
   * Creates dialog in DOM tree.
   */
  class DefaultTaskDialog extends FileManagerDialogBase {
    /**
     * @param {HTMLElement} parentNode Node to be parent for this dialog.
     */
    constructor(parentNode) {
      super(parentNode);

      this.frame.id = 'default-task-dialog';

      this.list_ = new cr.ui.List();
      this.list_.id = 'default-tasks-list';
      this.frame.insertBefore(this.list_, this.text.nextSibling);

      this.selectionModel_ = this.list_.selectionModel =
          new cr.ui.ListSingleSelectionModel();
      this.dataModel_ = this.list_.dataModel = new cr.ui.ArrayDataModel([]);

      // List has max-height defined at css, so that list grows automatically,
      // but doesn't exceed predefined size.
      this.list_.autoExpands = true;
      this.list_.activateItemAtIndex = this.activateItemAtIndex_.bind(this);
      // Use 'click' instead of 'change' for keyboard users.
      this.list_.addEventListener('click', this.onSelected_.bind(this));
      this.list_.addEventListener('change', this.onListChange_.bind(this));

      this.initialFocusElement_ = this.list_;

      /** @private {?function(*)} */
      this.onSelectedItemCallback_ = null;

      const self = this;

      // Binding stuff doesn't work with constructors, so we have to create
      // closure here.
      this.list_.itemConstructor = function(item) {
        return self.renderItem(item);
      };
    }

    /**
     * Renders item for list.
     * @param {Object} item Item to render.
     */
    renderItem(item) {
      const result = this.document_.createElement('li');

      const div = this.document_.createElement('div');
      div.textContent = item.label;

      if (item.iconType) {
        div.setAttribute('file-type-icon', item.iconType);
      } else if (item.iconUrl) {
        div.style.backgroundImage = 'url(' + item.iconUrl + ')';
      }

      if (item.class) {
        div.classList.add(item.class);
      }

      result.appendChild(div);
      // A11y - make it focusable and readable.
      result.setAttribute('tabindex', '-1');

      cr.defineProperty(result, 'lead', cr.PropertyKind.BOOL_ATTR);
      cr.defineProperty(result, 'selected', cr.PropertyKind.BOOL_ATTR);

      return result;
    }

    /**
     * Shows dialog.
     *
     * @param {string} title Title in dialog caption.
     * @param {string} message Message in dialog caption.
     * @param {Array<Object>} items Items to render in the list.
     * @param {number} defaultIndex Item to select by default.
     * @param {function(*)} onSelectedItem Callback which is called when an item
     *     is selected.
     */
    showDefaultTaskDialog(title, message, items, defaultIndex, onSelectedItem) {
      this.onSelectedItemCallback_ = onSelectedItem;

      const show = super.showTitleAndTextDialog(title, message);

      if (!show) {
        console.error('DefaultTaskDialog can\'t be shown.');
        return;
      }

      if (!message) {
        this.text.setAttribute('hidden', 'hidden');
      } else {
        this.text.removeAttribute('hidden');
      }

      this.list_.startBatchUpdates();
      this.dataModel_.splice(0, this.dataModel_.length);
      for (let i = 0; i < items.length; i++) {
        this.dataModel_.push(items[i]);
      }
      this.selectionModel_.selectedIndex = defaultIndex;
      this.list_.endBatchUpdates();
    }

    /**
     * List activation handler. Closes dialog and calls 'ok' callback.
     * @param {number} index Activated index.
     */
    activateItemAtIndex_(index) {
      this.hide();
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
     * Called when cr.ui.List triggers a change event, which means user
     * focused a new item on the list. Used here to issue .focus() on
     * currently active item so ChromeVox can read it out.
     * @param {!Event} event triggered by cr.ui.List.
     */
    onListChange_(event) {
      const list = /** @type {cr.ui.List} */ (event.target);
      const activeItem =
          list.getListItemByIndex(list.selectionModel_.selectedIndex);
      if (activeItem) {
        activeItem.focus();
      }
    }

    /**
     * @override
     */
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

  return {DefaultTaskDialog: DefaultTaskDialog};
});
