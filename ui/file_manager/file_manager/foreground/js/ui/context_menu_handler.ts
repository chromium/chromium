// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {type CustomEventMap, FilesEventTarget} from '../../../common/js/files_event_target.js';

import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {HideType} from './multi_menu_button.js';
import {positionPopupAtPoint} from './position_util.js';

interface ContextMenuEventDetail {
  element: HTMLElement;
  menu: Menu;
}

export type HideEvent = CustomEvent<ContextMenuEventDetail>;

export type ShowEvent = CustomEvent<ContextMenuEventDetail>;

interface ContextMenuHandlerEventMap extends CustomEventMap {
  'show': ShowEvent;
  'hide': HideEvent;
}

/**
 * Handles context menus.
 */
class ContextMenuHandler extends FilesEventTarget<ContextMenuHandlerEventMap> {
  private abortController_: AbortController|null = null;

  private menu_: Menu|null = null;
  private hideTimestamp_: number|null = null;
  private keyIsDown_ = false;
  private resizeObserver_: ResizeObserver|null = null;

  get menu() {
    return this.menu_;
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

  private getMenuPosition_(
      target: HTMLElement, clientX: number,
      clientY: number): {x: number, y: number} {
    // When the user presses the context menu key (on the keyboard) we need
    // to detect this.
    let x;
    let y;
    if (this.keyIsDown_) {
      let rect: DOMRect;
      if ('getRectForContextMenu' in target) {
        rect = (target.getRectForContextMenu as (() => DOMRect))();
      } else {
        rect = target.getBoundingClientRect();
      }
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
   * @param e The event triggering the show (usually a contextmenu event).
   * @param menu The menu to show.
   */
  showMenu(e: MouseEvent, menu: Menu) {
    assertInstanceof(e.currentTarget, Node);
    menu.updateCommands(e.currentTarget);
    if (!menu.hasVisibleItems()) {
      return;
    }

    const htmlElement = e.currentTarget as HTMLElement;
    const {x, y} = this.getMenuPosition_(htmlElement, e.clientX, e.clientY);
    this.menu_ = menu;
    menu.classList.remove('hide-delayed');
    menu.show({x, y});
    menu.contextElement = htmlElement;

    // When the menu is shown we steal a lot of events.
    const doc = menu.ownerDocument;
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    this.resizeObserver_ = new ResizeObserver((_entries) => {
      positionPopupAtPoint(x, y, menu);
    });
    this.resizeObserver_.observe(menu);
    this.abortController_ = new AbortController();
    const signal = this.abortController_.signal;
    doc.addEventListener(
        'keydown', this.handleKeyboardEvent_.bind(this),
        {signal, capture: true});
    doc.addEventListener(
        'mousedown', this.handleMouseEvent_.bind(this),
        {signal, capture: true});
    doc.addEventListener(
        'touchstart', this.handleTouchEvent_.bind(this),
        {signal, capture: true});
    doc.addEventListener('focus', this.handleFocusEvent_.bind(this), {signal});
    doc.defaultView?.addEventListener(
        'popstate', this.handleHideMenuEvent_.bind(this), {signal});
    doc.defaultView?.addEventListener(
        'resize', this.handleHideMenuEvent_.bind(this), {signal});
    doc.defaultView?.addEventListener(
        'blur', this.handleHideMenuEvent_.bind(this), {signal});
    menu.addEventListener(
        'contextmenu', this.handleContextMenuEvent_.bind(this), {signal});
    menu.addEventListener(
        'activate', this.handleActivateEvent_.bind(this), {signal});

    const ev =
        new CustomEvent('show', {detail: {element: menu.contextElement, menu}});
    this.dispatchEvent(ev);
  }

  /**
   * Hide the currently shown menu.
   * @param hideType Type of hide.
   *     default: HideType.INSTANT.
   */
  hideMenu(hideType?: HideType) {
    const menu = this.menu;
    if (!menu) {
      return;
    }

    if (hideType === HideType.DELAYED) {
      menu.classList.add('hide-delayed');
    } else {
      menu.classList.remove('hide-delayed');
    }
    menu.hide();
    const originalContextElement = menu.contextElement;
    menu.contextElement = null;
    this.abortController_?.abort();
    if (this.resizeObserver_) {
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

  private handleKeyboardEvent_(e: KeyboardEvent) {
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
      // intended for the menu.
    } else if (this.menu && this.isMenuEvent(e)) {
      this.menu.handleKeyDown(e);
      e.preventDefault();
      e.stopPropagation();
    }
  }

  private handleTouchEvent_(e: TouchEvent) {
    if (!this.menu?.contains(e.target as Node)) {
      this.hideMenu();
    }
  }

  private handleFocusEvent_(e: FocusEvent) {
    if (!this.menu?.contains(e.target as Node)) {
      this.hideMenu();
    }
  }

  private handleHideMenuEvent_(_e: Event) {
    if (this.menu) {
      this.hideMenu();
    }
  }

  private handleActivateEvent_(e: Event) {
    if (this.menu) {
      const hideDelayed = e.target instanceof MenuItem && e.target.checkable;
      this.hideMenu(hideDelayed ? HideType.DELAYED : HideType.INSTANT);
    }
  }

  private handleContextMenuEvent_(e: MouseEvent) {
    if ((!this.menu || !this.menu.contains(e.target as Node)) &&
        (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50)) {
      // Focus the item which triggers the context menu before showing the menu,
      // so when the menu hides, the focus can be brought back to the item.
      if (document.activeElement !== e.currentTarget) {
        (e.currentTarget as HTMLElement).focus();
      }
      this.showMenu(
          e, (e.currentTarget as Element & WithContextMenu).contextMenu!);
    }
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Handles mouse event callbacks.
   */
  private handleMouseEvent_(e: MouseEvent) {
    if (!this.menu) {
      return;
    }

    if (!this.menu.contains(e.target as Node)) {
      this.hideMenu();
    } else {
      e.preventDefault();
    }
  }

  /**
   * Adds a contextMenu property to an element or element class.
   * @param elementOrClass The element or class to add the contextMenu property
   *     to.
   */
  addContextMenuProperty(elementOrClass: Element|Function) {
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
          const signal = this.abortController_.signal;
          this.addEventListener(
              'contextmenu',
              contextMenuHandler.handleContextMenuEvent_.bind(
                  contextMenuHandler),
              {signal});
          this.addEventListener(
              'keydown',
              contextMenuHandler.handleKeyboardEvent_.bind(contextMenuHandler),
              {signal});
          this.addEventListener(
              'keyup',
              contextMenuHandler.handleKeyboardEvent_.bind(contextMenuHandler),
              {signal});
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
       * @return The rect to use for positioning the context menu when the
       *     context menu is not opened using a mouse position.
       */
      target.getRectForContextMenu = function(): ClientRect {
        return this.getBoundingClientRect();
      };
    }
  }

  /**
   * Sets the given contextMenu to the given element. A contextMenu property
   * would be added if necessary.
   * @param element The element or class to set the contextMenu to.
   * @param contextMenu The contextMenu property to be set.
   */
  setContextMenu(element: Element&WithContextMenu, contextMenu: Menu) {
    if (!element || !element.contextMenu) {
      this.addContextMenuProperty(element);
    }
    element.contextMenu = contextMenu;
  }
}

/**
 * Use this interface to define an element that also might have a context menu
 * attached, e.g. HTMLInputElement & WithContextMenu.
 */
export interface WithContextMenu {
  contextMenu?: Menu;
}

/**
 * The singleton context menu handler.
 */
export const contextMenuHandler = new ContextMenuHandler();
