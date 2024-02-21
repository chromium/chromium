// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Menu} from './menu.js';
import type {MenuItem} from './menu_item.js';
import {type MenuItemActivationEvent} from './menu_item.js';

/**
 * Creates a menu that supports sub-menus.
 *
 * This works almost identically to Menu apart from supporting
 * sub menus hanging off a <cr-menu-item> element. To add a sub menu
 * to a top level menu item, add a 'sub-menu' attribute which has as
 * its value an id selector for another <cr-menu> element.
 * (e.g. <cr-menu-item sub-menu="other-menu">).
 */
export class MultiMenu extends Menu {
  /**
   * Whether a sub-menu is positioned on the left of its parent.
   * Used to direct the arrow key navigation.
   */
  private subMenuOnLeft: null|boolean = null;

  /**
   * Property that hosts sub-menus for filling with overflow items.
   * Used for menu-items that overflow parent menu.
   */
  overflow: null|Menu = null;

  /**
   * Reference to the menu that the user is currently navigating.
   * Used to route events to the correct menu.
   */
  private currentMenu: undefined|MultiMenu = undefined;


  /** Sub menu being used. */
  private subMenu: Menu|null = null;

  /** Menu item hosting a sub menu. */
  private parentMenuItem: MenuItem|undefined = undefined;

  /**
   * Padding used when restricting menu height when the window is too small
   * to show the entire menu.

   * Padding on cr.menu + 2px.
   */
  private menuEndGap_: number = 0;

  /**
   * AbortController allows for global aborting of all event listeners and thus
   * their removal from the DOM.
   */
  private abortController_: AbortController|null = null;

  override initialize() {
    super.initialize();
    this.currentMenu = this;
    this.menuEndGap_ = 18;  // padding on cr.menu + 2px
  }

  /**
   * Handles event callbacks.
   * @param e The event object.
   */
  handleEvent(e: Event) {
    switch (e.type) {
      case 'activate':
        if (e.currentTarget === this) {
          const target = e.target as HTMLElement;
          // Don't activate if there's a sub-menu to show
          const item = this.findMenuItem(target);
          if (item) {
            const subMenuId = item.getAttribute('sub-menu');
            if (subMenuId) {
              e.preventDefault();
              e.stopPropagation();
              // Show the sub menu if needed.
              if (!item.getAttribute('sub-menu-shown')) {
                this.showSubMenu();
              }
            }
          }
        } else {
          // If the event was fired by the sub-menu, send an activate event to
          // the top level menu.
          const activationEvent =
              document.createEvent('Event') as MenuItemActivationEvent;
          activationEvent.initEvent('activate', true, true);
          activationEvent.originalEvent =
              (e as MenuItemActivationEvent).originalEvent;
          this.dispatchEvent(activationEvent);
        }
        break;
      case 'keydown':
        switch ((e as KeyboardEvent).key) {
          case 'ArrowLeft':
          case 'ArrowRight':
            if (!this.currentMenu) {
              break;
            }
            const key = (e as KeyboardEvent).key;
            if (this.currentMenu === this) {
              const menuItem = this.currentMenu.selectedItem;
              const subMenu = this.getSubMenuFromItem(menuItem);
              if (subMenu) {
                if (subMenu.hidden) {
                  break;
                }
                if (this.subMenuOnLeft && key === 'ArrowLeft') {
                  this.moveSelectionToSubMenu_(subMenu);
                } else if (
                    this.subMenuOnLeft === false && key === 'ArrowRight') {
                  this.moveSelectionToSubMenu_(subMenu);
                }
              }
            } else {
              const subMenu = this.currentMenu;
              // We only move off the sub-menu if we're on the top item
              if (subMenu.selectedIndex === 0) {
                if (this.subMenuOnLeft && key === 'ArrowRight') {
                  this.moveSelectionToTopMenu_(subMenu);
                } else if (
                    this.subMenuOnLeft === false && key === 'ArrowLeft') {
                  this.moveSelectionToTopMenu_(subMenu);
                }
              }
            }
            break;
          case 'ArrowDown':
          case 'ArrowUp':
            // Hide any showing sub-menu if we're moving in the parent.
            if (this.currentMenu === this) {
              this.hideSubMenu_();
            }
            break;
        }
        break;
      case 'mouseover':
      case 'mouseout':
        this.manageSubMenu(e as MouseEvent);
        break;
    }
  }

