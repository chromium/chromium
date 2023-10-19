// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {decorate} from '../../../common/js/ui.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {EventTracker} from 'chrome://resources/ash/common/event_tracker.js';

/**
 * Creates a menu that supports sub-menus.
 *
 * This works almost identically to Menu apart from supporting
 * sub menus hanging off a <cr-menu-item> element. To add a sub menu
 * to a top level menu item, add a 'sub-menu' attribute which has as
 * its value an id selector for another <cr-menu> element.
 * (e.g. <cr-menu-item sub-menu="other-menu">).
 * @extends {Menu}
 * @implements {EventListener}
 */
// @ts-ignore: error TS2420: Class 'MultiMenu' incorrectly implements interface
// 'EventListener'.
export class MultiMenu {
  constructor() {
    /**
     * Whether a sub-menu is positioned on the left of its parent.
     * @private @type {?boolean} Used to direct the arrow key navigation.
     */
    this.subMenuOnLeft = null;

    /**
     * Property that hosts sub-menus for filling with overflow items.
     * @public @type {?Menu} Used for menu-items that overflow parent
     * menu.
     */
    // @ts-ignore: error TS7008: Member 'overflow' implicitly has an 'any' type.
    this.overflow = null;

    /**
     * Reference to the menu that the user is currently navigating.
     * @private @type {?MultiMenu|Menu} Used to route events to the
     *     correct menu.
     */
    this.currentMenu = null;

    /** @private @type {?Menu} Sub menu being used. */
    this.subMenu = null;

    /** @private @type {?MenuItem} Menu item hosting a sub menu. */
    // @ts-ignore: error TS7008: Member 'parentMenuItem' implicitly has an 'any'
    // type.
    this.parentMenuItem = null;

    /** @private @type {?MenuItem} Selected item in a menu. */
    this.selectedItem = null;

    /**
     * Padding used when restricting menu height when the window is too small
     * to show the entire menu.
     * @private @type {number}
     */
    this.menuEndGap_ = 0;  // padding on cr.menu + 2px.

    /** @private @type {?EventTracker} */
    this.showingEvents_ = null;

    /** TODO(adanilo) Annotate these for closure checking. */
    // @ts-ignore: error TS7008: Member 'contains_' implicitly has an 'any'
    // type.
    this.contains_ = undefined;
    // @ts-ignore: error TS7008: Member 'handleKeyDown_' implicitly has an 'any'
    // type.
    this.handleKeyDown_ = undefined;
    // @ts-ignore: error TS7008: Member 'hide_' implicitly has an 'any' type.
    this.hide_ = undefined;
    // @ts-ignore: error TS7008: Member 'show_' implicitly has an 'any' type.
    this.show_ = undefined;
  }

