// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';

import {define as crUiDefine, decorate} from '../../../common/js/ui.js';
import {MenuItem} from './menu_item.js';


  /**
   * Creates a new menu element. Menu dispatches all commands on the element it
   * was shown for.
   *
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const Menu = crUiDefine('cr-menu');

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
  decorate() {
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
    // null; decorate(): void; addMenuItem(item: Object): MenuItem;
    // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
    // undefined): void; }'.
    this.addEventListener('mouseover', this.handleMouseOver_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
    // null; decorate(): void; addMenuItem(item: Object): MenuItem;
    // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
    // undefined): void; }'.
    this.addEventListener('mouseout', this.handleMouseOut_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
    // null; decorate(): void; addMenuItem(item: Object): MenuItem;
    // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
    // undefined): void; }'.
    this.addEventListener('mouseup', this.handleMouseUp_, true);

    // @ts-ignore: error TS2339: Property 'classList' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    this.classList.add('decorated');
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    this.setAttribute('role', 'menu');
    // @ts-ignore: error TS2551: Property 'hidden' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'hide'?
    this.hidden = true;  // Hide the menu by default.

    // Decorate the children as menu items.
    const menuItems = this.menuItems;
    for (let i = 0, menuItem; menuItem = menuItems[i]; i++) {
      decorate(menuItem, MenuItem);
    }
  },

  /**
   * Adds menu item at the end of the list.
   * @param {Object} item Menu item properties.
   * @return {!MenuItem} The created menu item.
   */
  addMenuItem(item) {
    const menuItem = /** @type {!MenuItem} */ (
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type '{ __proto__: HTMLElement; selectedIndex_: number;
        // contextElement: null; decorate(): void; addMenuItem(item: Object):
        // MenuItem; addSeparator(): void; ... 15 more ...;
        // updateCommands(node?: Node | undefined): void; }'.
        this.ownerDocument.createElement('cr-menu-item'));
    // @ts-ignore: error TS2339: Property 'appendChild' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    this.appendChild(menuItem);

    decorate(menuItem, MenuItem);

    // @ts-ignore: error TS2339: Property 'label' does not exist on type
    // 'Object'.
    if (item.label) {
      // @ts-ignore: error TS2339: Property 'label' does not exist on type
      // 'Object'.
      menuItem.label = item.label;
    }

    // @ts-ignore: error TS2339: Property 'iconUrl' does not exist on type
    // 'Object'.
    if (item.iconUrl) {
      // @ts-ignore: error TS2339: Property 'iconUrl' does not exist on type
      // 'Object'.
      menuItem.iconUrl = item.iconUrl;
    }

    return menuItem;
  },

  /**
   * Adds separator at the end of the list.
   */
  addSeparator() {
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    const separator = this.ownerDocument.createElement('hr');
    decorate(separator, MenuItem);
    // @ts-ignore: error TS2339: Property 'appendChild' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    this.appendChild(separator);
  },

  /**
   * Clears menu.
   */
  clear() {
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'MenuItem'.
    this.selectedItem = null;
    // @ts-ignore: error TS2339: Property 'textContent' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    this.textContent = '';
  },

  /**
   * Walks up the ancestors of |node| until a menu item belonging to this menu
   * is found.
   * @param {Node} node The node to start searching from.
   * @return {MenuItem} The found menu item or null.
   * @private
   */
  findMenuItem_(node) {
    // @ts-ignore: error TS2367: This comparison appears to be unintentional
    // because the types 'ParentNode | null' and '{ __proto__: HTMLElement;
    // selectedIndex_: number; contextElement: null; decorate(): void;
    // addMenuItem(item: Object): MenuItem; addSeparator(): void; ... 15 more
    // ...; updateCommands(node?: Node | undefined): void; }' have no overlap.
    while (node && node.parentNode !== this && !(node instanceof MenuItem)) {
      // @ts-ignore: error TS2322: Type 'ParentNode | null' is not assignable to
      // type 'Node'.
      node = node.parentNode;
    }
    // @ts-ignore: error TS2322: Type 'MenuItem | null' is not assignable to
    // type 'MenuItem'.
    return node ? assertInstanceof(node, MenuItem) : null;
  },

  /**
   * Handles mouseover events and selects the hovered item.
   * @param {Event} e The mouseover event.
   * @private
   */
  handleMouseOver_(e) {
    const overItem = this.findMenuItem_(/** @type {Element} */ (e.target));
    this.selectedItem = overItem;
  },

  /**
   * Handles mouseout events and deselects any selected item.
   * @param {Event} e The mouseout event.
   * @private
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleMouseOut_(e) {
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'MenuItem'.
    this.selectedItem = null;
  },

  /**
   * If there's a mouseup that happens quickly in about the same position,
   * stop it from propagating to items. This is to prevent accidentally
   * selecting a menu item that's created under the mouse cursor.
   * @param {Event} e A mouseup event on the menu (in capturing phase).
   * @private
   */
  handleMouseUp_(e) {
    // @ts-ignore: error TS2339: Property 'contains' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    assert(this.contains(/** @type {Element} */ (e.target)));

    // @ts-ignore: error TS2551: Property 'shown_' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'show'?
    if (!this.trustEvent_(e) || Date.now() - this.shown_.time > 200) {
      return;
    }

    // @ts-ignore: error TS2551: Property 'shown_' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'show'?
    const pos = this.shown_.mouseDownPos;
    if (!pos ||
        // @ts-ignore: error TS2339: Property 'screenY' does not exist on type
        // 'Event'.
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
  trustEvent_(e) {
    // @ts-ignore: error TS2339: Property 'isTrustedForTesting' does not exist
    // on type 'Event'.
    return e.isTrusted || e.isTrustedForTesting;
  },

  get menuItems() {
    // @ts-ignore: error TS2339: Property 'menuItemSelector' does not exist on
    // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
    // null; decorate(): void; addMenuItem(item: Object): MenuItem;
    // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
    // undefined): void; }'.
    return this.querySelectorAll(this.menuItemSelector || '*');
  },

  /**
   * The selected menu item or null if none.
   * @type {MenuItem}
   */
  get selectedItem() {
    // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'selectedIndex_'?
    return this.menuItems[this.selectedIndex];
  },
  set selectedItem(item) {
    const index = Array.prototype.indexOf.call(this.menuItems, item);
    // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'selectedIndex_'?
    this.selectedIndex = index;
  },

  /**
   * Focuses the selected item. If selectedIndex is invalid, set it to 0
   * first.
   */
  focusSelectedItem() {
    const items = this.menuItems;
    // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist on type
    // '{ __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'selectedIndex_'?
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
          // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist
          // on type '{ __proto__: HTMLElement; selectedIndex_: number;
          // contextElement: null; decorate(): void; addMenuItem(item: Object):
          // MenuItem; addSeparator(): void; ... 15 more ...;
          // updateCommands(node?: Node | undefined): void; }'. Did you mean
          // 'selectedIndex_'?
          this.selectedIndex = idx;
          break;
          // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist
          // on type '{ __proto__: HTMLElement; selectedIndex_: number;
          // contextElement: null; decorate(): void; addMenuItem(item: Object):
          // MenuItem; addSeparator(): void; ... 15 more ...;
          // updateCommands(node?: Node | undefined): void; }'. Did you mean
          // 'selectedIndex_'?
        } else if (this.selectedIndex === -1) {
          // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist
          // on type '{ __proto__: HTMLElement; selectedIndex_: number;
          // contextElement: null; decorate(): void; addMenuItem(item: Object):
          // MenuItem; addSeparator(): void; ... 15 more ...;
          // updateCommands(node?: Node | undefined): void; }'. Did you mean
          // 'selectedIndex_'?
          this.selectedIndex = idx;
        }
      }
    }

    if (this.selectedItem) {
      this.selectedItem.focus();
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
      // null; decorate(): void; addMenuItem(item: Object): MenuItem;
      // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
      // undefined): void; }'.
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
   * @param {!MenuItem} menuItem
   * @return {boolean}
   * @private
   */
  isItemVisible_(menuItem) {
    if (menuItem.hidden) {
      return false;
    }
    if (menuItem.offsetParent) {
      return true;
    }
    // A "position: fixed" element won't have an offsetParent, so we have to
    // do the full style computation.
    return window.getComputedStyle(menuItem).display !== 'none';
  },

  /**
   * Returns whether the menu has any visible items.
   * @return {boolean} True if the menu has visible item. Otherwise, false.
   */
  hasVisibleItems() {
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
  handleKeyDown(e) {
    let item = this.selectedItem;

    const self = this;
    // @ts-ignore: error TS7006: Parameter 'm' implicitly has an 'any' type.
    const selectNextAvailable = function(m) {
      const menuItems = self.menuItems;
      const len = menuItems.length;
      if (!len) {
        // Edge case when there are no items.
        return;
      }
      // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist on
      // type '{ __proto__: HTMLElement; selectedIndex_: number; contextElement:
      // null; decorate(): void; addMenuItem(item: Object): MenuItem;
      // addSeparator(): void; ... 15 more ...; updateCommands(node?: Node |
      // undefined): void; }'. Did you mean 'selectedIndex_'?
      let i = self.selectedIndex;
      if (i === -1 && m === -1) {
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
        if (i === startPosition) {
          break;
        }

        item = menuItems[i];
        // @ts-ignore: error TS2339: Property 'isSeparator' does not exist on
        // type 'MenuItem'.
        if (item && !item.isSeparator() && !item.disabled &&
            // @ts-ignore: error TS2683: 'this' implicitly has type 'any'
            // because it does not have a type annotation.
            this.isItemVisible_(item)) {
          break;
        }
      }
      if (item && !item.disabled) {
        // @ts-ignore: error TS2551: Property 'selectedIndex' does not exist on
        // type '{ __proto__: HTMLElement; selectedIndex_: number;
        // contextElement: null; decorate(): void; addMenuItem(item: Object):
        // MenuItem; addSeparator(): void; ... 15 more ...;
        // updateCommands(node?: Node | undefined): void; }'. Did you mean
        // 'selectedIndex_'?
        self.selectedIndex = i;
      }
    }.bind(this);

    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
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
          // @ts-ignore: error TS2339: Property 'originalEvent' does not exist
          // on type 'Event'.
          activationEvent.originalEvent = e;
          if (item.dispatchEvent(activationEvent)) {
            // @ts-ignore: error TS2339: Property 'command' does not exist on
            // type 'MenuItem'.
            if (item.command) {
              // @ts-ignore: error TS2339: Property 'command' does not exist on
              // type 'MenuItem'.
              item.command.execute(contextElement);
            }
          }
        }
        return true;
    }

    return false;
  },

  hide() {
    // @ts-ignore: error TS2551: Property 'hidden' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'hide'?
    this.hidden = true;
    // @ts-ignore: error TS2551: Property 'shown_' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'show'?
    delete this.shown_;
  },

  /** @param {{x: number, y: number}=} opt_mouseDownPos */
  show(opt_mouseDownPos) {
    // @ts-ignore: error TS2551: Property 'shown_' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'show'?
    this.shown_ = {mouseDownPos: opt_mouseDownPos, time: Date.now()};
    // @ts-ignore: error TS2551: Property 'hidden' does not exist on type '{
    // __proto__: HTMLElement; selectedIndex_: number; contextElement: null;
    // decorate(): void; addMenuItem(item: Object): MenuItem; addSeparator():
    // void; ... 15 more ...; updateCommands(node?: Node | undefined): void; }'.
    // Did you mean 'hide'?
    this.hidden = false;
  },

  /**
   * Updates menu items command according to context.
   * @param {Node=} node Node for which to actuate commands state.
   */
  updateCommands(node) {
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
  },
};

/** @suppress {globalThis} This standalone function is used like method. */
// @ts-ignore: error TS7006: Parameter 'oldSelectedIndex' implicitly has an
// 'any' type.
function selectedIndexChanged(selectedIndex, oldSelectedIndex) {
  // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it does
  // not have a type annotation.
  const oldSelectedItem = this.menuItems[oldSelectedIndex];
  if (oldSelectedItem) {
    oldSelectedItem.selected = false;
    oldSelectedItem.blur();
  }
  // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it does
  // not have a type annotation.
  const item = this.selectedItem;
  if (item) {
    item.selected = true;
  }
}

  /**
   * The selected menu item.
   * @type {number}
   */
  Menu.prototype.selectedIndex;
  Object.defineProperty(
      Menu.prototype, 'selectedIndex',
      getPropertyDescriptor(
          'selectedIndex', PropertyKind.JS, selectedIndexChanged));

  /**
   * Selector for children which are menu items.
   * @type {string}
   */
  Menu.prototype.menuItemSelector;
  Object.defineProperty(
      Menu.prototype, 'menuItemSelector',
      getPropertyDescriptor('menuItemSelector', PropertyKind.ATTR));
