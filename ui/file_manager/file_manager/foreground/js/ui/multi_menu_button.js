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

import {util} from '../../../common/js/util.js';

import {MultiMenu} from './multi_menu.js';


/**
 * A button that displays a MultiMenu (menu with sub-menus).
 * @extends {HTMLButtonElement}
 * @implements {EventListener}
 */
export class MultiMenuButton {
  constructor() {
    /**
     * Property that hosts sub-menus for filling with overflow items.
     * @public {Menu|null} Used for menu-items that overflow parent
     * menu.
     */
    this.overflow = null;

    /**
     * Padding used when restricting menu height when the window is too small
     * to show the entire menu.
     * @private {number}
     */
    this.menuEndGap_ = 0;  // padding on cr.menu + 2px

    /** @private {?EventTracker} */
    this.showingEvents_ = null;

    /** @private {?Menu} */
    this.menu_ = null;

    /** @private {?ResizeObserver} */
    this.observer_ = null;

    /** @private {?Element} */
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
    element = /** @type {!MultiMenuButton} */ (element);
    element.decorate();
    return element;
  }

  /**
   * Initializes the menu button.
   */
  decorate() {
    this.setAttribute('aria-expanded', 'false');

    // Listen to the touch events on the document so that we can handle it
    // before cancelled by other UI components.
    this.ownerDocument.addEventListener('touchstart', this, {passive: true});
    this.addEventListener('mousedown', this);
    this.addEventListener('keydown', this);
    this.addEventListener('dblclick', this);
    this.addEventListener('blur', this);

    this.menuEndGap_ = 18;  // padding on cr.menu + 2px

    // Adding the 'custom-appearance' class prevents widgets.css from
    // changing the appearance of this element.
    this.classList.add('custom-appearance');
    this.classList.add('menu-button');  // For styles in menu_button.css.

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
    return this.menu_;
  }
  setMenu_(menu) {
    if (typeof menu == 'string' && menu[0] == '#') {
      menu = assert(this.ownerDocument.body.querySelector(menu));
      decorate(menu, MultiMenu);
    }

    this.menu_ = menu;
    if (menu) {
      if (menu.id) {
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
    return e.target instanceof Node && !this.contains(e.target) &&
        !this.menu.contains(e.target);
  }

  /**
   * Display any sub-menu hanging off the current selection.
   */
  showSubMenu() {
    if (!this.isMenuShown()) {
      return;
    }
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
            // Prevent the button from stealing focus on mousedown unless
            // focus is on another button or cr-input element.
            if (!(document.hasFocus() &&
                  (document.activeElement.tagName === 'BUTTON' ||
                   document.activeElement.tagName === 'CR-BUTTON' ||
                   document.activeElement.tagName === 'CR-INPUT'))) {
              e.preventDefault();
            }
          }
        }

        // Hide the focus ring on mouse click.
        this.classList.add('using-mouse');
        break;
      case 'keydown':
        this.handleKeyDown(e);
        // If a menu is visible we let it handle keyboard events intended for
        // the menu.
        if (e.currentTarget == this.ownerDocument && this.hasVisibleMenu_() &&
            this.isMenuEvent(e)) {
          this.menu.handleKeyDown(e);
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
        const hideDelayed = e.target instanceof MenuItem && e.target.checkable;
        const hideType = hideDelayed ? HideType.DELAYED : HideType.INSTANT;
        // If the menu-item hosts a sub-menu, don't hide
        if (this.menu.getSubMenuFromItem(
                /** @type {!MenuItem} */ (e.target)) !== null) {
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
        if ((!this.menu || !this.menu.contains(e.target))) {
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

    this.menu.updateCommands(this);

    const event = new UIEvent(
        'menushow', {bubbles: true, cancelable: true, view: window});
    if (!this.dispatchEvent(event)) {
      return;
    }

    // Track element for which menu was opened so that command events are
    // dispatched to the correct element.
    this.menu.contextElement = this;
    this.menu.show(opt_mousePos);

    // Toggle aria and open state.
    this.setAttribute('aria-expanded', 'true');
    this.setAttribute('menu-shown', '');

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
    this.observedElement_ = this.parentElement;
    this.observer_.observe(assert(this.observedElement_));
    this.positionMenu_();

    if (shouldSetFocus) {
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
    this.setAttribute('aria-expanded', 'false');
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

    this.observer_.unobserve(assert(this.observedElement_));

    const event = new UIEvent(
        'menuhide', {bubbles: true, cancelable: false, view: window});
    this.dispatchEvent(event);
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
    const style = this.menu.style;

    style.marginTop = '8px';  // crbug.com/1066727
    // Clear any maxHeight we've set from previous calls into here.
    style.maxHeight = 'none';
    const invertLeftRight = false;
    /** @type {!AnchorType} */
    const anchorType = AnchorType.BELOW;
    positionPopupAroundElement(this, this.menu, anchorType, invertLeftRight);
    // Check if menu is larger than the viewport and adjust its height to
    // enable scrolling if so. Note: style.bottom would have been set to 0.
    const viewportHeight = window.innerHeight;
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
