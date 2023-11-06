// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {decorate} from '../../../common/js/ui.js';
import {Menu} from './menu.js';
import {HideType} from './menu_button.js';
import {MenuItem} from './menu_item.js';
import {AnchorType, positionPopupAroundElement} from './position_util.js';
import {EventTracker} from 'chrome://resources/ash/common/event_tracker.js';
import {MultiMenu} from './multi_menu.js';

/**
 * A button that displays a MultiMenu (menu with sub-menus).
 * @extends {HTMLButtonElement}
 * @implements {EventListener}
 */
// @ts-ignore: error TS2420: Class 'MultiMenuButton' incorrectly implements
// interface 'EventListener'.
export class MultiMenuButton {
  constructor() {
    /**
     * Property that hosts sub-menus for filling with overflow items.
     * @public @type {Menu|null} Used for menu-items that overflow parent
     * menu.
     */
    this.overflow = null;

    /**
     * Padding used when restricting menu height when the window is too small
     * to show the entire menu.
     * @private @type {number}
     */
    this.menuEndGap_ = 0;  // padding on cr.menu + 2px

    /** @private @type {?EventTracker} */
    this.showingEvents_ = null;

    /** @private @type {?Menu} */
    this.menu_ = null;

    /** @private @type {?ResizeObserver} */
    this.observer_ = null;

    /** @private @type {?Element} */
    this.observedElement_ = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * Decorates the element.
   * @param {!Element} element Element to be decorated.
   * @return {!MultiMenuButton} Decorated element.
   */
  static decorate(element) {
    // Add the MultiMenuButton methods to the element we're
    // decorating, leaving it's prototype chain intact.
    // Don't copy 'constructor' or property get/setters.
    Object.getOwnPropertyNames(MultiMenuButton.prototype).forEach(name => {
      if (name !== 'constructor' &&
          !Object.getOwnPropertyDescriptor(element, name)) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'MultiMenuButton'.
        element[name] = MultiMenuButton.prototype[name];
      }
    });
    // Set up the 'menu' property & setter/getter.
    Object.defineProperty(element, 'menu', {
      get() {
        return this.menu_;
      },
      set(menu) {
        this.setMenu_(menu);
      },
      enumerable: true,
      configurable: true,
    });
    // @ts-ignore: error TS2352: Conversion of type 'Element' to type
    // 'MultiMenuButton' may be a mistake because neither type sufficiently
    // overlaps with the other. If this was intentional, convert the expression
    // to 'unknown' first.
    element = /** @type {!MultiMenuButton} */ (element);
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Element'.
    element.decorate();
    // @ts-ignore: error TS2740: Type 'Element' is missing the following
    // properties from type 'MultiMenuButton': overflow, menuEndGap_,
    // showingEvents_, menu_, and 18 more.
    return element;
  }

  /**
   * Initializes the menu button.
   */
  decorate() {
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'MultiMenuButton'.
    this.setAttribute('aria-expanded', 'false');

    // Listen to the touch events on the document so that we can handle it
    // before cancelled by other UI components.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'MultiMenuButton'.
    this.ownerDocument.addEventListener('touchstart', this, {passive: true});
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type 'MultiMenuButton'.
    this.addEventListener('mousedown', this);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type 'MultiMenuButton'.
    this.addEventListener('keydown', this);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type 'MultiMenuButton'.
    this.addEventListener('dblclick', this);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type 'MultiMenuButton'.
    this.addEventListener('blur', this);

    this.menuEndGap_ = 18;  // padding on cr.menu + 2px

    // Adding the 'custom-appearance' class prevents widgets.css from
    // changing the appearance of this element.
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'MultiMenuButton'.
    this.classList.add('custom-appearance');
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'MultiMenuButton'.
    this.classList.add('menu-button');  // For styles in menu_button.css.

    // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on type
    // 'MultiMenuButton'.
    this.menu = this.getAttribute('menu');

    // Align the menu if the button moves. When the button moves, the parent
    // container resizes.
    this.observer_ = new ResizeObserver(() => {
      this.positionMenu_();
    });

    // An event tracker for events we only connect to while the menu is
    // displayed.
    this.showingEvents_ = new EventTracker();
  }

  /**
   * TODO(adanilo) Get rid of the getter/setter duplication.
   * The menu associated with the menu button.
   * @type {Menu}
   */
  get menu() {
    // @ts-ignore: error TS2322: Type 'Menu | null' is not assignable to type
    // 'Menu'.
    return this.menu_;
  }
  // @ts-ignore: error TS7006: Parameter 'menu' implicitly has an 'any' type.
  setMenu_(menu) {
    if (typeof menu == 'string' && menu[0] == '#') {
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type 'MultiMenuButton'.
      menu = assert(this.ownerDocument.body.querySelector(menu));
      decorate(menu, MultiMenu);
    }

    this.menu_ = menu;
    if (menu) {
      if (menu.id) {
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type 'MultiMenuButton'.
        this.setAttribute('menu', '#' + menu.id);
      }
    }
  }
  set menu(menu) {
    this.setMenu_(menu);
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
    // @ts-ignore: error TS2339: Property 'contains' does not exist on type
    // 'MultiMenuButton'.
    return e.target instanceof Node && !this.contains(e.target) &&
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        !this.menu.contains(e.target);
  }

