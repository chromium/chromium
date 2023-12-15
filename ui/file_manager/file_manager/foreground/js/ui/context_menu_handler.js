// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {HideType} from './multi_menu_button.js';
import {positionPopupAtPoint} from './position_util.js';

/**
 * Handles context menus.
 * @implements {EventListener}
 */
// @ts-ignore: error TS2420: Class 'ContextMenuHandler' incorrectly implements
// interface 'EventListener'.
class ContextMenuHandler extends EventTarget {
  constructor() {
    super();
    /** @private @type {AbortController|null} */
    this.abortController_ = null;

    /**
     * The menu that we are currently showing.
     * @private @type {?Menu}
     */
    this.menu_ = null;

    /** @private @type {?number} */
    this.hideTimestamp_ = null;

    /** @private @type {boolean} */
    this.keyIsDown_ = false;

    /** @private @type {?ResizeObserver} */
    this.resizeObserver_ = null;
  }

  get menu() {
    return this.menu_;
  }

  /**
   * @param {!HTMLElement} target
   * @param {number} clientX
   * @param {number} clientY
   * @return {{x: number, y: number}}
   * @private
   */
  getMenuPosition_(target, clientX, clientY) {
    // When the user presses the context menu key (on the keyboard) we need
    // to detect this.
    let x;
    let y;
    if (this.keyIsDown_) {
      // @ts-ignore: error TS2339: Property 'getRectForContextMenu' does not
      // exist on type 'HTMLElement'.
      const rect = target.getRectForContextMenu ?
          // @ts-ignore: error TS2339: Property 'getRectForContextMenu' does not
          // exist on type 'HTMLElement'.
          target.getRectForContextMenu() :
          target.getBoundingClientRect();
      const offset = Math.min(rect.width, rect.height) / 2;
      x = rect.left + offset;
      y = rect.top + offset;
    } else {
      x = clientX;
      y = clientY;
    }

    return {x, y};
  }

  /**
   * Shows a menu as a context menu.
   * @param {!Event} e The event triggering the show (usually a contextmenu
   *     event).
   * @param {!Menu} menu The menu to show.
   */
  showMenu(e, menu) {
    // @ts-ignore: error TS2339: Property 'updateCommands' does not exist on
    // type 'Menu'.
    menu.updateCommands(assertInstanceof(e.currentTarget, Node));
    // @ts-ignore: error TS2339: Property 'hasVisibleItems' does not exist on
    // type 'Menu'.
    if (!menu.hasVisibleItems()) {
      return;
    }

    const {x, y} = this.getMenuPosition_(
        // @ts-ignore: error TS2339: Property 'clientY' does not exist on type
        // 'Event'.
        /** @type {!HTMLElement} */ (e.currentTarget), e.clientX, e.clientY);
    this.menu_ = menu;
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'Menu'.
    menu.classList.remove('hide-delayed');
    // @ts-ignore: error TS2339: Property 'show' does not exist on type 'Menu'.
    menu.show({x, y});
    // @ts-ignore: error TS2339: Property 'contextElement' does not exist on
    // type 'Menu'.
    menu.contextElement = e.currentTarget;

    // When the menu is shown we steal a lot of events.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'Menu'.
    const doc = menu.ownerDocument;
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    // @ts-ignore: error TS6133: 'entries' is declared but its value is never
    // read.
    this.resizeObserver_ = new ResizeObserver((entries) => {
      // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
      // parameter of type 'HTMLElement'.
      positionPopupAtPoint(x, y, menu);
    });
    // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
    // parameter of type 'Element'.
    this.resizeObserver_.observe(menu);
    this.abortController_ = new AbortController();
    doc.addEventListener(
        'keydown', this.handleKeyboardEvent_.bind(this),
        {signal: this.abortController_.signal, capture: true});
    doc.addEventListener(
        'mousedown', this.handleMouseEvent_.bind(this),
        {signal: this.abortController_.signal, capture: true});
    doc.addEventListener(
        'touchstart', this.handleTouchEvent_.bind(this),
        {signal: this.abortController_.signal, capture: true});
    doc.addEventListener(
        'focus', this.handleFocusEvent_.bind(this),
        {signal: this.abortController_.signal});
    doc.defaultView?.addEventListener(
        'popstate', this.handleHideMenuEvent_.bind(this),
        {signal: this.abortController_.signal});
    doc.defaultView?.addEventListener(
        'resize', this.handleHideMenuEvent_.bind(this),
        {signal: this.abortController_.signal});
    doc.defaultView?.addEventListener(
        'blur', this.handleHideMenuEvent_.bind(this),
        {signal: this.abortController_.signal});
    menu.addEventListener(
        'contextmenu', this.handleContextMenuEvent_.bind(this),
        {signal: this.abortController_.signal});
    menu.addEventListener(
        'activate', this.handleActivateEvent_.bind(this),
        {signal: this.abortController_.signal});

    const ev =
        new CustomEvent('show', {detail: {element: menu.contextElement, menu}});
    this.dispatchEvent(ev);
  }

