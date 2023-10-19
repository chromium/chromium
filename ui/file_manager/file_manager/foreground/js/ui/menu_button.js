// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {EventTracker} from 'chrome://resources/ash/common/event_tracker.js';
import {define as crUiDefine, decorate} from '../../../common/js/ui.js';
import {positionPopupAroundElement, AnchorType} from './position_util.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';

  /**
   * Enum for type of hide. Delayed is used when called by clicking on a
   * checkable menu item.
   * @enum {number}
   */
  export const HideType = {
    INSTANT: 0,
    DELAYED: 1,
  };

  /** @const */

  /**
   * Creates a new menu button element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLButtonElement}
   * @implements {EventListener}
   */
  // @ts-ignore: error TS8022: JSDoc '@implements' is not attached to a class.
  export const MenuButton = crUiDefine('button');

  MenuButton.prototype = {
    __proto__: HTMLButtonElement.prototype,

    /**
     * Initializes the menu button.
     */
    decorate() {
      // Listen to the touch events on the document so that we can handle it
      // before cancelled by other UI components.
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.ownerDocument.addEventListener('touchstart', this);
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.addEventListener('mousedown', this);
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.addEventListener('keydown', this);
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.addEventListener('dblclick', this);
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.addEventListener('blur', this);

      // Adding the 'custom-appearance' class prevents widgets.css from changing
      // the appearance of this element.
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.classList.add('custom-appearance');
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.classList.add('menu-button');  // For styles in menu_button.css.

      let menu;
      // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      if ((menu = this.getAttribute('menu'))) {
        this.menu = menu;
      }

      // An event tracker for events we only connect to while the menu is
      // displayed.
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_ = new EventTracker();

      // @ts-ignore: error TS2339: Property 'anchorType' does not exist on type
      // '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.anchorType = AnchorType.BELOW;
      // @ts-ignore: error TS2339: Property 'invertLeftRight' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.invertLeftRight = false;
    },

    /**
     * The menu associated with the menu button.
     * @type {Menu}
     */
    get menu() {
      // @ts-ignore: error TS2551: Property 'menu_' does not exist on type '{
      // __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'. Did you mean 'menu'?
      return this.menu_;
    },
    set menu(menu) {
      if (typeof menu === 'string' && menu[0] === '#') {
        // @ts-ignore: error TS2339: Property 'slice' does not exist on type
        // 'never'.
        menu = assert(this.ownerDocument.getElementById(menu.slice(1)));
        // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable
        // to parameter of type 'string | Element'.
        decorate(menu, Menu);
      }

      // @ts-ignore: error TS2551: Property 'menu_' does not exist on type '{
      // __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'. Did you mean 'menu'?
      this.menu_ = menu;
      if (menu) {
        // @ts-ignore: error TS2339: Property 'id' does not exist on type
        // 'Menu'.
        if (menu.id) {
          // @ts-ignore: error TS2339: Property 'id' does not exist on type
          // 'Menu'.
          this.setAttribute('menu', '#' + menu.id);
        }
      }
    },

    /**
     * Whether to show the menu on press of the Up or Down arrow keys.
     */
    respondToArrowKeys: true,

    /**
     * Checks if the menu should be closed based on the target of a mouse click
     * or a touch event target.
     * @param {Event} e The event object.
     * @return {boolean}
     * @private
     */
    shouldDismissMenu_(e) {
      // The menu is dismissed when clicking outside the menu.
      // The button is excluded here because it should toggle show/hide the
      // menu and handled separately.
      // @ts-ignore: error TS2339: Property 'contains' does not exist on type '{
      // __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      return e.target instanceof Node && !this.contains(e.target) &&
          // @ts-ignore: error TS2339: Property 'contains' does not exist on
          // type 'Menu'.
          !this.menu.contains(e.target);
    },

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
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type '{ __proto__: HTMLButtonElement; decorate(): void; menu:
          // Menu; respondToArrowKeys: boolean; shouldDismissMenu_(e: Event):
          // boolean; handleEvent(e: Event): void; ... 6 more ...;
          // handleKeyDown(e: any): void; }'.
          if (e.currentTarget === this.ownerDocument) {
            if (this.shouldDismissMenu_(e)) {
              this.hideMenuWithoutTakingFocus_();
            } else {
              e.preventDefault();
            }
          } else {
            if (this.isMenuShown()) {
              this.hideMenuWithoutTakingFocus_();
              // @ts-ignore: error TS2339: Property 'button' does not exist on
              // type 'Event'.
            } else if (e.button === 0) {  // Only show the menu when using left
                                          // mouse button.
              // @ts-ignore: error TS2339: Property 'screenY' does not exist on
              // type 'Event'.
              this.showMenu(false, {x: e.screenX, y: e.screenY});

              // Prevent the button from stealing focus on mousedown.
              e.preventDefault();
            }
          }

          // Hide the focus ring on mouse click.
          // @ts-ignore: error TS2339: Property 'classList' does not exist on
          // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
          // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
          // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
          // void; }'.
          this.classList.add('using-mouse');
          break;
        case 'keydown':
          this.handleKeyDown(e);
          // If the menu is visible we let it handle all the keyboard events.
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type '{ __proto__: HTMLButtonElement; decorate(): void; menu:
          // Menu; respondToArrowKeys: boolean; shouldDismissMenu_(e: Event):
          // boolean; handleEvent(e: Event): void; ... 6 more ...;
          // handleKeyDown(e: any): void; }'.
          if (this.isMenuShown() && e.currentTarget === this.ownerDocument) {
            // @ts-ignore: error TS2339: Property 'handleKeyDown' does not exist
            // on type 'Menu'.
            this.menu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }

          // Show the focus ring on keypress.
          // @ts-ignore: error TS2339: Property 'classList' does not exist on
          // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
          // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
          // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
          // void; }'.
          this.classList.remove('using-mouse');
          break;
        case 'focus':
          if (this.shouldDismissMenu_(e)) {
            this.hideMenu();
            // Show the focus ring on focus - if it's come from a mouse event,
            // the focus ring will be hidden in the mousedown event handler,
            // executed after this.
            // @ts-ignore: error TS2339: Property 'classList' does not exist on
            // type '{ __proto__: HTMLButtonElement; decorate(): void; menu:
            // Menu; respondToArrowKeys: boolean; shouldDismissMenu_(e: Event):
            // boolean; handleEvent(e: Event): void; ... 6 more ...;
            // handleKeyDown(e: any): void; }'.
            this.classList.remove('using-mouse');
          }
          break;
        case 'blur':
          // No need to hide the focus ring anymore, without having focus.
          // @ts-ignore: error TS2339: Property 'classList' does not exist on
          // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
          // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
          // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
          // void; }'.
          this.classList.remove('using-mouse');
          break;
        case 'activate':
          const hideDelayed =
              e.target instanceof MenuItem && e.target.checkable;
          const hideType = hideDelayed ? HideType.DELAYED : HideType.INSTANT;
          // @ts-ignore: error TS2339: Property 'originalEvent' does not exist
          // on type 'Event'.
          if (e.originalEvent instanceof MouseEvent ||
              // @ts-ignore: error TS2339: Property 'originalEvent' does not
              // exist on type 'Event'.
              e.originalEvent instanceof TouchEvent) {
            this.hideMenuWithoutTakingFocus_(hideType);
          } else {
            // Keyboard. Take focus to continue keyboard operation.
            this.hideMenu(hideType);
          }
          break;
        case 'scroll':
          // @ts-ignore: error TS2339: Property 'contains' does not exist on
          // type 'Menu'.
          if (!(e.target === this.menu || this.menu.contains(e.target))) {
            this.hideMenu();
          }
          break;
        case 'popstate':
        case 'resize':
          this.hideMenu();
          break;
        case 'contextmenu':
          // @ts-ignore: error TS2339: Property 'contains' does not exist on
          // type 'Menu'.
          if ((!this.menu || !this.menu.contains(e.target)) &&
              // @ts-ignore: error TS2339: Property 'hideTimestamp_' does not
              // exist on type '{ __proto__: HTMLButtonElement; decorate():
              // void; menu: Menu; respondToArrowKeys: boolean;
              // shouldDismissMenu_(e: Event): boolean; handleEvent(e: Event):
              // void; ... 6 more ...; handleKeyDown(e: any): void; }'.
              (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50)) {
            // @ts-ignore: error TS2339: Property 'screenY' does not exist on
            // type 'Event'.
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
      }
    },

    /**
     * Shows the menu.
     * @param {boolean} shouldSetFocus Whether to set focus on the
     *     selected menu item.
     * @param {{x: number, y: number}=} opt_mousePos The position of the mouse
     *     when shown (in screen coordinates).
     */
    showMenu(shouldSetFocus, opt_mousePos) {
      this.hideMenu();

      // @ts-ignore: error TS2339: Property 'updateCommands' does not exist on
      // type 'Menu'.
      this.menu.updateCommands(this);

      const event = new UIEvent(
          'menushow', {bubbles: true, cancelable: true, view: window});
      // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      if (!this.dispatchEvent(event)) {
        return;
      }

      // @ts-ignore: error TS2339: Property 'show' does not exist on type
      // 'Menu'.
      this.menu.show(opt_mousePos);

      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.setAttribute('menu-shown', '');

      // When the menu is shown we steal all keyboard events.
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      const doc = this.ownerDocument;
      const win = doc.defaultView;
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(doc, 'keydown', this, true);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(doc, 'mousedown', this, true);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(doc, 'focus', this, true);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(doc, 'scroll', this, true);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(win, 'popstate', this);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(win, 'resize', this);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(this.menu, 'contextmenu', this);
      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.add(this.menu, 'activate', this);
      this.positionMenu_();

      if (shouldSetFocus) {
        // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist
        // on type 'Menu'.
        this.menu.focusSelectedItem();
      }
    },

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {HideType=} opt_hideType Type of hide.
     *     default: HideType.INSTANT.
     */
    hideMenu(opt_hideType) {
      this.hideMenuInternal_(true, opt_hideType);
    },

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {HideType=} opt_hideType Type of hide.
     *     default: HideType.INSTANT.
     */
    hideMenuWithoutTakingFocus_(opt_hideType) {
      this.hideMenuInternal_(false, opt_hideType);
    },

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {boolean} shouldTakeFocus Moves the focus to the button if true.
     * @param {HideType=} opt_hideType Type of hide.
     *     default: HideType.INSTANT.
     */
    hideMenuInternal_(shouldTakeFocus, opt_hideType) {
      if (!this.isMenuShown()) {
        return;
      }

      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.removeAttribute('menu-shown');
      if (opt_hideType === HideType.DELAYED) {
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'Menu'.
        this.menu.classList.add('hide-delayed');
      } else {
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'Menu'.
        this.menu.classList.remove('hide-delayed');
      }
      // @ts-ignore: error TS2339: Property 'hide' does not exist on type
      // 'Menu'.
      this.menu.hide();

      // @ts-ignore: error TS2339: Property 'showingEvents_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.showingEvents_.removeAll();
      if (shouldTakeFocus) {
        // @ts-ignore: error TS2339: Property 'focus' does not exist on type '{
        // __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
        // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
        // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
        // void; }'.
        this.focus();
      }

      const event = new UIEvent(
          'menuhide', {bubbles: true, cancelable: false, view: window});
      // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.dispatchEvent(event);

      // @ts-ignore: error TS2339: Property 'hideTimestamp_' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      this.hideTimestamp_ = 0;
    },

    /**
     * Whether the menu is shown.
     */
    isMenuShown() {
      // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on
      // type '{ __proto__: HTMLButtonElement; decorate(): void; menu: Menu;
      // respondToArrowKeys: boolean; shouldDismissMenu_(e: Event): boolean;
      // handleEvent(e: Event): void; ... 6 more ...; handleKeyDown(e: any):
      // void; }'.
      return this.hasAttribute('menu-shown');
    },

    /**
     * Positions the menu below the menu button. At this point we do not use any
     * advanced positioning logic to ensure the menu fits in the viewport.
     * @private
     */
    positionMenu_() {
      positionPopupAroundElement(
          // @ts-ignore: error TS2339: Property 'invertLeftRight' does not exist
          // on type '{ __proto__: HTMLButtonElement; decorate(): void; menu:
          // Menu; respondToArrowKeys: boolean; shouldDismissMenu_(e: Event):
          // boolean; handleEvent(e: Event): void; ... 6 more ...;
          // handleKeyDown(e: any): void; }'.
          this, this.menu, this.anchorType, this.invertLeftRight);
    },

    /**
     * Handles the keydown event for the menu button.
     */
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    handleKeyDown(e) {
      switch (e.key) {
        case 'ArrowDown':
        case 'ArrowUp':
          if (!this.respondToArrowKeys) {
            break;
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
    },
  };
