// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {decorate} from '../../../common/js/cr_ui.js';

import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';

/**
 * Menu item with ripple animation.
 */
export class FilesMenuItem extends MenuItem {
  constructor() {
    super();

    /** @private @type {boolean} */
    this.animating_ = false;

    /** @private @type {(boolean|undefined)} */
    this.hidden_ = undefined;

    /** @private @type {?HTMLElement} */
    this.label_ = null;

    /** @private @type {?HTMLElement} */
    this.iconStart_ = null;

    /** @private @type {?HTMLElement} */
    this.iconManaged_ = null;

    /** @private @type {?HTMLElement} */
    this.iconEnd_ = null;

    /** @private @type {?HTMLElement} */
    this.ripple_ = null;

    /** @public @type {?chrome.fileManagerPrivate.FileTaskDescriptor} */
    this.descriptor = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * Decorates the element.
   * @param {!HTMLElement} element Element to be decorated.
   * @return {!FilesMenuItem} Decorated element.
   * @override
   */
  static decorate(element) {
    decorate(element, FilesMenuItem);
    return /** @type {!FilesMenuItem} */ (element);
  }

  /**
   * @override
   */
  decorate() {
    this.animating_ = false;

    // Custom menu item can have sophisticated content (elements).
    if (!this.children.length) {
      this.label_ =
          assertInstanceof(document.createElement('span'), HTMLElement);
      this.label_.textContent = this.textContent;

      this.iconStart_ =
          assertInstanceof(document.createElement('div'), HTMLElement);
      this.iconStart_.classList.add('icon', 'start');

      this.iconManaged_ =
          assertInstanceof(document.createElement('div'), HTMLElement);
      this.iconManaged_.classList.add('icon', 'managed');

      this.iconEnd_ =
          assertInstanceof(document.createElement('div'), HTMLElement);
      this.iconEnd_.classList.add('icon', 'end');
      /**
       * This is hidden by default because most of the menu items require
       * neither the end icon nor the managed icon, so the component that
       * plans to use either end icon should explicitly make it visible.
       */
      this.setIconEndHidden(true);
      this.toggleManagedIcon(/*visible=*/ false);

      // Override with standard menu item elements.
      this.textContent = '';
      this.appendChild(this.iconStart_);
      this.appendChild(this.label_);
      this.appendChild(this.iconManaged_);
      this.appendChild(this.iconEnd_);
    }

    this.ripple_ =
        assertInstanceof(document.createElement('paper-ripple'), HTMLElement);
    this.appendChild(this.ripple_);

    this.addEventListener('activate', this.onActivated_.bind(this));
  }

  /**
   * Handles activate event.
   * @param {Event} event
   * @private
   */
  onActivated_(event) {
    // Perform ripple animation if it's activated by keyboard.
    // @ts-ignore: error TS2339: Property 'originalEvent' does not exist on type
    // 'Event'.
    if (event.originalEvent instanceof KeyboardEvent) {
      // @ts-ignore: error TS2339: Property 'simulatedRipple' does not exist on
      // type 'HTMLElement'.
      this.ripple_.simulatedRipple();
    }

    // Perform fade out animation.
    // @ts-ignore: error TS2345: Argument of type '(arg0?: Object | undefined)
    // => Element' is not assignable to parameter of type 'new (...arg1: any[])
    // => any'.
    const menu = assertInstanceof(this.parentNode, Menu);
    // If activation was on a menu-item that hosts a sub-menu, don't animate
    // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on type
    // 'EventTarget'.
    const subMenuId = event.target.getAttribute('sub-menu');
    if (subMenuId !== null) {
      if (document.querySelector(subMenuId) !== null) {
        return;
      }
    }
    this.setMenuAsAnimating_(menu, true /* animating */);

    const player = menu.animate(
        [
          {
            opacity: 1,
            offset: 0,
          },
          {
            opacity: 0,
            offset: 1,
          },
        ],
        300);

    player.addEventListener(
        'finish',
        this.setMenuAsAnimating_.bind(this, menu, false /* not animating */));
  }

  /**
   * Sets menu as animating.
   * @param {!Menu} menu
   * @param {boolean} value True to set it as animating.
   * @private
   */
  setMenuAsAnimating_(menu, value) {
    menu.classList.toggle('animating', value);

    for (let i = 0; i < menu.menuItems.length; i++) {
      const menuItem = menu.menuItems[i];
      if (menuItem instanceof FilesMenuItem) {
        menuItem.setAnimating_(value);
      }
    }

    if (!value) {
      menu.classList.remove('toolbar-menu');
    }
  }

  /**
   * Sets the menu item as animating.
   * @param {boolean} value True to set this as animating.
   * @private
   */
  setAnimating_(value) {
    this.animating_ = value;

    if (this.animating_) {
      return;
    }

    // Update hidden property if there is a pending change.
    if (this.hidden_ !== undefined) {
      this.hidden = this.hidden_;
      this.hidden_ = undefined;
    }
  }

  /**
   * @return {boolean}
   * @override
   */
  get hidden() {
    if (this.hidden_ !== undefined) {
      return this.hidden_;
    }


    return Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        ?.get?.call(this);
  }

  /**
   * Overrides hidden property to block the change of hidden property while
   * menu is animating.
   * @param {boolean} value
   * @override
   */
  set hidden(value) {
    if (this.animating_) {
      this.hidden_ = value;
      return;
    }

    Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        ?.set?.call(this, value);
  }

  /**
   * @return {string}
   * @override
   */
  get label() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.label_.textContent;
  }

  /**
   * @param {string} value
   * @override
   */
  set label(value) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.label_.textContent = value;
  }

  /**
   * @return {string}
   */
  get iconStartImage() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.iconStart_.style.backgroundImage;
  }

  /**
   * @param {string} value
   */
  set iconStartImage(value) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconStart_.setAttribute('style', 'background-image: ' + value);
  }

  /**
   * @return {string}
   */
  get iconStartFileType() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.iconStart_.getAttribute('file-type-icon');
  }

  /**
   * @param {string} value
   */
  set iconStartFileType(value) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconStart_.setAttribute('file-type-icon', value);
  }

  /**
   * Sets or removes the `is-managed` attribute.
   * @param {boolean} isManaged
   */
  toggleIsManagedAttribute(isManaged) {
    this.toggleAttribute('is-managed', isManaged);
  }

  /**
   * Sets the `is-default` attribute.
   */
  setIsDefaultAttribute() {
    this.toggleAttribute('is-default', true);
  }

  /**
   * Toggles visibility of the `Managed by Policy` icon.
   * @param {boolean} visible
   */
  toggleManagedIcon(visible) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconManaged_.toggleAttribute('hidden', !visible);
    this.toggleIsManagedAttribute(visible);
  }

  /**
   * @return {string}
   */
  get iconEndImage() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.iconEnd_.style.backgroundImage;
  }

  /**
   * @param {string} value
   */
  set iconEndImage(value) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconEnd_.setAttribute('style', 'background-image: ' + value);
  }

  /**
   * @return {string}
   */
  get iconEndFileType() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.iconEnd_.getAttribute('file-type-icon');
  }

  /**
   * @param {string} value
   */
  set iconEndFileType(value) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconEnd_.setAttribute('file-type-icon', value);
  }

  removeIconEndFileType() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconEnd_.removeAttribute('file-type-icon');
  }

  /**
   *
   * @param {boolean} isHidden
   */
  setIconEndHidden(isHidden) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.iconEnd_.toggleAttribute('hidden', isHidden);
  }
}