  /**
   * Hide the currently shown menu.
   * @param {HideType=} opt_hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  hideMenu(opt_hideType) {
    const menu = this.menu;
    if (!menu) {
      return;
    }

    if (opt_hideType === HideType.DELAYED) {
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'Menu'.
      menu.classList.add('hide-delayed');
    } else {
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'Menu'.
      menu.classList.remove('hide-delayed');
    }
    // @ts-ignore: error TS2339: Property 'hide' does not exist on type 'Menu'.
    menu.hide();
    // @ts-ignore: error TS2339: Property 'contextElement' does not exist on
    // type 'Menu'.
    const originalContextElement = menu.contextElement;
    // @ts-ignore: error TS2339: Property 'contextElement' does not exist on
    // type 'Menu'.
    menu.contextElement = null;
    this.abortController_?.abort();
    if (this.resizeObserver_) {
      // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
      // parameter of type 'Element'.
      this.resizeObserver_.unobserve(menu);
      this.resizeObserver_ = null;
    }
    menu.selectedIndex = -1;
    this.menu_ = null;

    // On windows we might hide the menu in a right mouse button up and if
    // that is the case we wait some short period before we allow the menu
    // to be shown again.
    this.hideTimestamp_ = 0;

    const ev = new CustomEvent(
        'hide', {detail: {element: originalContextElement, menu}});
    this.dispatchEvent(ev);
  }

  /**
   * @param {KeyboardEvent} e
   * @private
   */
  handleKeyboardEvent_(e) {
    // Keep track of keydown state so that we can use that to determine the
    // reason for the contextmenu event.
    switch (e.type) {
      case 'keydown':
        this.keyIsDown_ = !e.ctrlKey && !e.altKey &&
            // context menu key or Shift-F10.
            (e.keyCode === 93 && !e.shiftKey || e.key === 'F10' && e.shiftKey);
        break;
      case 'keyup':
        this.keyIsDown_ = false;
        break;
    }

    if (!this.menu || e.type !== 'keydown') {
      return;
    }

    if (e.key === 'Escape') {
      this.hideMenu();
      e.stopPropagation();
      e.preventDefault();

      // If the menu is visible we let it handle all the keyboard events
      // unless Ctrl is held down.
    } else if (this.menu && !e.ctrlKey) {
      this.menu.handleKeyDown(e);
      e.preventDefault();
      e.stopPropagation();
    }
  }

  /**
   * @param {TouchEvent} e
   * @private
   */
  handleTouchEvent_(e) {
    if (!this.menu?.contains(/** @type {Node} */ (e.target))) {
      this.hideMenu();
    }
  }

  /**
   * @param {FocusEvent} e
   * @private
   */
  handleFocusEvent_(e) {
    if (!this.menu?.contains(/** @type {Node} */ (e.target))) {
      this.hideMenu();
    }
  }

  /**
   * @param {Event} _e
   * @private
   */
  handleHideMenuEvent_(_e) {
    if (this.menu) {
      this.hideMenu();
    }
  }

  /**
   * @param {Event} e
   */
  handleActivateEvent_(e) {
    if (this.menu) {
      const hideDelayed = e.target instanceof MenuItem && e.target.checkable;
      this.hideMenu(hideDelayed ? HideType.DELAYED : HideType.INSTANT);
    }
  }

