// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', () => {
  /**
   * Menu item with ripple animation.
   */
  class FilesMenuItem extends cr.ui.MenuItem {
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
      this.ripple_ = null;

      throw new Error('Designed to decorate elements');
    }

    /**
     * Decorates the element.
     * @param {!Element} element Element to be decorated.
     * @return {!cr.ui.FilesMenuItem} Decorated element.
     */
    static decorate(element) {
      element.__proto__ = FilesMenuItem.prototype;
      element = /** @type {!cr.ui.FilesMenuItem} */ (element);
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

        // Override with standard menu item elements.
        this.textContent = '';
        this.appendChild(this.iconStart_);
        this.appendChild(this.label_);
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
      const menu = assertInstanceof(this.parentNode, cr.ui.Menu);
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
     * @param {!cr.ui.Menu} menu
     * @param {boolean} value True to set it as animating.
     * @private
     */
    setMenuAsAnimating_(menu, value) {
      menu.classList.toggle('animating', value);

      for (let i = 0; i < menu.menuItems.length; i++) {
        const menuItem = menu.menuItems[i];
        if (menuItem instanceof cr.ui.FilesMenuItem) {
          menuItem.setAnimating_(value);
        }
      }

      if (!value) {
        menu.classList.remove('toolbar-menu');
      }
    }

    /**
     * Sets thie menu item as animating.
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
  }

  return {
    FilesMenuItem: FilesMenuItem,
  };
});
