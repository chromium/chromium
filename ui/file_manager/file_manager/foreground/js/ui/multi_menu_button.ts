// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import type {Menu} from './menu.js';
import {MenuItem, type MenuItemActivationEvent} from './menu_item.js';
import {MultiMenu} from './multi_menu.js';
import {AnchorType, positionPopupAroundElement} from './position_util.js';

/**
 * Enum for type of hide. Delayed is used when called by clicking on a
 * checkable menu item.
 */
export enum HideType {
  INSTANT = 0,
  DELAYED = 1,
}

/**
 * A button that displays a MultiMenu (menu with sub-menus).
 */
export class MultiMenuButton extends CrButtonElement {
  /**
   * Property that hosts sub-menus for filling with overflow items.
   * Used for menu-items that overflow parent menu.
   */
  overflow: Menu|null = null;

  /**
   * Padding used when restricting menu height when the window is too small
   * to show the entire menu.
   */
  private menuEndGap_: number = 0;  // padding on cr.menu + 2px

  private abortController_: AbortController|null = null;
  private menu_: MultiMenu|null = null;
  private observer_: ResizeObserver|null = null;
  private observedElement_: HTMLElement|null = null;

  /**
   * Initializes the menu button.
   */
  initialize() {
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

    this.menu = this.getAttribute('menu')!;

    // Align the menu if the button moves. When the button moves, the parent
    // container resizes.
    this.observer_ = new ResizeObserver(() => {
      this.positionMenu_();
    });
  }

  get menu(): MultiMenu|null {
    return this.menu_;
  }

  private setMenu_(menu: string|MultiMenu) {
    if (typeof menu === 'string' && menu[0] === '#') {
      menu = this.ownerDocument.body.querySelector<MultiMenu>(menu)!;
      assert(menu);
      crInjectTypeAndInit(menu, MultiMenu);
    }

    this.menu_ = menu as MultiMenu;
    if (menu) {
      if ('id' in this.menu_) {
        this.setAttribute('menu', '#' + this.menu_.id);
      }
    }
  }

  set menu(menu: string|MultiMenu) {
    this.setMenu_(menu);
  }

  /**
   * Checks if the menu(s) should be closed based on the target of a mouse
   * click or a touch event target.
   * @param e The event object.
   */
  private shouldDismissMenu_(e: Event): boolean {
    // All menus are dismissed when clicking outside the menus. If we are
    // showing a sub-menu, we need to detect if the target is the top
    // level menu, or in the sub menu when the sub menu is being shown.
    // The button is excluded here because it should toggle show/hide the
    // menu and handled separately.
    return e.target instanceof Node && !this.contains(e.target) &&
        !this.menu?.contains(e.target);
  }

  /**
   * Display any sub-menu hanging off the current selection.
   */
  showSubMenu() {
    if (!this.isMenuShown()) {
      return;
    }
    this.menu!.showSubMenu();
  }

  /**
   * Do we have a menu visible to handle a keyboard event.
   * @return True if there's a visible menu.
   */
  private hasVisibleMenu_(): boolean {
    if (this.isMenuShown()) {
      return true;
    }
    return false;
  }

