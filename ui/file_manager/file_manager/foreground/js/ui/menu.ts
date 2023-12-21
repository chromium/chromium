// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';

import {convertToKebabCase, crInjectTypeAndInit, domAttrSetter} from '../../../common/js/cr_ui.js';

import {MenuItem, type MenuItemActivationEvent} from './menu_item.js';

export interface ShownPosition {
  mouseDownPos?: {
    x: number,
    y: number,
  };

  // Time from Date.now().
  time: number;
}

export class Menu extends HTMLElement {
  private selectedIndex_: number = -1;

  /**
   * Element for which menu is being shown.
   */
  contextElement: HTMLElement|null = null;

  private shown_: ShownPosition|null = null;

  /**
   * Initializes the menu element.
   */
  initialize() {
    this.selectedIndex_ = -1;
    this.contextElement = null;
    this.shown_ = null;

    this.addEventListener('mouseover', this.handleMouseOver_);
    this.addEventListener('mouseout', this.handleMouseOut_);
    this.addEventListener('mouseup', this.handleMouseUp_, true);

    this.classList.add('decorated');
    this.setAttribute('role', 'menu');
    this.hidden = true;  // Hide the menu by default.

    // Decorate the children as menu items.
    for (const item of this.menuItems) {
      crInjectTypeAndInit(item, MenuItem);
    }
  }

  /**
   * Adds menu item at the end of the list.
   * @param item Menu item properties.
   * @return The created menu item.
   */
  addMenuItem(item: {label?: string, iconUrl?: string} = {}): MenuItem {
    const menuItem =
        this.ownerDocument.createElement('cr-menu-item') as MenuItem;
    this.appendChild(menuItem);
    crInjectTypeAndInit(menuItem, MenuItem);

    if (item.label) {
      menuItem.label = item.label;
    }

    if (item.iconUrl) {
      menuItem.iconUrl = item.iconUrl;
    }

    return menuItem;
  }

  /**
   * Adds separator at the end of the list.
   */
  addSeparator() {
    const separator = this.ownerDocument.createElement('hr');
    crInjectTypeAndInit(separator, MenuItem);
    this.appendChild(separator);
  }

  /**
   * Clears menu.
   */
  clear() {
    this.selectedItem = undefined;
    this.textContent = '';
  }

  /**
   * Walks up the ancestors of |node| until a menu item belonging to this menu
   * is found.
   * @param node The node to start searching from.
   * @return The found menu item or undefined.
   */
  protected findMenuItem(node: Node): MenuItem|undefined {
    while (node && node.parentNode !== this && !(node instanceof MenuItem)) {
      node = node.parentNode!;
    }

    if (node) {
      assertInstanceof(node, MenuItem);
      return node;
    }

    return undefined;
  }

  /**
   * Handles mouseover events and selects the hovered item.
   */
  private handleMouseOver_(e: Event) {
    const target = e.target as HTMLElement;
    const overItem = this.findMenuItem(target);
    this.selectedItem = overItem;
  }

  /**
   * Handles mouseout events and deselects any selected item.
   * @param e The mouseout event.
   */
  private handleMouseOut_(_e: Event) {
    this.selectedItem = undefined;
  }

