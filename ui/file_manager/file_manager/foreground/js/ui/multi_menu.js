// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {MenuItem} from 'chrome://resources/js/cr/ui/menu_item.m.js';
// #import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
// #import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
// #import {Menu} from 'chrome://resources/js/cr/ui/menu.m.js';
// #import {decorate} from 'chrome://resources/js/cr/ui.m.js';

cr.define('cr.ui', () => {
  /**
   * Creates a menu that supports sub-menus.
   *
   * This works almost identically to cr.ui.Menu apart from supporting
   * sub menus hanging off a <cr-menu-item> element. To add a sub menu
   * to a top level menu item, add a 'sub-menu' attribute which has as
   * its value an id selector for another <cr-menu> element.
   * (e.g. <cr-menu-item sub-menu="other-menu">).
   * @extends {cr.ui.Menu}
   * @implements {EventListener}
   */
  /* #export */ class MultiMenu {
    constructor() {
      /**
       * Whether a sub-menu is positioned on the left of its parent.
       * @private {?boolean} Used to direct the arrow key navigation.
       */
      this.subMenuOnLeft = null;

      /**
       * Property that hosts sub-menus for filling with overflow items.
       * @public {?cr.ui.Menu} Used for menu-items that overflow parent
       * menu.
       */
      this.overflow = null;

      /**
       * Reference to the menu that the user is currently navigating.
       * @private {?cr.ui.MultiMenu|cr.ui.Menu} Used to route events to the
       *     correct menu.
       */
      this.currentMenu = null;

      /** @private {?cr.ui.Menu} Sub menu being used. */
      this.subMenu = null;

      /** @private {?cr.ui.MenuItem} Menu item hosting a sub menu. */
      this.parentMenuItem = null;

      /** @private {?cr.ui.MenuItem} Selected item in a menu. */
      this.selectedItem = null;

      /**
       * Padding used when restricting menu height when the window is too small
       * to show the entire menu.
       * @private {number}
       */
      this.menuEndGap_ = 0;  // padding on cr.menu + 2px.

      /** @private {?cr.EventTracker} */
      this.showingEvents_ = null;

      /** TODO(adanilo) Annotate these for closure checking. */
      this.contains_ = undefined;
      this.handleKeyDown_ = undefined;
      this.hide_ = undefined;
      this.show_ = undefined;
    }

    /**
     * Initializes the multi menu.
     * @param {!Element} element Element to be decorated.
     * @return {!cr.ui.MultiMenu} Decorated element.
     */
    static decorate(element) {
      // Decorate the menu as a single level menu.
      cr.ui.decorate(element, cr.ui.Menu);
      // Grab cr.ui.Menu functions we want to override.
      // TODO(adanilo) Try to work around this suppress.
      // It's needed when monkey patching an in-built DOM method
      // that closure doesn't understand.
      /** @suppress {checkTypes} */
      element.contains_ = cr.ui.Menu.prototype['contains'];
      element.handleKeyDown_ = cr.ui.Menu.prototype['handleKeyDown'];
      element.hide_ = cr.ui.Menu.prototype['hide'];
      element.show_ = cr.ui.Menu.prototype['show'];
      // Add the MultiMenuButton methods to the element we're decorating.
      Object.getOwnPropertyNames(MultiMenu.prototype).forEach(name => {
        if (name !== 'constructor' &&
            !Object.getOwnPropertyDescriptor(element, name)) {
          element[name] = MultiMenu.prototype[name];
        }
      });
      element = /** @type {!cr.ui.MultiMenu} */ (element);
      element.decorate();
      return element;
    }

    decorate() {
      // Event tracker for the sub-menu specific listeners.
      this.showingEvents_ = new cr.EventTracker();
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
          if (e.currentTarget === this) {
            // Don't activate if there's a sub-menu to show
            const item = this.findMenuItem(e.target);
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
            const activationEvent = document.createEvent('Event');
            activationEvent.initEvent('activate', true, true);
            activationEvent.originalEvent = e.originalEvent;
            this.dispatchEvent(activationEvent);
          }
          break;
        case 'keydown':
          switch (e.key) {
            case 'ArrowLeft':
            case 'ArrowRight':
              if (!this.currentMenu) {
                break;
              }
              if (this.currentMenu === this) {
                const menuItem = this.currentMenu.selectedItem;
                const subMenu = this.getSubMenuFromItem(menuItem);
                if (subMenu) {
                  if (subMenu.hidden) {
                    break;
                  }
                  if (this.subMenuOnLeft && e.key == 'ArrowLeft') {
                    this.moveSelectionToSubMenu_(subMenu);
                  } else if (
                      this.subMenuOnLeft === false && e.key == 'ArrowRight') {
                    this.moveSelectionToSubMenu_(subMenu);
                  }
                }
              } else {
                const subMenu = /** @type {cr.ui.Menu} */ (this.currentMenu);
                // We only move off the sub-menu if we're on the top item
                if (subMenu.selectedIndex == 0) {
                  if (this.subMenuOnLeft && e.key == 'ArrowRight') {
                    this.moveSelectionToTopMenu_(subMenu);
                  } else if (
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
     * cr.ui.Menu has a handleKeyDown() method and to support
     * sub-menus we monkey patch the cr.ui.menu call via
     * this.handleKeyDown_() and if any sub menu is active, by
     * calling the cr.ui.Menu method directly.
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
        return this.currentMenu.handleKeyDown(e);
      }
    }

    /**
     * Position the sub menu adjacent to the cr-menu-item that triggered it.
     * @param {cr.ui.MenuItem} item The menu item to position against.
     * @param {cr.ui.Menu} subMenu The child (sub) menu to be positioned.
     */
    positionSubMenu_(item, subMenu) {
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
      style.maxHeight =
          (viewportHeight - itemRect.top - this.menuEndGap_) + 'px';
      // Let the browser deal with scroll bar generation.
      style.overflowY = 'auto';
    }

    /**
     * Get the subMenu hanging off a menu-item if it exists.
     * @param {cr.ui.MenuItem} item The menu item.
     * @return {cr.ui.Menu|null}
     */
    getSubMenuFromItem(item) {
      if (!item) {
        return null;
      }
      const subMenuId = item.getAttribute('sub-menu');
      if (subMenuId === null) {
        return null;
      }
      return /** @type {!cr.ui.Menu|null} */ (
          document.querySelector(subMenuId));
    }

    /**
     * Display any sub-menu hanging off the current selection.
     */
    showSubMenu() {
      const item = this.selectedItem;
      const subMenu = this.getSubMenuFromItem(item);
      if (subMenu) {
        this.subMenu = subMenu;
        item.setAttribute('sub-menu-shown', 'shown');
        this.positionSubMenu_(item, subMenu);
        subMenu.show();
        subMenu.parentMenuItem = item;
        this.moveSelectionToSubMenu_(subMenu);
      }
    }

    /**
     * Find any ancestor menu item from a node.
     * TODO(adanilo) refactor this with the menu code to get rid of it.
     * @param {EventTarget} node The node to start searching from.
     * @return {?cr.ui.MenuItem} The found menu item or null.
     * @private
     */
    findMenuItem(node) {
      /* #ignore */ const MenuItem = cr.ui.MenuItem;
      while (node && node.parentNode !== this && !(node instanceof MenuItem)) {
        node = node.parentNode;
      }
      return node ? assertInstanceof(node, MenuItem) : null;
    }

    /**
     * Find any sub-menu hanging off the event target and show/hide it.
     * @param {Event} e The event object.
     */
    manageSubMenu(e) {
      const item = this.findMenuItem(e.target);
      const subMenu = this.getSubMenuFromItem(item);
      if (!subMenu) {
        return;
      }
      this.subMenu = subMenu;
      switch (e.type) {
        case 'activate':
        case 'mouseover':
          // Hide any other sub menu being shown.
          const showing = /** @type {cr.ui.MenuItem} */ (
              this.querySelector('cr-menu-item[sub-menu-shown]'));
          if (showing && showing !== item) {
            showing.removeAttribute('sub-menu-shown');
            const shownSubMenu = this.getSubMenuFromItem(showing);
            if (shownSubMenu) {
              shownSubMenu.hide();
            }
          }
          item.setAttribute('sub-menu-shown', 'shown');
          this.positionSubMenu_(item, subMenu);
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
          item.removeAttribute('sub-menu-shown');
          subMenu.hide();
          this.subMenu = null;
          this.currentMenu = this;
          break;
      }
    }

    /**
     * Change the selection from the top level menu to the first item
     * in the subMenu passed in.
     * @param {cr.ui.Menu} subMenu sub-menu that should take selection.
     * @private
     */
    moveSelectionToSubMenu_(subMenu) {
      this.selectedItem = null;
      this.currentMenu = subMenu;
      subMenu.selectedIndex = 0;
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
     * @param {cr.ui.Menu} subMenu sub-menu that should lose selection.
     * @private
     */
    moveSelectionToTopMenu_(subMenu) {
      subMenu.selectedItem = null;
      this.currentMenu = this;
      this.selectedItem = subMenu.parentMenuItem;
      const menu = /** @type {!cr.ui.Menu} */ (this.actAsMenu_());
      menu.focusSelectedItem();
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
            this.showingEvents_.add(subMenu, 'activate', this);
          }
        }
      });
    }

    /** @param {{x: number, y: number}=} opt_mouseDownPos */
    show(opt_mouseDownPos) {
      this.show_(opt_mouseDownPos);
      // When the menu is shown we steal all keyboard events.
      const doc = /** @type {EventTarget} */ (this.ownerDocument);
      if (doc) {
        this.showingEvents_.add(doc, 'keydown', this, true);
      }
      this.showingEvents_.add(this, 'activate', this, true);
      // Handle mouse-over to trigger sub menu opening on hover.
      this.showingEvents_.add(this, 'mouseover', this);
      this.showingEvents_.add(this, 'mouseout', this);
      this.addSubMenuListeners();
    }

    /**
     * Hides any sub-menu that is active.
     */
    hideSubMenu_() {
      const items =
          this.querySelectorAll('cr-menu-item[sub-menu][sub-menu-shown]');
      items.forEach((menuItem) => {
        const subMenuId = menuItem.getAttribute('sub-menu');
        if (subMenuId) {
          const subMenu = /** @type {!cr.ui.Menu|null} */
              (document.querySelector(subMenuId));
          if (subMenu) {
            subMenu.hide();
          }
          menuItem.removeAttribute('sub-menu-shown');
        }
      });
      this.currentMenu = this;
    }

    hide() {
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
          (this.subMenu && this.subMenu.contains(node));
    }
  }

  // Export
  // #cr_define_end
  return {MultiMenu};
});