  /**
   * Handles event callbacks.
   */
  handleEvent(e: Event) {
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
        if (e.currentTarget === this.ownerDocument) {
          if (this.shouldDismissMenu_(e)) {
            this.hideMenuWithoutTakingFocus_();
          } else {
            e.preventDefault();
          }
        } else {
          if (this.isMenuShown()) {
            this.hideMenuWithoutTakingFocus_();
          } else if ((e as MouseEvent).button === 0) {
            // Only show the menu when using left mouse button.
            const mouseEvent = e as MouseEvent;
            this.showMenu(
                false, {x: mouseEvent.screenX, y: mouseEvent.screenY});
            // Prevent the button from stealing focus on mousedown unless
            // focus is on another button or cr-input element.
            if (!(document.hasFocus() &&
                  (document.activeElement?.tagName === 'BUTTON' ||
                   document.activeElement?.tagName === 'CR-BUTTON' ||
                   document.activeElement?.tagName === 'CR-INPUT'))) {
              e.preventDefault();
            }
          }
        }

        // Hide the focus ring on mouse click.
        this.classList.add('using-mouse');
        break;
      case 'keydown':
        this.handleKeyDown(e as KeyboardEvent);
        // If a menu is visible we let it handle keyboard events intended for
        // the menu.
        if (e.currentTarget === this.ownerDocument && this.hasVisibleMenu_() &&
            this.isMenuEvent(e as KeyboardEvent)) {
          this.menu.handleKeyDown(e as KeyboardEvent);
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
        if (this.menu.getSubMenuFromItem((e.target as MenuItem)) !== null) {
          break;
        }
        const activateEvent = e as MenuItemActivationEvent;
        if (activateEvent.originalEvent instanceof MouseEvent ||
            activateEvent.originalEvent instanceof TouchEvent) {
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

        if ((!this.menu || !this.menu.contains(e.target as HTMLElement))) {
          const mouseEvent = e as MouseEvent;
          this.showMenu(true, {x: mouseEvent.screenX, y: mouseEvent.screenY});
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
   * @param shouldSetFocus Whether to set focus on the
   *     selected menu item.
   * @param mousePos The position of the mouse
   *     when shown (in screen coordinates).
   */
  showMenu(shouldSetFocus: boolean, mousePos?: {x: number, y: number}) {
    this.hideMenu();

    assert(this.menu);

    this.menu.updateCommands(this);

    const event = new UIEvent(
        'menushow', {bubbles: true, cancelable: true, view: window});
    if (!this.dispatchEvent(event)) {
      return;
    }

    // Track element for which menu was opened so that command events are
    // dispatched to the correct element.
    this.menu.contextElement = this;
    this.menu.show(mousePos);

    // Toggle aria and open state.
    this.setAttribute('aria-expanded', 'true');
    this.setAttribute('menu-shown', '');

    // When the menu is shown we steal all keyboard events.
    const doc = this.ownerDocument;
    assert(doc.defaultView);
    const win = doc.defaultView;
    this.abortController_ = new AbortController();
    const signal = this.abortController_.signal;
    doc.addEventListener('keydown', this, {capture: true, signal});
    doc.addEventListener('mousedown', this, {capture: true, signal});
    doc.addEventListener('focus', this, {capture: true, signal});
    doc.addEventListener('scroll', this, {capture: true, signal});
    win.addEventListener('popstate', this, {signal});
    win.addEventListener('resize', this, {signal});
    this.menu.addEventListener('contextmenu', this, {signal});
    this.menu.addEventListener('activate', this, {signal});
    this.observedElement_ = this.parentElement;

    assert(this.observedElement_);
    assert(this.observer_);
    this.observer_.observe(this.observedElement_);
    this.positionMenu_();

    if (shouldSetFocus) {
      this.menu.focusSelectedItem();
    }
  }

  /**
   * Hides the menu. If your menu can go out of scope, make sure to call this
   * first.
   * @param hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  hideMenu(hideType?: HideType) {
    this.hideMenuInternal_(true, hideType);
  }

  /**
   * Hides the menu. If your menu can go out of scope, make sure to call this
   * first.
   * @param hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  private hideMenuWithoutTakingFocus_(hideType?: HideType) {
    this.hideMenuInternal_(false, hideType);
  }

  /**
   * Hides the menu. If your menu can go out of scope, make sure to call this
   * first.
   * @param shouldTakeFocus Moves the focus to the button if true.
   * @param hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  private hideMenuInternal_(shouldTakeFocus: boolean, hideType?: HideType) {
    if (!this.isMenuShown()) {
      return;
    }

    // Toggle aria and open state.
    this.setAttribute('aria-expanded', 'false');
    this.removeAttribute('menu-shown');
    assert(this.menu);
    assert(this.observer_);
    assert(this.observedElement_);

    if (hideType === HideType.DELAYED) {
      this.menu.classList.add('hide-delayed');
    } else {
      this.menu.classList.remove('hide-delayed');
    }
    this.menu.hide();

    this.abortController_?.abort();
    if (shouldTakeFocus) {
      this.focus();
    }

    this.observer_.unobserve(this.observedElement_);

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
   */
  private positionMenu_() {
    assert(this.menu);
    const style = this.menu.style;

    style.marginTop = '8px';  // crbug.com/1066727
    // Clear any maxHeight we've set from previous calls into here.
    style.maxHeight = 'none';
    const invertLeftRight = false;
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
  handleKeyDown(e: KeyboardEvent) {
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

  isMenuEvent(e: KeyboardEvent) {
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