  /**
   * Display any sub-menu hanging off the current selection.
   */
  showSubMenu() {
    if (!this.isMenuShown()) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'showSubMenu' does not exist on type
    // 'Menu'.
    this.menu.showSubMenu();
  }

  /**
   * Do we have a menu visible to handle a keyboard event.
   * @return {boolean} True if there's a visible menu.
   * @private
   */
  hasVisibleMenu_() {
    if (this.isMenuShown()) {
      return true;
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
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'MultiMenuButton'.
        if (e.currentTarget == this.ownerDocument) {
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
          } else if (e.button == 0) {  // Only show the menu when using left
                                       // mouse button.
            // @ts-ignore: error TS2339: Property 'screenY' does not exist on
            // type 'Event'.
            this.showMenu(false, {x: e.screenX, y: e.screenY});
            // Prevent the button from stealing focus on mousedown unless
            // focus is on another button or cr-input element.
            if (!(document.hasFocus() &&
                  // @ts-ignore: error TS18047: 'document.activeElement' is
                  // possibly 'null'.
                  (document.activeElement.tagName === 'BUTTON' ||
                   // @ts-ignore: error TS18047: 'document.activeElement' is
                   // possibly 'null'.
                   document.activeElement.tagName === 'CR-BUTTON' ||
                   // @ts-ignore: error TS18047: 'document.activeElement' is
                   // possibly 'null'.
                   document.activeElement.tagName === 'CR-INPUT'))) {
              e.preventDefault();
            }
          }
        }

        // Hide the focus ring on mouse click.
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'MultiMenuButton'.
        this.classList.add('using-mouse');
        break;
      case 'keydown':
        this.handleKeyDown(e);
        // If a menu is visible we let it handle keyboard events intended for
        // the menu.
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'MultiMenuButton'.
        if (e.currentTarget == this.ownerDocument && this.hasVisibleMenu_() &&
            this.isMenuEvent(e)) {
          // @ts-ignore: error TS2339: Property 'handleKeyDown' does not exist
          // on type 'Menu'.
          this.menu.handleKeyDown(e);
          e.preventDefault();
          e.stopPropagation();
        }

        // Show the focus ring on keypress.
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'MultiMenuButton'.
        this.classList.remove('using-mouse');
        break;
      case 'focus':
        if (this.shouldDismissMenu_(e)) {
          this.hideMenu();
          // Show the focus ring on focus - if it's come from a mouse event,
          // the focus ring will be hidden in the mousedown event handler,
          // executed after this.
          // @ts-ignore: error TS2339: Property 'classList' does not exist on
          // type 'MultiMenuButton'.
          this.classList.remove('using-mouse');
        }
        break;
      case 'blur':
        // No need to hide the focus ring anymore, without having focus.
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'MultiMenuButton'.
        this.classList.remove('using-mouse');
        break;
      case 'activate':
        const hideDelayed = e.target instanceof MenuItem && e.target.checkable;
        const hideType = hideDelayed ? HideType.DELAYED : HideType.INSTANT;
        // If the menu-item hosts a sub-menu, don't hide
        // @ts-ignore: error TS2339: Property 'getSubMenuFromItem' does not
        // exist on type 'Menu'.
        if (this.menu.getSubMenuFromItem(
                /** @type {!MenuItem} */ (e.target)) !== null) {
          break;
        }
        // @ts-ignore: error TS2339: Property 'originalEvent' does not exist on
        // type 'Event'.
        if (e.originalEvent instanceof MouseEvent ||
            // @ts-ignore: error TS2339: Property 'originalEvent' does not exist
            // on type 'Event'.
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
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        if ((!this.menu || !this.menu.contains(e.target))) {
          // @ts-ignore: error TS2339: Property 'screenY' does not exist on type
          // 'Event'.
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

    // @ts-ignore: error TS2339: Property 'updateCommands' does not exist on
    // type 'Menu'.
    this.menu.updateCommands(this);

    const event = new UIEvent(
        'menushow', {bubbles: true, cancelable: true, view: window});
    // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on type
    // 'MultiMenuButton'.
    if (!this.dispatchEvent(event)) {
      return;
    }

    // Track element for which menu was opened so that command events are
    // dispatched to the correct element.
    // @ts-ignore: error TS2339: Property 'contextElement' does not exist on
    // type 'Menu'.
    this.menu.contextElement = this;
    // @ts-ignore: error TS2339: Property 'show' does not exist on type 'Menu'.
    this.menu.show(opt_mousePos);

    // Toggle aria and open state.
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'MultiMenuButton'.
    this.setAttribute('aria-expanded', 'true');
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'MultiMenuButton'.
    this.setAttribute('menu-shown', '');

    // When the menu is shown we steal all keyboard events.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'MultiMenuButton'.
    const doc = this.ownerDocument;
    const win = assert(doc.defaultView);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'keydown', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'mousedown', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'focus', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'scroll', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(win, 'popstate', this);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(win, 'resize', this);
    // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(this.menu, 'contextmenu', this);
    // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(this.menu, 'activate', this);
    // @ts-ignore: error TS2339: Property 'parentElement' does not exist on type
    // 'MultiMenuButton'.
    this.observedElement_ = this.parentElement;
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Element'.
    this.observer_.observe(assert(this.observedElement_));
    this.positionMenu_();

    if (shouldSetFocus) {
      // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist
      // on type 'Menu'.
      this.menu.focusSelectedItem();
    }
  }

  /**
   * Hides the menu. If your menu can go out of scope, make sure to call this
   * first.
   * @param {HideType=} opt_hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  hideMenu(opt_hideType) {
    this.hideMenuInternal_(true, opt_hideType);
  }

  /**
   * Hides the menu. If your menu can go out of scope, make sure to call this
   * first.
   * @param {HideType=} opt_hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  hideMenuWithoutTakingFocus_(opt_hideType) {
    this.hideMenuInternal_(false, opt_hideType);
  }

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

    // Toggle aria and open state.
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'MultiMenuButton'.
    this.setAttribute('aria-expanded', 'false');
    // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
    // type 'MultiMenuButton'.
    this.removeAttribute('menu-shown');

    if (opt_hideType == HideType.DELAYED) {
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'Menu'.
      this.menu.classList.add('hide-delayed');
    } else {
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'Menu'.
      this.menu.classList.remove('hide-delayed');
    }
    // @ts-ignore: error TS2339: Property 'hide' does not exist on type 'Menu'.
    this.menu.hide();

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.showingEvents_.removeAll();
    if (shouldTakeFocus) {
      // @ts-ignore: error TS2339: Property 'focus' does not exist on type
      // 'MultiMenuButton'.
      this.focus();
    }

    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Element'.
    this.observer_.unobserve(assert(this.observedElement_));

    const event = new UIEvent(
        'menuhide', {bubbles: true, cancelable: false, view: window});
    // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on type
    // 'MultiMenuButton'.
    this.dispatchEvent(event);
  }

  /**
   * Whether the menu is shown.
   */
  isMenuShown() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // 'MultiMenuButton'.
    return this.hasAttribute('menu-shown');
  }

  /**
   * Positions the menu below the menu button. We check the menu fits
   * in the viewport, and enable scrolling if required.
   * @private
   */
  positionMenu_() {
    // @ts-ignore: error TS2339: Property 'style' does not exist on type 'Menu'.
    const style = this.menu.style;

    style.marginTop = '8px';  // crbug.com/1066727
    // Clear any maxHeight we've set from previous calls into here.
    style.maxHeight = 'none';
    const invertLeftRight = false;
    /** @type {!AnchorType} */
    const anchorType = AnchorType.BELOW;
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'HTMLElement'.
    positionPopupAroundElement(this, this.menu, anchorType, invertLeftRight);
    // Check if menu is larger than the viewport and adjust its height to
    // enable scrolling if so. Note: style.bottom would have been set to 0.
    const viewportHeight = window.innerHeight;
    // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not exist
    // on type 'Menu'.
    const menuRect = this.menu.getBoundingClientRect();
    // Limit the height to fit in the viewport.
    style.maxHeight = (viewportHeight - this.menuEndGap_) + 'px';
    // If the menu is too tall, position 2px from the bottom of the viewport
    // so users can see the end of the menu (helps when scroll is needed).
    if ((menuRect.height + this.menuEndGap_) > viewportHeight) {
      style.bottom = '2px';
    }
    // Let the browser deal with scroll bar generation.
    style.overflowY = 'auto';
  }

  /**
   * Handles the keydown event for the menu button.
   */
  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleKeyDown(e) {
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
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

  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  isMenuEvent(e) {
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
      case 'ArrowLeft':
      case 'ArrowRight':
      case 'Enter':
      case ' ':
      case 'Escape':
      case 'Tab':
        return true;
    }
    return false;
  }
}

MultiMenuButton.prototype.__proto__ = HTMLButtonElement.prototype;