  /**
   * This event handler is used to redirect keydown events to
   * the top level and sub-menus when they're active.
   * Menu has a handleKeyDown() method and to support
   * sub-menus we monkey patch the cr.ui.menu call via
   * this.handleKeyDown_() and if any sub menu is active, by
   * calling the Menu method directly.
   * @param e The keydown event object.
   * @return Whether the event was handled be the menu.
   */
  override handleKeyDown(e: KeyboardEvent): boolean {
    if (!this.currentMenu) {
      return false;
    }
    if (this.currentMenu === this) {
      return super.handleKeyDown(e);
    } else {
      return this.currentMenu.handleKeyDown(e);
    }
  }

  /**
   * Position the sub menu adjacent to the cr-menu-item that triggered it.
   * @param item The menu item to position against.
   * @param subMenu The child (sub) menu to be positioned.
   */
  private positionSubMenu_(item: MenuItem, subMenu: Menu) {
    const style = subMenu.style;

    style.marginTop = '0';  // crbug.com/1066727

    // The sub-menu needs to sit aligned to the top and side of
    // the menu-item passed in. It also needs to fit inside the viewport
    const itemRect = item.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    const childRect = subMenu.getBoundingClientRect();
    const maxShift = itemRect.width / 2;

    // See if it fits on the right, if not position on the left
    // if there's more room on the left.
    style.left = style.right = style.top = style.bottom = 'auto';
    if ((itemRect.right + childRect.width) > viewportWidth &&
        ((viewportWidth - itemRect.right) < itemRect.left)) {
      let leftPosition = itemRect.left - childRect.width;
      // Allow some menu overlap if sub menu will be clipped off.
      if (leftPosition < 0) {
        if (leftPosition < -maxShift) {
          leftPosition += maxShift;
        } else {
          leftPosition = 0;
        }
      }
      this.subMenuOnLeft = true;
      style.left = leftPosition + 'px';
    } else {
      let rightPosition = itemRect.right;
      // Allow overlap on the right to reduce sub menu clip.
      if ((rightPosition + childRect.width) > viewportWidth) {
        if ((rightPosition + childRect.width - viewportWidth) > maxShift) {
          rightPosition -= maxShift;
        } else {
          rightPosition = viewportWidth - childRect.width;
        }
      }
      this.subMenuOnLeft = false;
      style.left = rightPosition + 'px';
    }
    style.top = itemRect.top + 'px';
    // Size the subMenu to fit inside the height of the viewport
    // Always set the maximum height so that expanding the window
    // allows the menu height to grow crbug/934207
    style.maxHeight = (viewportHeight - itemRect.top - this.menuEndGap_) + 'px';
    // Let the browser deal with scroll bar generation.
    style.overflowY = 'auto';
  }

  /**
   * Get the subMenu hanging off a menu-item if it exists.
   * @param item The menu item.
   */
  getSubMenuFromItem(item: MenuItem|undefined): MultiMenu|null {
    if (!item) {
      return null;
    }
    const subMenuId = item.getAttribute('sub-menu');
    if (subMenuId === null) {
      return null;
    }
    return document.querySelector<MultiMenu>(subMenuId);
  }

  /**
   * Display any sub-menu hanging off the current selection.
   */
  showSubMenu() {
    const item = this.selectedItem;
    const subMenu = this.getSubMenuFromItem(item);
    if (subMenu) {
      this.subMenu = subMenu;
      if (item) {
        item.setAttribute('sub-menu-shown', 'shown');
        this.positionSubMenu_(item, subMenu);
      }
      subMenu.show();
      subMenu.parentMenuItem = item;
      this.moveSelectionToSubMenu_(subMenu);
    }
  }