  /**
   * Initializes the multi menu.
   * @param {!Element} element Element to be decorated.
   * @return {!MultiMenu} Decorated element.
   */
  static decorate(element) {
    // Decorate the menu as a single level menu.
    decorate(element, Menu);
    // Grab Menu functions we want to override.
    // TODO(adanilo) Try to work around this suppress.
    // It's needed when monkey patching an in-built DOM method
    // that closure doesn't understand.
    /** @suppress {checkTypes} */
    // @ts-ignore: error TS2551: Property 'contains_' does not exist on type
    // 'Element'. Did you mean 'contains'?
    element.contains_ = Menu.prototype['contains'];
    // @ts-ignore: error TS2339: Property 'handleKeyDown_' does not exist on
    // type 'Element'.
    element.handleKeyDown_ = Menu.prototype['handleKeyDown'];
    // @ts-ignore: error TS2339: Property 'hide_' does not exist on type
    // 'Element'.
    element.hide_ = Menu.prototype['hide'];
    // @ts-ignore: error TS2339: Property 'show_' does not exist on type
    // 'Element'.
    element.show_ = Menu.prototype['show'];
    // Add the MultiMenuButton methods to the element we're decorating.
    Object.getOwnPropertyNames(MultiMenu.prototype).forEach(name => {
      if (name !== 'constructor' &&
          !Object.getOwnPropertyDescriptor(element, name)) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'MultiMenu'.
        element[name] = MultiMenu.prototype[name];
      }
    });
    // @ts-ignore: error TS2352: Conversion of type 'Element' to type
    // 'MultiMenu' may be a mistake because neither type sufficiently overlaps
    // with the other. If this was intentional, convert the expression to
    // 'unknown' first.
    element = /** @type {!MultiMenu} */ (element);
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Element'.
    element.decorate();
    // @ts-ignore: error TS2740: Type 'Element' is missing the following
    // properties from type 'MultiMenu': subMenuOnLeft, overflow, currentMenu,
    // subMenu, and 23 more.
    return element;
  }

  decorate() {
    // Event tracker for the sub-menu specific listeners.
    this.showingEvents_ = new EventTracker();
    this.currentMenu = this;
    this.menuEndGap_ = 18;  // padding on cr.menu + 2px
  }

  /**
   * Handles event callbacks.
   * @param {Event} e The event object.
   */
  handleEvent(e) {
    switch (e.type) {
      case 'activate':
        // @ts-ignore: error TS2367: This comparison appears to be unintentional
        // because the types 'EventTarget | null' and 'this' have no overlap.
        if (e.currentTarget === this) {
          // Don't activate if there's a sub-menu to show
          // @ts-ignore: error TS2339: Property 'findMenuItem' does not exist on
          // type 'never'.
          const item = this.findMenuItem(e.target);
          if (item) {
            const subMenuId = item.getAttribute('sub-menu');
            if (subMenuId) {
              e.preventDefault();
              e.stopPropagation();
              // Show the sub menu if needed.
              if (!item.getAttribute('sub-menu-shown')) {
                // @ts-ignore: error TS2339: Property 'showSubMenu' does not
                // exist on type 'never'.
                this.showSubMenu();
              }
            }
          }
        } else {
          // If the event was fired by the sub-menu, send an activate event to
          // the top level menu.
          const activationEvent = document.createEvent('Event');
          activationEvent.initEvent('activate', true, true);
          // @ts-ignore: error TS2339: Property 'originalEvent' does not exist
          // on type 'Event'.
          activationEvent.originalEvent = e.originalEvent;
          // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist
          // on type 'MultiMenu'.
          this.dispatchEvent(activationEvent);
        }
        break;
      case 'keydown':
        // @ts-ignore: error TS2339: Property 'key' does not exist on type
        // 'Event'.
        switch (e.key) {
          case 'ArrowLeft':
          case 'ArrowRight':
            if (!this.currentMenu) {
              break;
            }
            if (this.currentMenu === this) {
              const menuItem = this.currentMenu.selectedItem;
              // @ts-ignore: error TS2345: Argument of type 'MenuItem | null' is
              // not assignable to parameter of type 'MenuItem'.
              const subMenu = this.getSubMenuFromItem(menuItem);
              if (subMenu) {
                // @ts-ignore: error TS2339: Property 'hidden' does not exist on
                // type 'Menu'.
                if (subMenu.hidden) {
                  break;
                }
                // @ts-ignore: error TS2339: Property 'key' does not exist on
                // type 'Event'.
                if (this.subMenuOnLeft && e.key == 'ArrowLeft') {
                  this.moveSelectionToSubMenu_(subMenu);
                } else if (
                    // @ts-ignore: error TS2339: Property 'key' does not exist
                    // on type 'Event'.
                    this.subMenuOnLeft === false && e.key == 'ArrowRight') {
                  this.moveSelectionToSubMenu_(subMenu);
                }
              }
            } else {
              const subMenu = /** @type {Menu} */ (this.currentMenu);
              // We only move off the sub-menu if we're on the top item
              if (subMenu.selectedIndex == 0) {
                // @ts-ignore: error TS2339: Property 'key' does not exist on
                // type 'Event'.
                if (this.subMenuOnLeft && e.key == 'ArrowRight') {
                  this.moveSelectionToTopMenu_(subMenu);
                } else if (
                    // @ts-ignore: error TS2339: Property 'key' does not exist
                    // on type 'Event'.
                    this.subMenuOnLeft === false && e.key == 'ArrowLeft') {
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
        this.manageSubMenu(e);
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
   * @param {Event} e The keydown event object.
   * @return {boolean} Whether the event was handled be the menu.
   */
  handleKeyDown(e) {
    if (!this.currentMenu) {
      return false;
    }
    if (this.currentMenu === this) {
      return this.handleKeyDown_(e);
    } else {
      // @ts-ignore: error TS2339: Property 'handleKeyDown' does not exist on
      // type 'Menu | this'.
      return this.currentMenu.handleKeyDown(e);
    }
  }

  /**
   * Position the sub menu adjacent to the cr-menu-item that triggered it.
   * @param {MenuItem} item The menu item to position against.
   * @param {Menu} subMenu The child (sub) menu to be positioned.
   */
  positionSubMenu_(item, subMenu) {
    // @ts-ignore: error TS2339: Property 'style' does not exist on type 'Menu'.
    const style = subMenu.style;

    style.marginTop = '0';  // crbug.com/1066727

    // The sub-menu needs to sit aligned to the top and side of
    // the menu-item passed in. It also needs to fit inside the viewport
    const itemRect = item.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not exist
    // on type 'Menu'.
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
   * @param {MenuItem} item The menu item.
   * @return {Menu|null}
   */
  getSubMenuFromItem(item) {
    if (!item) {
      return null;
    }
    const subMenuId = item.getAttribute('sub-menu');
    if (subMenuId === null) {
      return null;
    }
    return /** @type {!Menu|null} */ (document.querySelector(subMenuId));
  }

  /**
   * Display any sub-menu hanging off the current selection.
   */
  showSubMenu() {
    const item = this.selectedItem;
    // @ts-ignore: error TS2345: Argument of type 'MenuItem | null' is not
    // assignable to parameter of type 'MenuItem'.
    const subMenu = this.getSubMenuFromItem(item);
    if (subMenu) {
      this.subMenu = subMenu;
      // @ts-ignore: error TS18047: 'item' is possibly 'null'.
      item.setAttribute('sub-menu-shown', 'shown');
      // @ts-ignore: error TS2345: Argument of type 'MenuItem | null' is not
      // assignable to parameter of type 'MenuItem'.
      this.positionSubMenu_(item, subMenu);
      // @ts-ignore: error TS2339: Property 'show' does not exist on type
      // 'Menu'.
      subMenu.show();
      // @ts-ignore: error TS2339: Property 'parentMenuItem' does not exist on
      // type 'Menu'.
      subMenu.parentMenuItem = item;
      this.moveSelectionToSubMenu_(subMenu);
    }
  }

  /**
   * Find any ancestor menu item from a node.
   * TODO(adanilo) refactor this with the menu code to get rid of it.
   * @param {EventTarget} node The node to start searching from.
   * @return {?MenuItem} The found menu item or null.
   * @private
   */
  findMenuItem(node) {
    // @ts-ignore: error TS2339: Property 'parentNode' does not exist on type
    // 'EventTarget'.
    while (node && node.parentNode !== this && !(node instanceof MenuItem)) {
      // @ts-ignore: error TS2339: Property 'parentNode' does not exist on type
      // 'EventTarget'.
      node = node.parentNode;
    }
    return node ? assertInstanceof(node, MenuItem) : null;
  }

  /**
   * Find any sub-menu hanging off the event target and show/hide it.
   * @param {Event} e The event object.
   */
  manageSubMenu(e) {
    // @ts-ignore: error TS2345: Argument of type 'EventTarget | null' is not
    // assignable to parameter of type 'EventTarget'.
    const item = this.findMenuItem(e.target);
    // @ts-ignore: error TS2345: Argument of type 'MenuItem | null' is not
    // assignable to parameter of type 'MenuItem'.
    const subMenu = this.getSubMenuFromItem(item);
    if (!subMenu) {
      return;
    }
    this.subMenu = subMenu;
    switch (e.type) {
      case 'activate':
      case 'mouseover':
        // Hide any other sub menu being shown.
        const showing = /** @type {MenuItem} */ (
            // @ts-ignore: error TS2339: Property 'querySelector' does not exist
            // on type 'MultiMenu'.
            this.querySelector('cr-menu-item[sub-menu-shown]'));
        if (showing && showing !== item) {
          showing.removeAttribute('sub-menu-shown');
          const shownSubMenu = this.getSubMenuFromItem(showing);
          if (shownSubMenu) {
            // @ts-ignore: error TS2339: Property 'hide' does not exist on type
            // 'Menu'.
            shownSubMenu.hide();
          }
        }
        // @ts-ignore: error TS18047: 'item' is possibly 'null'.
        item.setAttribute('sub-menu-shown', 'shown');
        // @ts-ignore: error TS2345: Argument of type 'MenuItem | null' is not
        // assignable to parameter of type 'MenuItem'.
        this.positionSubMenu_(item, subMenu);
        // @ts-ignore: error TS2339: Property 'show' does not exist on type
        // 'Menu'.
        subMenu.show();
        break;
      case 'mouseout':
        // If we're on top of the sub-menu, we don't want to dismiss it
        // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not
        // exist on type 'Menu'.
        const childRect = subMenu.getBoundingClientRect();
        // @ts-ignore: error TS2339: Property 'clientX' does not exist on type
        // 'Event'.
        if (childRect.left <= e.clientX && e.clientX < childRect.right &&
            // @ts-ignore: error TS2339: Property 'clientY' does not exist on
            // type 'Event'.
            childRect.top <= e.clientY && e.clientY < childRect.bottom) {
          this.currentMenu = subMenu;
          break;
        }
        // @ts-ignore: error TS18047: 'item' is possibly 'null'.
        item.removeAttribute('sub-menu-shown');
        // @ts-ignore: error TS2339: Property 'hide' does not exist on type
        // 'Menu'.
        subMenu.hide();
        this.subMenu = null;
        this.currentMenu = this;
        break;
    }
  }

  /**
   * Change the selection from the top level menu to the first item
   * in the subMenu passed in.
   * @param {Menu} subMenu sub-menu that should take selection.
   * @private
   */
  moveSelectionToSubMenu_(subMenu) {
    this.selectedItem = null;
    this.currentMenu = subMenu;
    subMenu.selectedIndex = 0;
    // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
    // type 'Menu'.
    subMenu.focusSelectedItem();
  }

  /**
   * TODO(adanilo) Get rid of this - just used to satisfy closure.
   */
  actAsMenu_() {
    return this;
  }

  /**
   * Change the selection from the sub menu to the top level menu.
   * @param {Menu} subMenu sub-menu that should lose selection.
   * @private
   */
  moveSelectionToTopMenu_(subMenu) {
    // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on type
    // 'Menu'.
    subMenu.selectedItem = null;
    this.currentMenu = this;
    // @ts-ignore: error TS2339: Property 'parentMenuItem' does not exist on
    // type 'Menu'.
    this.selectedItem = subMenu.parentMenuItem;
    // @ts-ignore: error TS2352: Conversion of type 'this' to type 'Menu' may be
    // a mistake because neither type sufficiently overlaps with the other. If
    // this was intentional, convert the expression to 'unknown' first.
    const menu = /** @type {!Menu} */ (this.actAsMenu_());
    // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
    // type 'Menu'.
    menu.focusSelectedItem();
  }

  /**
   * Add event listeners to any sub menus.
   */
  addSubMenuListeners() {
    // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist on
    // type 'MultiMenu'.
    const items = this.querySelectorAll('cr-menu-item[sub-menu]');
    // @ts-ignore: error TS7006: Parameter 'menuItem' implicitly has an 'any'
    // type.
    items.forEach((menuItem) => {
      const subMenuId = menuItem.getAttribute('sub-menu');
      if (subMenuId) {
        const subMenu = document.querySelector(subMenuId);
        if (subMenu) {
          // @ts-ignore: error TS2345: Argument of type 'this' is not assignable
          // to parameter of type 'Function | EventListener'.
          this.showingEvents_.add(subMenu, 'activate', this);
        }
      }
    });
  }

  /** @param {{x: number, y: number}=} opt_mouseDownPos */
  show(opt_mouseDownPos) {
    this.show_(opt_mouseDownPos);
    // When the menu is shown we steal all keyboard events.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'MultiMenu'.
    const doc = /** @type {EventTarget} */ (this.ownerDocument);
    if (doc) {
      // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
      // parameter of type 'Function | EventListener'.
      this.showingEvents_.add(doc, 'keydown', this, true);
    }
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(this, 'activate', this, true);
    // Handle mouse-over to trigger sub menu opening on hover.
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(this, 'mouseover', this);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(this, 'mouseout', this);
    this.addSubMenuListeners();
  }

  /**
   * Hides any sub-menu that is active.
   */
  hideSubMenu_() {
    const items =
        // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist
        // on type 'MultiMenu'.
        this.querySelectorAll('cr-menu-item[sub-menu][sub-menu-shown]');
    // @ts-ignore: error TS7006: Parameter 'menuItem' implicitly has an 'any'
    // type.
    items.forEach((menuItem) => {
      const subMenuId = menuItem.getAttribute('sub-menu');
      if (subMenuId) {
        const subMenu = /** @type {!Menu|null} */
            (document.querySelector(subMenuId));
        if (subMenu) {
          // @ts-ignore: error TS2339: Property 'hide' does not exist on type
          // 'Menu'.
          subMenu.hide();
        }
        menuItem.removeAttribute('sub-menu-shown');
      }
    });
    this.currentMenu = this;
  }

  hide() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.showingEvents_.removeAll();
    // Hide any visible sub-menus first
    this.hideSubMenu_();
    this.hide_();
  }

  /**
   * Check if a DOM element is containd within the main top
   * level menu or any sub-menu hanging off the top level menu.
   * @param {Node} node Node being tested for containment.
   */
  contains(node) {
    return this.contains_(node) ||
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        (this.subMenu && this.subMenu.contains(node));
  }
}
