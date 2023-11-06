// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {EventTracker} from 'chrome://resources/ash/common/event_tracker.js';

import {decorate} from '../../../common/js/ui.js';

import {Menu} from './menu.js';
import {HideType} from './menu_button.js';
import {MenuItem} from './menu_item.js';
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
    /** @private @type {!EventTracker} */
    this.showingEvents_ = new EventTracker();

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
    const win = /** @type {!Window} */ (doc.defaultView);
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
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'keydown', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'mousedown', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'touchstart', this, true);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(doc, 'focus', this);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(win, 'popstate', this);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(win, 'resize', this);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Function | EventListener'.
    this.showingEvents_.add(win, 'blur', this);
    // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(menu, 'contextmenu', this);
    // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
    // parameter of type 'EventTarget'.
    this.showingEvents_.add(menu, 'activate', this);

    const ev = new Event('show');
    // @ts-ignore: error TS2339: Property 'contextElement' does not exist on
    // type 'Menu'.
    ev.element = menu.contextElement;
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type 'Event'.
    ev.menu = menu;
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
    this.showingEvents_.removeAll();
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

    const ev = new Event('hide');
    // @ts-ignore: error TS2339: Property 'element' does not exist on type
    // 'Event'.
    ev.element = originalContextElement;
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type 'Event'.
    ev.menu = menu;
    this.dispatchEvent(ev);
  }

  /**
   * Handles event callbacks.
   * @param {!Event} e The event object.
   */
  handleEvent(e) {
    // Keep track of keydown state so that we can use that to determine the
    // reason for the contextmenu event.
    switch (e.type) {
      case 'keydown':
        // @ts-ignore: error TS2339: Property 'altKey' does not exist on type
        // 'Event'.
        this.keyIsDown_ = !e.ctrlKey && !e.altKey &&
            // context menu key or Shift-F10
            // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on
            // type 'Event'.
            (e.keyCode === 93 && !e.shiftKey || e.key === 'F10' && e.shiftKey);
        break;

      case 'keyup':
        this.keyIsDown_ = false;
        break;
    }

    // Context menu is handled even when we have no menu.
    if (e.type !== 'contextmenu' && !this.menu) {
      return;
    }

    switch (e.type) {
      case 'mousedown':
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        if (!this.menu.contains(e.target)) {
          this.hideMenu();
        } else {
          e.preventDefault();
        }
        break;

      case 'touchstart':
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        if (!this.menu.contains(e.target)) {
          this.hideMenu();
        }
        break;

      case 'keydown':
        // @ts-ignore: error TS2339: Property 'key' does not exist on type
        // 'Event'.
        if (e.key === 'Escape') {
          this.hideMenu();
          e.stopPropagation();
          e.preventDefault();

          // If the menu is visible we let it handle all the keyboard events
          // unless Ctrl is held down.
          // @ts-ignore: error TS2339: Property 'ctrlKey' does not exist on type
          // 'Event'.
        } else if (this.menu && !e.ctrlKey) {
          // @ts-ignore: error TS2339: Property 'handleKeyDown' does not exist
          // on type 'Menu'.
          this.menu.handleKeyDown(e);
          e.preventDefault();
          e.stopPropagation();
        }
        break;

      case 'activate':
        const hideDelayed = e.target instanceof MenuItem && e.target.checkable;
        this.hideMenu(hideDelayed ? HideType.DELAYED : HideType.INSTANT);
        break;

      case 'focus':
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // 'Menu'.
        if (!this.menu.contains(e.target)) {
          this.hideMenu();
        }
        break;

      case 'blur':
        this.hideMenu();
        break;

      case 'popstate':
      case 'resize':
        this.hideMenu();
        break;

      case 'contextmenu':
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
        break;
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
          decorate(menu, Menu);
        }

        if (menu === oldContextMenu) {
          return;
        }

        if (oldContextMenu && !menu) {
          this.removeEventListener('contextmenu', contextMenuHandler);
          this.removeEventListener('keydown', contextMenuHandler);
          this.removeEventListener('keyup', contextMenuHandler);
        }
        if (menu && !oldContextMenu) {
          this.addEventListener('contextmenu', contextMenuHandler);
          this.addEventListener('keydown', contextMenuHandler);
          this.addEventListener('keyup', contextMenuHandler);
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
 * The singleton context menu handler.
 * @type {!ContextMenuHandler}
 */
export const contextMenuHandler = new ContextMenuHandler();