  /**
   * Find any sub-menu hanging off the event target and show/hide it.
   * @param e The event object.
   */
  manageSubMenu(e: MouseEvent) {
    const target = e.target as HTMLElement;
    const item = this.findMenuItem(target);
    const subMenu = this.getSubMenuFromItem(item);
    if (!subMenu) {
      return;
    }
    this.subMenu = subMenu;
    switch (e.type) {
      case 'activate':
      case 'mouseover':
        // Hide any other sub menu being shown.
        const showing =
            this.querySelector<MenuItem>('cr-menu-item[sub-menu-shown]');
        if (showing && showing !== item) {
          showing.removeAttribute('sub-menu-shown');
          const shownSubMenu = this.getSubMenuFromItem(showing);
          if (shownSubMenu) {
            shownSubMenu.hide();
          }
        }
        if (item) {
          item.setAttribute('sub-menu-shown', 'shown');
          this.positionSubMenu_(item, subMenu);
        }
        subMenu.show();
        break;
      case 'mouseout':
        // If we're on top of the sub-menu, we don't want to dismiss it
        const childRect = subMenu.getBoundingClientRect();
        if (childRect.left <= e.clientX && e.clientX < childRect.right &&
            childRect.top <= e.clientY && e.clientY < childRect.bottom) {
          this.currentMenu = subMenu;
          break;
        }
        item?.removeAttribute('sub-menu-shown');
        subMenu.hide();
        this.subMenu = null;
        this.currentMenu = this;
        break;
    }
  }

  /**
   * Change the selection from the top level menu to the first item
   * in the subMenu passed in.
   * @param subMenu sub-menu that should take selection.
   */
  private moveSelectionToSubMenu_(subMenu: MultiMenu) {
    this.selectedItem = undefined;
    this.currentMenu = subMenu;
    subMenu.selectedIndex = 0;
    subMenu.focusSelectedItem();
  }

  /**
   * Change the selection from the sub menu to the top level menu.
   * @param subMenu sub-menu that should lose selection.
   */
  private moveSelectionToTopMenu_(subMenu: MultiMenu) {
    subMenu.selectedItem = undefined;
    this.currentMenu = this;
    this.selectedItem = subMenu.parentMenuItem;
    this.focusSelectedItem();
  }

  /**
   * Add event listeners to any sub menus.
   */
  addSubMenuListeners() {
    const items = this.querySelectorAll('cr-menu-item[sub-menu]');
    items.forEach((menuItem) => {
      const subMenuId = menuItem.getAttribute('sub-menu');
      if (subMenuId) {
        const subMenu = document.querySelector(subMenuId);
        if (subMenu) {
          subMenu.addEventListener(
              'activate', this, {signal: this.abortController_?.signal});
        }
      }
    });
  }

  override show(mouseDownPos?: {x: number, y: number}) {
    super.show(mouseDownPos);
    // When the menu is shown we steal all keyboard events.
    const doc = this.ownerDocument;
    this.abortController_ = new AbortController();
    const signal = this.abortController_.signal;
    if (doc) {
      doc.addEventListener('keydown', this, {capture: true, signal});
    }
    this.addEventListener('activate', this, {capture: true, signal});
    // Handle mouse-over to trigger sub menu opening on hover.
    this.addEventListener('mouseover', this, {signal});
    this.addEventListener('mouseout', this, {signal});
    this.addSubMenuListeners();
  }

  /**
   * Hides any sub-menu that is active.
   */
  private hideSubMenu_() {
    const items =
        this.querySelectorAll('cr-menu-item[sub-menu][sub-menu-shown]');
    for (const menuItem of items) {
      const subMenuId = menuItem.getAttribute('sub-menu');
      if (subMenuId) {
        const subMenu = document.querySelector<Menu>(subMenuId);
        if (subMenu) {
          subMenu.hide();
        }
        menuItem.removeAttribute('sub-menu-shown');
      }
    }
    this.currentMenu = this;
  }

  override hide() {
    this.abortController_?.abort();
    // Hide any visible sub-menus first
    this.hideSubMenu_();
    super.hide();
  }

  /**
   * Check if a DOM element is containd within the main top
   * level menu or any sub-menu hanging off the top level menu.
   * @param node Node being tested for containment.
   */
  override contains(node: Node|null) {
    return super.contains(node) ||
        (this.subMenu ? this.subMenu.contains(node) : false);
  }
}
