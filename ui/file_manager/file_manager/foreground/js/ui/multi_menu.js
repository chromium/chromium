// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('cr.ui');

cr.define('cr.ui', () => {
  /** @const */
  const HideType = cr.ui.HideType;

  /**
   * Creates a new menu button element.
   * @extends {HTMLButtonElement}
   * @implements {EventListener}
   */
  class MultiMenuButton {
    constructor() {
      /**
       * Whether to show the menu on press of the Up or Down arrow keys.
       * @private {boolean}
       */
      this.respondToArrowKeys = true;

      /**
       * Whether a sub-menu is positioned on the left of its parent.
       * @private {boolean|null} Used to direct the arrow key navigation.
       */
      this.subMenuOnLeft = null;

      /**
       * Property that hosts sub-menus for filling with overflow items.
       * @public {cr.ui.Menu|null} Used for menu-items that overflow parent
       * menu.
       */
      this.overflow = null;

      /**
       * Reference to the menu that the user is currently navigating.
       * @private {cr.ui.Menu|null} Used to route events to the correct menu.
       */
      this.currentMenu = null;

      /**
       * Padding used when restricting menu height when the window is too small
       * to show the entire menu.
       * @private {number}
       */
      this.menuEndGap_ = 0;  // padding on cr.menu + 2px

      /** @private {boolean} */
      this.invertLeftRight = false;

      /** @private {cr.ui.AnchorType} */
      this.anchorType = cr.ui.AnchorType.BELOW;

      /** @private {?Date|?number} */
      this.hideTimestamp_ = null;

      /** @private {?EventTracker} */
      this.showingEvents_ = null;

      /** @private {?cr.ui.Menu} */
      this.menu_ = null;

      throw new Error('Designed to decorate elements');
    }

    /**
     * Decorates the element.
     * @param {!Element} element Element to be decorated.
     * @return {!cr.ui.MultiMenuButton} Decorated element.
     */
    static decorate(element) {
      element.__proto__ = MultiMenuButton.prototype;
      element = /** @type {!cr.ui.MultiMenuButton} */ (element);
      element.decorate();
      return element;
    }

    /**
     * Initializes the menu button.
     */
    decorate() {
      // Listen to the touch events on the document so that we can handle it
      // before cancelled by other UI components.
      this.ownerDocument.addEventListener('touchstart', this, {passive: true});
      this.addEventListener('mousedown', this);
      this.addEventListener('keydown', this);
      this.addEventListener('dblclick', this);
      this.addEventListener('blur', this);

      this.menuEndGap_ = 18;  // padding on cr.menu + 2px
      this.respondToArrowKeys = true;

      // Adding the 'custom-appearance' class prevents widgets.css from
      // changing the appearance of this element.
      this.classList.add('custom-appearance');
      this.classList.add('menu-button');  // For styles in menu_button.css.

      let menu;
      if ((menu = this.getAttribute('menu'))) {
        this.menu = menu;
      }

      // An event tracker for events we only connect to while the menu is
      // displayed.
      this.showingEvents_ = new EventTracker();

      this.anchorType = cr.ui.AnchorType.BELOW;
      this.invertLeftRight = false;
    }

    /**
     * The menu associated with the menu button.
     * @type {cr.ui.Menu}
     */
    get menu() {
      return this.menu_;
    }
    set menu(menu) {
      if (typeof menu == 'string' && menu[0] == '#') {
        menu = assert(this.ownerDocument.body.querySelector(menu));
        cr.ui.decorate(menu, cr.ui.Menu);
      }

      this.menu_ = menu;
      if (menu) {
        if (menu.id) {
          this.setAttribute('menu', '#' + menu.id);
        }
      }
    }

    /**
     * Checks if the menu(s) should be closed based on the target of a mouse
     * click or a touch event target.
     * @param {Event} e The event object.
     * @return {boolean}
     * @private
     */
    shouldDismissMenu_(e) {
      // All menus are dismissed when clicking outside the menus. If we are
      // showing a sub-menu, we need to detect if the target is the top
      // level menu, or in the sub menu when the sub menu is being shown.
      // The button is excluded here because it should toggle show/hide the
      // menu and handled separately.
      return e.target instanceof Node && !this.contains(e.target) &&
          !this.menu.contains(e.target) &&
          !(this.menu.subMenu && this.menu.subMenu.contains(e.target));
    }

    /**
     * Position the sub menu adjacent to the cr-menu-item that triggered it.
     * @param {cr.ui.MenuItem} item The menu item to position against.
     * @param {cr.ui.Menu} subMenu The child (sub) menu to be positioned.
     */
    positionSubMenu_(item, subMenu) {
      // The sub-menu needs to sit aligned to the top and side of
      // the menu-item passed in. It also needs to fit inside the viewport
      const itemRect = item.getBoundingClientRect();
      const viewportWidth = window.innerWidth;
      const viewportHeight = window.innerHeight;
      const childRect = subMenu.getBoundingClientRect();
      const style = subMenu.style;
      // See if it fits on the right, if not position on the left
      style.left = style.right = style.top = style.bottom = 'auto';
      if ((itemRect.right + childRect.width) > viewportWidth) {
        this.subMenuOnLeft = true;
        style.left = (itemRect.left - childRect.width) + 'px';
      } else {
        this.subMenuOnLeft = false;
        style.left = itemRect.right + 'px';
      }
      style.top = itemRect.top + 'px';
      // Size the subMenu to fit inside the height of the viewport
      // Always set the maximum height so that expanding the window
      // allows the menu height to grow crbug/934207
      style.maxHeight =
          (viewportHeight - itemRect.top - this.menuEndGap_) + 'px';
      if ((itemRect.top + childRect.height + this.menuEndGap_) >
          viewportHeight) {
        style.overflowY = 'scroll';
      } else {
        style.overflowY = 'auto';
      }
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
      if (!this.isMenuShown()) {
        return;
      }
      const item = this.menu.selectedItem;
      const subMenu = this.getSubMenuFromItem(item);
      if (subMenu) {
        this.menu.subMenu = subMenu;
        item.setAttribute('sub-menu-shown', 'shown');
        this.positionSubMenu_(item, subMenu);
        subMenu.show();
        subMenu.parentMenuItem = item;
        this.moveSelectionToSubMenu_(subMenu);
      }
    }

    /**
     * Find any sub-menu hanging off the event target and show/hide it.
     * @param {Event} e The event object.
     */
    manageSubMenu(e) {
      const item = this.menu.findMenuItem_(e.target);
      const subMenu = this.getSubMenuFromItem(item);
      if (!subMenu) {
        return;
      }
      this.menu.subMenu = subMenu;
      switch (e.type) {
        case 'mouseover':
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
          this.menu.subMenu = null;
          this.currentMenu = this.menu;
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
      this.menu.selectedItem = null;
      this.currentMenu = subMenu;
      subMenu.selectedIndex = 0;
      subMenu.focusSelectedItem();
    }

    /**
     * Change the selection from the sub menu to the top level menu.
     * @param {cr.ui.Menu} subMenu sub-menu that should lose selection.
     * @private
     */
    moveSelectionToTopMenu_(subMenu) {
      subMenu.selectedItem = null;
      this.currentMenu = this.menu;
      this.menu.selectedItem = subMenu.parentMenuItem;
      this.menu.focusSelectedItem();
    }

    /**
     * Do we have a menu visible to handle a keyboard event.
     * @return {boolean} True if there's a visible menu.
     * @private
     */
    hasVisibleMenu_() {
      if (this.currentMenu == this.menu && this.isMenuShown()) {
        return true;
      } else if (this.currentMenu) {
        if (this.currentMenu.parentMenuItem.hasAttribute('sub-menu-shown')) {
          return true;
        }
      }
      return false;
    }

    /**
     * Handles event callbacks.
     * @param {Event} e The event object.
     */
    handleEvent(e) {
      if (!this.menu) {
        return;
      }

      switch (e.type) {
        case 'touchstart':
          // Touch on the menu button itself is ignored to avoid that the menu
          // opened again by the mousedown event following the touch events.
          if (this.shouldDismissMenu_(e)) {
            this.hideMenuWithoutTakingFocus_();
          }
          break;
        case 'mousedown':
          if (e.currentTarget == this.ownerDocument) {
            if (this.shouldDismissMenu_(e)) {
              this.hideMenuWithoutTakingFocus_();
            } else {
              e.preventDefault();
            }
          } else {
            if (this.isMenuShown()) {
              this.hideMenuWithoutTakingFocus_();
            } else if (e.button == 0) {  // Only show the menu when using left
                                         // mouse button.
              this.showMenu(false, {x: e.screenX, y: e.screenY});

              // Prevent the button from stealing focus on mousedown.
              e.preventDefault();
            }
          }

          // Hide the focus ring on mouse click.
          this.classList.add('using-mouse');
          break;
        case 'keydown':
          switch (e.key) {
            case 'ArrowLeft':
            case 'ArrowRight':
              if (!this.currentMenu) {
                break;
              }
              if (this.currentMenu === this.menu) {
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
                const subMenu = this.currentMenu;
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
          }
          this.handleKeyDown(e);
          // If a menu is visible we let it handle all the keyboard events.
          if (e.currentTarget == this.ownerDocument && this.hasVisibleMenu_()) {
            this.currentMenu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }

          // Show the focus ring on keypress.
          this.classList.remove('using-mouse');
          break;
        case 'focus':
          if (this.shouldDismissMenu_(e)) {
            this.hideMenu();
            // Show the focus ring on focus - if it's come from a mouse event,
            // the focus ring will be hidden in the mousedown event handler,
            // executed after this.
            this.classList.remove('using-mouse');
          }
          break;
        case 'blur':
          // No need to hide the focus ring anymore, without having focus.
          this.classList.remove('using-mouse');
          break;
        case 'activate':
          const hideDelayed =
              e.target instanceof cr.ui.MenuItem && e.target.checkable;
          const hideType = hideDelayed ? HideType.DELAYED : HideType.INSTANT;
          // If the menu-item hosts a sub-menu, don't hide
          if (this.getSubMenuFromItem(
                  /** @type {!cr.ui.MenuItem} */ (e.target)) !== null) {
            break;
          }
          if (e.originalEvent instanceof MouseEvent ||
              e.originalEvent instanceof TouchEvent) {
            this.hideMenuWithoutTakingFocus_(hideType);
          } else {
            // Keyboard. Take focus to continue keyboard operation.
            this.hideMenu(hideType);
          }
          break;
        case 'popstate':
        case 'resize':
          this.hideMenu();
          break;
        case 'contextmenu':
          if ((!this.menu || !this.menu.contains(e.target)) &&
              (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50)) {
            this.showMenu(true, {x: e.screenX, y: e.screenY});
          }
          e.preventDefault();
          // Don't allow elements further up in the DOM to show their menus.
          e.stopPropagation();
          break;
        case 'dblclick':
          // Don't allow double click events to propagate.
          e.preventDefault();
          e.stopPropagation();
          break;
        case 'mouseover':
        case 'mouseout':
          this.manageSubMenu(e);
          break;
      }
    }

    /**
     * Add event listeners to any sub menus.
     */
    addSubMenuListeners() {
      const items = this.menu.querySelectorAll('cr-menu-item[sub-menu]');
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

    /**
     * Shows the menu.
     * @param {boolean} shouldSetFocus Whether to set focus on the
     *     selected menu item.
     * @param {{x: number, y: number}=} opt_mousePos The position of the mouse
     *     when shown (in screen coordinates).
     */
    showMenu(shouldSetFocus, opt_mousePos) {
      this.hideMenu();

      this.menu.updateCommands(this);

      const event = new UIEvent(
          'menushow', {bubbles: true, cancelable: true, view: window});
      if (!this.dispatchEvent(event)) {
        return;
      }

      this.menu.show(opt_mousePos);

      this.setAttribute('menu-shown', '');
      // Handle mouse-over to trigger sub menu opening on hover.
      this.showingEvents_.add(this.menu, 'mouseover', this);
      this.showingEvents_.add(this.menu, 'mouseout', this);
      this.addSubMenuListeners();

      // When the menu is shown we steal all keyboard events.
      const doc = this.ownerDocument;
      const win = assert(doc.defaultView);
      this.showingEvents_.add(doc, 'keydown', this, true);
      this.showingEvents_.add(doc, 'mousedown', this, true);
      this.showingEvents_.add(doc, 'focus', this, true);
      this.showingEvents_.add(doc, 'scroll', this, true);
      this.showingEvents_.add(win, 'popstate', this);
      this.showingEvents_.add(win, 'resize', this);
      this.showingEvents_.add(this.menu, 'contextmenu', this);
      this.showingEvents_.add(this.menu, 'activate', this);
      this.positionMenu_();

      if (shouldSetFocus) {
        this.menu.focusSelectedItem();
      }
      this.currentMenu = this.menu;
    }

    /**
     * Hides any sub-menu that is active.
     */
    hideSubMenu_() {
      const items =
          this.menu.querySelectorAll('cr-menu-item[sub-menu][sub-menu-shown]');
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
      this.currentMenu = this.menu;
    }

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {cr.ui.HideType=} opt_hideType Type of hide.
     *     default: cr.ui.HideType.INSTANT.
     */
    hideMenu(opt_hideType) {
      this.hideMenuInternal_(true, opt_hideType);
    }

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {cr.ui.HideType=} opt_hideType Type of hide.
     *     default: cr.ui.HideType.INSTANT.
     */
    hideMenuWithoutTakingFocus_(opt_hideType) {
      this.hideMenuInternal_(false, opt_hideType);
    }

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {boolean} shouldTakeFocus Moves the focus to the button if true.
     * @param {cr.ui.HideType=} opt_hideType Type of hide.
     *     default: cr.ui.HideType.INSTANT.
     */
    hideMenuInternal_(shouldTakeFocus, opt_hideType) {
      if (!this.isMenuShown()) {
        return;
      }

      // Hide any visible sub-menus first
      this.hideSubMenu_();

      this.removeAttribute('menu-shown');
      if (opt_hideType == HideType.DELAYED) {
        this.menu.classList.add('hide-delayed');
      } else {
        this.menu.classList.remove('hide-delayed');
      }
      this.menu.hide();

      this.showingEvents_.removeAll();
      if (shouldTakeFocus) {
        this.focus();
      }

      const event = new UIEvent(
          'menuhide', {bubbles: true, cancelable: false, view: window});
      this.dispatchEvent(event);

      // On windows we might hide the menu in a right mouse button up and if
      // that is the case we wait some short period before we allow the menu
      // to be shown again.
      this.hideTimestamp_ = cr.isWindows ? Date.now() : 0;
      this.currentMenu = null;
    }

    /**
     * Whether the menu is shown.
     */
    isMenuShown() {
      return this.hasAttribute('menu-shown');
    }

    /**
     * Positions the menu below the menu button. We check the menu fits
     * in the viewport, and enable scrolling if required.
     * @private
     */
    positionMenu_() {
      cr.ui.positionPopupAroundElement(
          this, this.menu, this.anchorType, this.invertLeftRight);
      // Check if menu is larger than the viewport and adjust its height
      // and enable scrolling if so. Note: style.bottom would have been set to
      // 0.
      const viewportHeight = window.innerHeight;
      const menuRect = this.menu.getBoundingClientRect();
      const style = this.menu.style;
      // Make sure the top of the menu is in the viewport.
      let top = menuRect.top;
      if (top < 0) {
        top = 0;
      }
      // Limit the height to fit in the viewport.
      style.maxHeight = (viewportHeight - top - this.menuEndGap_) + 'px';
      // Make the menu scrollable if needed.
      if ((top + menuRect.height + this.menuEndGap_) > viewportHeight) {
        style.overflowY = 'scroll';
        style.top = '0';
        style.bottom = 'auto';
      } else {
        style.overflowY = 'auto';
      }
    }

    /**
     * Handles the keydown event for the menu button.
     */
    handleKeyDown(e) {
      switch (e.key) {
        case 'ArrowDown':
        case 'ArrowUp':
          if (!this.respondToArrowKeys) {
            break;
          }
          // Hide any showing sub-menu if we're moving in the parent
          if (this.currentMenu === this.menu) {
            this.hideSubMenu_();
          }
        case 'Enter':
        case ' ':
          if (!this.isMenuShown()) {
            this.showMenu(true);
          }
          e.preventDefault();
          break;
        case 'Escape':
        case 'Tab':
          this.hideMenu();
          break;
      }
    }
  }

  MultiMenuButton.prototype.__proto__ = HTMLButtonElement.prototype;

  // Export
  return {
    MultiMenuButton: MultiMenuButton,
  };
});
