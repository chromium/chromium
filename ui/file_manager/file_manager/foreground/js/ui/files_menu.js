// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';

/**
 * Menu item with ripple animation.
 */
export class FilesMenuItem extends MenuItem {
  constructor() {
    super();

    /** @private {boolean} */
    this.animating_ = false;

    /** @private {(boolean|undefined)} */
    this.hidden_ = undefined;

    /** @private {?HTMLElement} */
    this.label_ = null;

    /** @private {?HTMLElement} */
    this.iconStart_ = null;

    /** @private {?HTMLElement} */
    this.iconManaged_ = null;

    /** @private {?HTMLElement} */
    this.iconEnd_ = null;

    /** @private {?HTMLElement} */
    this.ripple_ = null;

    /** @public @type {?chrome.fileManagerPrivate.FileTaskDescriptor} */
    this.descriptor = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * Decorates the element.
   * @param {!Element} element Element to be decorated.
   * @return {!FilesMenuItem} Decorated element.
   */
  static decorate(element) {
    element.__proto__ = FilesMenuItem.prototype;
    element = /** @type {!FilesMenuItem} */ (element);
    element.decorate();
    return element;
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
    if (event.originalEvent instanceof KeyboardEvent) {
      this.ripple_.simulatedRipple();
    }

    // Perform fade out animation.
    const menu = assertInstanceof(this.parentNode, Menu);
    // If activation was on a menu-item that hosts a sub-menu, don't animate
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
   */
  get hidden() {
    if (this.hidden_ !== undefined) {
      return this.hidden_;
    }

    return Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        .get.call(this);
  }

  /**
   * Overrides hidden property to block the change of hidden property while
   * menu is animating.
   * @param {boolean} value
   */
  set hidden(value) {
    if (this.animating_) {
      this.hidden_ = value;
      return;
    }

    Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        .set.call(this, value);
  }

  /**
   * @return {string}
   */
  get label() {
    return this.label_.textContent;
  }

  /**
   * @param {string} value
   */
  set label(value) {
    this.label_.textContent = value;
  }

  /**
   * @return {string}
   */
  get iconStartImage() {
    return this.iconStart_.style.backgroundImage;
  }

  /**
   * @param {string} value
   */
  set iconStartImage(value) {
    this.iconStart_.setAttribute('style', 'background-image: ' + value);
  }

  /**
   * @return {string}
   */
  get iconStartFileType() {
    return this.iconStart_.getAttribute('file-type-icon');
  }

  /**
   * @param {string} value
   */
  set iconStartFileType(value) {
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
    this.iconManaged_.toggleAttribute('hidden', !visible);
    this.toggleIsManagedAttribute(visible);
  }

  /**
   * @return {string}
   */
  get iconEndImage() {
    return this.iconEnd_.style.backgroundImage;
  }

  /**
   * @param {string} value
   */
  set iconEndImage(value) {
    this.iconEnd_.setAttribute('style', 'background-image: ' + value);
  }

  /**
   * @return {string}
   */
  get iconEndFileType() {
    return this.iconEnd_.getAttribute('file-type-icon');
  }

  /**
   * @param {string} value
   */
  set iconEndFileType(value) {
    this.iconEnd_.setAttribute('file-type-icon', value);
  }

  removeIconEndFileType() {
    this.iconEnd_.removeAttribute('file-type-icon');
  }

  /**
   *
   * @param {boolean} isHidden
   */
  setIconEndHidden(isHidden) {
    this.iconEnd_.toggleAttribute('hidden', isHidden);
  }
}
