// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /** @const */ const MenuItem = cr.ui.MenuItem;

  /**
   * Creates a new menu element. Menu dispatches all commands on the element it
   * was shown for.
   *
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const Menu = cr.ui.define('cr-menu');

  Menu.prototype = {
    __proto__: HTMLElement.prototype,

    selectedIndex_: -1,

    /**
     * Element for which menu is being shown.
     */
    contextElement: null,

    /**
     * Initializes the menu element.
     */
    decorate: function() {
      this.addEventListener('mouseover', this.handleMouseOver_);
      this.addEventListener('mouseout', this.handleMouseOut_);
      this.addEventListener('mouseup', this.handleMouseUp_, true);

      this.classList.add('decorated');
      this.setAttribute('role', 'menu');
      this.hidden = true;  // Hide the menu by default.

      // Decorate the children as menu items.
      const menuItems = this.menuItems;
      for (let i = 0, menuItem; menuItem = menuItems[i]; i++) {
        cr.ui.decorate(menuItem, MenuItem);
      }
    },

    /**
     * Adds menu item at the end of the list.
     * @param {Object} item Menu item properties.
     * @return {!cr.ui.MenuItem} The created menu item.
     */
    addMenuItem: function(item) {
      const menuItem = this.ownerDocument.createElement('cr-menu-item');
      this.appendChild(menuItem);

      cr.ui.decorate(menuItem, MenuItem);

      if (item.label) {
        menuItem.label = item.label;
      }

      if (item.iconUrl) {
        menuItem.iconUrl = item.iconUrl;
      }

      return menuItem;
    },

    /**
     * Adds separator at the end of the list.
     */
    addSeparator: function() {
      const separator = this.ownerDocument.createElement('hr');
      cr.ui.decorate(separator, MenuItem);
      this.appendChild(separator);
    },

    /**
     * Clears menu.
     */
    clear: function() {
      this.selectedItem = null;
      this.textContent = '';
    },

    /**
     * Walks up the ancestors of |node| until a menu item belonging to this menu
     * is found.
     * @param {Node} node The node to start searching from.
     * @return {cr.ui.MenuItem} The found menu item or null.
     * @private
     */
    findMenuItem_: function(node) {
      while (node && node.parentNode != this && !(node instanceof MenuItem)) {
        node = node.parentNode;
      }
      return node ? assertInstanceof(node, MenuItem) : null;
    },

    /**
     * Handles mouseover events and selects the hovered item.
     * @param {Event} e The mouseover event.
     * @private
     */
    handleMouseOver_: function(e) {
      const overItem = this.findMenuItem_(/** @type {Element} */ (e.target));
      this.selectedItem = overItem;
    },

    /**
     * Handles mouseout events and deselects any selected item.
     * @param {Event} e The mouseout event.
     * @private
     */
    handleMouseOut_: function(e) {
      this.selectedItem = null;
    },

    /**
     * If there's a mouseup that happens quickly in about the same position,
     * stop it from propagating to items. This is to prevent accidentally
     * selecting a menu item that's created under the mouse cursor.
     * @param {Event} e A mouseup event on the menu (in capturing phase).
     * @private
     */
    handleMouseUp_: function(e) {
      assert(this.contains(/** @type {Element} */ (e.target)));

      if (!this.trustEvent_(e) || Date.now() - this.shown_.time > 200) {
        return;
      }

      const pos = this.shown_.mouseDownPos;
      if (!pos ||
          Math.abs(pos.x - e.screenX) + Math.abs(pos.y - e.screenY) > 4) {
        return;
      }

      e.preventDefault();
      e.stopPropagation();
    },

    /**
     * @param {!Event} e
     * @return {boolean} Whether |e| can be trusted.
     * @private
     * @suppress {checkTypes}
     */
    trustEvent_: function(e) {
      return e.isTrusted || e.isTrustedForTesting;
    },

    get menuItems() {
      return this.querySelectorAll(this.menuItemSelector || '*');
    },

    /**
     * The selected menu item or null if none.
     * @type {cr.ui.MenuItem}
     */
    get selectedItem() {
      return this.menuItems[this.selectedIndex];
    },
    set selectedItem(item) {
      const index = Array.prototype.indexOf.call(this.menuItems, item);
      this.selectedIndex = index;
    },

    /**
     * Focuses the selected item. If selectedIndex is invalid, set it to 0
     * first.
     */
    focusSelectedItem: function() {
      const items = this.menuItems;
      if (this.selectedIndex < 0 || this.selectedIndex > items.length) {
        // Find first visible item to focus by default.
        for (let idx = 0; idx < items.length; idx++) {
          const item = items[idx];
          if (item.hasAttribute('hidden') || item.isSeparator()) {
            continue;
          }
          // If the item is disabled we accept it, but try to find the next
          // enabled item, but keeping the first disabled item.
          if (!item.disabled) {
            this.selectedIndex = idx;
            break;
          } else if (this.selectedIndex === -1) {
            this.selectedIndex = idx;
          }
        }
      }

      if (this.selectedItem) {
        this.selectedItem.focus();
        this.setAttribute('aria-activedescendant', this.selectedItem.id);
      }
    },

    /**
     * Menu length
     */
    get length() {
      return this.menuItems.length;
    },

    /**
     * Returns whether the given menu item is visible.
     * @param {!cr.ui.MenuItem} menuItem
     * @return {boolean}
     * @private
     */
    isItemVisible_: function(menuItem) {
      if (menuItem.hidden) {
        return false;
      }
      if (menuItem.offsetParent) {
        return true;
      }
      // A "position: fixed" element won't have an offsetParent, so we have to
      // do the full style computation.
      return window.getComputedStyle(menuItem).display != 'none';
    },

    /**
     * Returns whether the menu has any visible items.
     * @return {boolean} True if the menu has visible item. Otherwise, false.
     */
    hasVisibleItems: function() {
      // Inspect items in reverse order to determine if the separator above each
      // set of items is required.
      for (const menuItem of this.menuItems) {
        if (this.isItemVisible_(menuItem)) {
          return true;
        }
      }
      return false;
    },

    /**
     * This is the function that handles keyboard navigation. This is usually
     * called by the element responsible for managing the menu.
     * @param {Event} e The keydown event object.
     * @return {boolean} Whether the event was handled be the menu.
     */
    handleKeyDown: function(e) {
      let item = this.selectedItem;

      const self = this;
      const selectNextAvailable = function(m) {
        const menuItems = self.menuItems;
        const len = menuItems.length;
        if (!len) {
          // Edge case when there are no items.
          return;
        }
        let i = self.selectedIndex;
        if (i == -1 && m == -1) {
          // Edge case when needed to go the last item first.
          i = 0;
        }

        // "i" may be negative(-1), so modulus operation and cycle below
        // wouldn't work as assumed. This trick makes startPosition positive
        // without altering it's modulo.
        const startPosition = (i + len) % len;

        while (true) {
          i = (i + m + len) % len;

          // Check not to enter into infinite loop if all items are hidden or
          // disabled.
          if (i == startPosition) {
            break;
          }

          item = menuItems[i];
          if (item && !item.isSeparator() && !item.disabled &&
              this.isItemVisible_(item)) {
            break;
          }
        }
        if (item && !item.disabled) {
          self.selectedIndex = i;
        }
      }.bind(this);

      switch (e.key) {
        case 'ArrowDown':
          selectNextAvailable(1);
          this.focusSelectedItem();
          return true;
        case 'ArrowUp':
          selectNextAvailable(-1);
          this.focusSelectedItem();
          return true;
        case 'Enter':
        case ' ':
          if (item) {
            // Store |contextElement| since it'll be removed when handling the
            // 'activate' event.
            const contextElement = this.contextElement;
            const activationEvent = document.createEvent('Event');
            activationEvent.initEvent('activate', true, true);
            activationEvent.originalEvent = e;
            if (item.dispatchEvent(activationEvent)) {
              if (item.command) {
                item.command.execute(contextElement);
              }
            }
          }
          return true;
      }

      return false;
    },

    hide: function() {
      this.hidden = true;
      delete this.shown_;
    },

    /** @param {{x: number, y: number}=} opt_mouseDownPos */
    show: function(opt_mouseDownPos) {
      this.shown_ = {mouseDownPos: opt_mouseDownPos, time: Date.now()};
      this.hidden = false;
    },

    /**
     * Updates menu items command according to context.
     * @param {Node=} node Node for which to actuate commands state.
     */
    updateCommands: function(node) {
      const menuItems = this.menuItems;

      for (const menuItem of menuItems) {
        if (!menuItem.isSeparator()) {
          menuItem.updateCommand(node);
        }
      }

      let separatorRequired = false;
      let lastSeparator = null;
      // Hide any separators without a visible item between them and the next
      // separator or the end of the menu.
      for (const menuItem of menuItems) {
        if (menuItem.isSeparator()) {
          if (separatorRequired) {
            lastSeparator = menuItem;
          }
          menuItem.hidden = true;
          separatorRequired = false;
          continue;
        }
        if (this.isItemVisible_(menuItem)) {
          if (lastSeparator) {
            lastSeparator.hidden = false;
          }
          separatorRequired = true;
        }
      }
    }
  };

  function selectedIndexChanged(selectedIndex, oldSelectedIndex) {
    const oldSelectedItem = this.menuItems[oldSelectedIndex];
    if (oldSelectedItem) {
      oldSelectedItem.selected = false;
      oldSelectedItem.blur();
    }
    const item = this.selectedItem;
    if (item) {
      item.selected = true;
    }
  }

  /**
   * The selected menu item.
   * type {number}
   */
  cr.defineProperty(
      Menu, 'selectedIndex', cr.PropertyKind.JS, selectedIndexChanged);

  /**
   * Selector for children which are menu items.
   */
  cr.defineProperty(Menu, 'menuItemSelector', cr.PropertyKind.ATTR);

  // Export
  return {Menu: Menu};
});
