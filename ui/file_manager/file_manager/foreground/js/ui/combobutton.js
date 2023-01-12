// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {decorate} from '../../../common/js/ui.js';
import {MenuItem} from './menu_item.js';

import {util} from '../../../common/js/util.js';

import {FilesMenuItem} from './files_menu.js';
import {MultiMenuButton} from './multi_menu_button.js';

/**
 * @fileoverview This implements a combobutton control.
 */
  /**
   * Creates a new combo button element.
   */
export class ComboButton extends MultiMenuButton {
  constructor() {
    super();

    /** @private {?MenuItem} */
    this.defaultItem_ = null;

    /** @private {?Element} */
    this.trigger_ = null;

    /** @private {?Element} */
    this.actionNode_ = null;

    /** @private {boolean} */
    this.disabled = false;

    /** @private {boolean} */
    this.multiple = false;
  }

  /**
   * Truncates drop-down list.
   */
  clear() {
    this.menu.clear();
    this.multiple = false;
  }

  addDropDownItem(item) {
    this.multiple = true;
    const menuitem = /** @type {!MenuItem} */ (this.menu.addMenuItem(item));

    // If menu is files-menu, decorate menu item as FilesMenuItem.
    if (this.menu.classList.contains('files-menu')) {
      decorate(menuitem, FilesMenuItem);
      /** @type {!FilesMenuItem} */ (menuitem).toggleManagedIcon(
          /*visible=*/ item.isPolicyDefault);
      if (item.isDefault) {
        /** @type {!FilesMenuItem} */ (menuitem).setIsDefaultAttribute();
      }
    }

    menuitem.data = item;
    // Move backgroundImage from the menu item container to the child icon.
    menuitem.iconStartImage = menuitem.style.backgroundImage;
    menuitem.style.backgroundImage = '';

    if (item.iconType) {
      menuitem.iconStartImage = '';
      menuitem.iconStartFileType = item.iconType;
    }
    if (item.bold) {
      menuitem.style.fontWeight = 'bold';
    }
    menuitem.toggleAttribute('disabled', !!item.isDlpBlocked);
    return menuitem;
  }

  /**
   * Adds separator to drop-down list.
   */
  addSeparator() {
    this.menu.addSeparator();
  }

  /**
   * Default item to fire on combobox click
   */
  get defaultItem() {
    return this.defaultItem_;
  }
  setDefaultItem_(defaultItem) {
    this.defaultItem_ = defaultItem;
    this.actionNode_.textContent = defaultItem.label || '';
  }
  set defaultItem(defaultItem) {
    this.setDefaultItem_(defaultItem);
  }

  /**
   * Utility function to set a boolean property get/setter.
   * @param {string} property Name of the property.
   */
  addBooleanProperty_(property) {
    Object.defineProperty(this, property, {
      get() {
        return this.getAttribute(property);
      },
      set(value) {
        if (value) {
          this.setAttribute(property, property);
        } else {
          this.removeAttribute(property);
        }
      },
      enumerable: true,
      configurable: true,
    });
  }

  /**
   * decorate expects a static |decorate| method.
   *
   * @param {!Element} el Element to be decorated.
   * @return {!ComboButton} Decorated element.
   * @public
   */
  static decorate(el) {
    // Add the ComboButton methods to the element we're
    // decorating, leaving it's prototype chain intact.
    // Don't copy 'constructor' or property get/setters.
    Object.getOwnPropertyNames(ComboButton.prototype).forEach(name => {
      if (name !== 'constructor' && name !== 'multiple' &&
          name !== 'disabled') {
        el[name] = ComboButton.prototype[name];
      }
    });
    Object.getOwnPropertyNames(MultiMenuButton.prototype).forEach(name => {
      if (name !== 'constructor' &&
          !Object.getOwnPropertyDescriptor(el, name)) {
        el[name] = MultiMenuButton.prototype[name];
      }
    });
    // Set up the 'menu, defaultItem, multiple and disabled'
    // properties & setter/getters.
    Object.defineProperty(el, 'menu', {
      get() {
        return this.menu_;
      },
      set(menu) {
        this.setMenu_(menu);
      },
      enumerable: true,
      configurable: true,
    });
    Object.defineProperty(el, 'defaultItem', {
      get() {
        return this.defaultItem_;
      },
      set(defaultItem) {
        this.setDefaultItem_(defaultItem);
      },
      enumerable: true,
      configurable: true,
    });
    el.addBooleanProperty_('multiple');
    el.addBooleanProperty_('disabled');
    el = /** @type {!ComboButton} */ (el);
    el.decorate();
    return el;
  }

  /**
   * Initializes the element.
   */
  decorate() {
    MultiMenuButton.prototype.decorate.call(this);

    this.classList.add('combobutton');

    const buttonLayer = this.ownerDocument.createElement('div');
    buttonLayer.classList.add('button');
    this.appendChild(buttonLayer);

    this.actionNode_ = this.ownerDocument.createElement('div');
    this.actionNode_.classList.add('action');
    buttonLayer.appendChild(this.actionNode_);

    const triggerIcon = this.ownerDocument.createElement('iron-icon');
    triggerIcon.setAttribute('icon', 'files:arrow-drop-down');
    this.trigger_ = this.ownerDocument.createElement('div');
    this.trigger_.classList.add('trigger');
    this.trigger_.appendChild(triggerIcon);

    buttonLayer.appendChild(this.trigger_);

    this.addEventListener('click', this.handleButtonClick_.bind(this));

    this.trigger_.addEventListener(
        'click', this.handleTriggerClicked_.bind(this));

    this.menu.addEventListener('activate', this.handleMenuActivate_.bind(this));

    // Remove mousedown event listener created by MultiMenuButton::decorate,
    // and move it down to trigger_.
    this.removeEventListener('mousedown', this);
    this.trigger_.addEventListener('mousedown', this);
  }

  /**
   * Handles the keydown event for the menu button.
   */
  handleKeyDown(e) {
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        if (!this.isMenuShown()) {
          this.showMenu(false);
        }
        e.preventDefault();
        break;
      case 'Escape':  // Maybe this is remote desktop playing a prank?
        this.hideMenu();
        break;
    }
  }

  handleTriggerClicked_(event) {
    event.stopPropagation();
  }

  handleMenuActivate_(event) {
    this.dispatchSelectEvent(event.target.data);
  }

  handleButtonClick_(event) {
    if (this.multiple) {
      // When there are multiple choices just show/hide menu.
      if (this.isMenuShown()) {
        this.hideMenu();
      } else {
        this.showMenu(true);
      }
    } else {
      // When there is only 1 choice, just dispatch to open.
      this.blur();
      this.dispatchSelectEvent(this.defaultItem_);
    }
  }

  dispatchSelectEvent(item) {
    const selectEvent = new Event('select');
    selectEvent.item = item;
    this.dispatchEvent(selectEvent);
  }
}