  /**
   * @param {Event} e
   */
  handleContextMenuEvent_(e) {
    // @ts-ignore: error TS2339: Property 'contains' does not exist on type
    // 'Menu'.
    if ((!this.menu || !this.menu.contains(e.target)) &&
        (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50)) {
      // @ts-ignore: error TS2339: Property 'contextMenu' does not exist on
      // type 'EventTarget'.
      this.showMenu(e, e.currentTarget.contextMenu);
    }
    e.preventDefault();
    // Don't allow elements further up in the DOM to show their menus.
    e.stopPropagation();
  }

  /**
   * Handles event callbacks.
   * @param {MouseEvent} e The event object.
   * @private
   */
  handleMouseEvent_(e) {
    if (!this.menu) {
      return;
    }

    // @ts-ignore: error TS2339: Property 'contains' does not exist on type
    // 'Menu'.
    if (!this.menu.contains(e.target)) {
      this.hideMenu();
    } else {
      e.preventDefault();
    }
  }

  /**
   * Adds a contextMenu property to an element or element class.
   * @param {!Element|!Function} elementOrClass The element or class to add
   *     the contextMenu property to.
   */
  addContextMenuProperty(elementOrClass) {
    const target = typeof elementOrClass === 'function' ?
        elementOrClass.prototype :
        elementOrClass;

    Object.defineProperty(target, 'contextMenu', {
      get: function() {
        return this.contextMenu_;
      },
      set: function(menu) {
        const oldContextMenu = this.contextMenu;

        if (typeof menu === 'string' && menu[0] === '#') {
          menu = this.ownerDocument.getElementById(menu.slice(1));
          crInjectTypeAndInit(menu, Menu);
        }

        if (menu === oldContextMenu) {
          return;
        }

        if (oldContextMenu && !menu) {
          this.abortController_?.abort();
        }
        if (menu && !oldContextMenu) {
          this.abortController_ = new AbortController();
          this.addEventListener(
              'contextmenu',
              contextMenuHandler.handleContextMenuEvent_.bind(
                  contextMenuHandler),
              {signal: this.abortController_.signal});
          this.addEventListener(
              'keydown',
              contextMenuHandler.handleKeyboardEvent_.bind(contextMenuHandler),
              {signal: this.abortController_.signal});
          this.addEventListener(
              'keyup',
              contextMenuHandler.handleKeyboardEvent_.bind(contextMenuHandler),
              {signal: this.abortController_.signal});
        }

        this.contextMenu_ = menu;

        if (menu && menu.id) {
          this.setAttribute('contextmenu', '#' + menu.id);
        }

        dispatchPropertyChange(this, 'contextMenu', menu, oldContextMenu);
      },
    });

    if (!target.getRectForContextMenu) {
      /**
       * @return {!ClientRect} The rect to use for positioning the context
       *     menu when the context menu is not opened using a mouse position.
       */
      target.getRectForContextMenu = function() {
        return this.getBoundingClientRect();
      };
    }
  }

  /**
   * Sets the given contextMenu to the given element. A contextMenu property
   * would be added if necessary.
   * @param {!Element} element The element or class to set the contextMenu to.
   * @param {!Menu} contextMenu The contextMenu property to be set.
   */
  setContextMenu(element, contextMenu) {
    // @ts-ignore: error TS2339: Property 'contextMenu' does not exist on type
    // 'Element'.
    if (!element.contextMenu) {
      this.addContextMenuProperty(element);
    }
    // @ts-ignore: error TS2339: Property 'contextMenu' does not exist on type
    // 'Element'.
    element.contextMenu = contextMenu;
  }
}

/**
 * Use this interface to define an element that also might have a context menu
 * attached, e.g. HTMLInputElement & WithContextMenu.
 * @interface
 */
export class WithContextMenu {
  constructor() {
    /** @type {Menu|undefined} */
    this.contextMenu = undefined;
  }
}

/**
 * The singleton context menu handler.
 * @type {!ContextMenuHandler}
 */
export const contextMenuHandler = new ContextMenuHandler();