  /**
   * If there's a mouseup that happens quickly in about the same position,
   * stop it from propagating to items. This is to prevent accidentally
   * selecting a menu item that's created under the mouse cursor.
   * @param e A mouseup event on the menu (in capturing phase).
   */
  private handleMouseUp_(e: MouseEvent) {
    const target = e.target as HTMLElement;

    assert(this.contains(target));
    assert(this.shown_);
    if (!this.trustEvent_(e) || Date.now() - this.shown_.time > 200) {
      return;
    }

    const pos = this.shown_.mouseDownPos;
    if (!pos || Math.abs(pos.x - e.screenX) + Math.abs(pos.y - e.screenY) > 4) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * @return Whether `e` can be trusted.
   */
  private trustEvent_(e: Event): boolean {
    return e.isTrusted || (e as any).isTrustedForTesting;
  }

  get menuItems(): MenuItem[] {
    return Array.from(this.querySelectorAll(this.menuItemSelector || '*'));
  }

  /**
   * The selected menu item or undefined if none.
   */
  get selectedItem(): MenuItem|undefined {
    return this.menuItems[this.selectedIndex];
  }

  set selectedItem(item: MenuItem|undefined) {
    const index = this.menuItems.indexOf(item!);
    this.selectedIndex = index;
  }

  /**
   * Focuses the selected item. If selectedIndex is invalid, set it to 0
   * first.
   */
  focusSelectedItem() {
    const items = this.menuItems;
    if (this.selectedIndex < 0 || this.selectedIndex > items.length) {
      // Find first visible item to focus by default.
      for (const [idx, item] of items.entries()) {
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
  }

  /**
   * Menu length
   */
  get length() {
    return this.menuItems.length;
  }

  /**
   * Returns whether the given menu item is visible.
   */
  private isItemVisible_(menuItem: MenuItem): boolean {
    if (menuItem.hidden) {
      return false;
    }
    if (menuItem.offsetParent) {
      return true;
    }
    // A "position: fixed" element won't have an offsetParent, so we have to
    // do the full style computation.
    return window.getComputedStyle(menuItem).display !== 'none';
  }

  /**
   * Returns whether the menu has any visible items.
   * @return True if the menu has visible item. Otherwise, false.
   */
  hasVisibleItems(): boolean {
    // Inspect items in reverse order to determine if the separator above each
    // set of items is required.
    for (const menuItem of this.menuItems) {
      if (this.isItemVisible_(menuItem)) {
        return true;
      }
    }
    return false;
  }

  /**
   * This is the function that handles keyboard navigation. This is usually
   * called by the element responsible for managing the menu.
   * @param e The keydown event object.
   * @return Whether the event was handled be the menu.
   */
  handleKeyDown(e: KeyboardEvent): boolean {
    let item = this.selectedItem;

    const self = this;
    const selectNextAvailable = (m: number) => {
      const menuItems = self.menuItems;
      const len = menuItems.length;
      if (!len) {
        // Edge case when there are no items.
        return;
      }
      let i = self.selectedIndex;
      if (i === -1 && m === -1) {
        // Edge case when needed to go the last item first.
        i = 0;
      }

      // `i` may be negative(-1), so modulus operation and cycle below
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
        if (item && !item.isSeparator() && !item.disabled &&
            this.isItemVisible_(item)) {
          break;
        }
      }
      if (item && !item.disabled) {
        self.selectedIndex = i;
      }
    };

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
          const activationEvent =
              document.createEvent('Event') as MenuItemActivationEvent;
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
  }

  hide() {
    this.hidden = true;
    this.shown_ = null;
  }

  show(mouseDownPos?: {x: number, y: number}) {
    this.shown_ = {mouseDownPos: mouseDownPos, time: Date.now()};
    this.hidden = false;
  }

  /**
   * Updates menu items command according to context.
   * @param node Node for which to actuate commands state.
   */
  updateCommands(node?: Node) {
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

  private selectedIndexChanged_(oldSelectedIndex: number) {
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
   */
  get selectedIndex(): number {
    return this.selectedIndex_;
  }

  set selectedIndex(value: number) {
    const oldValue = this.selectedIndex_;
    this.selectedIndex_ = value;
    this.selectedIndexChanged_(oldValue);
    dispatchPropertyChange(this, 'selectedIndex', value, oldValue);
  }

  /**
   * Selector for children which are menu items.
   */
  get menuItemSelector(): string {
    return this.getAttribute(convertToKebabCase('menuItemSelector')) ?? '';
  }

  set menuItemSelector(value: string) {
    domAttrSetter(this, 'menuItemSelector', value);
  }
}
