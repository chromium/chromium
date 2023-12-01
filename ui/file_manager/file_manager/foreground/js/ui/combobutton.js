// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decorate} from '../../../common/js/cr_ui.js';

import {FilesMenuItem} from './files_menu.js';
import {MenuItem} from './menu_item.js';
import {MultiMenuButton} from './multi_menu_button.js';


/**
 * @fileoverview This implements a combobutton control.
 */
/**
 * Creates a new combo button element.
 */
// @ts-ignore: error TS2415: Class 'ComboButton' incorrectly extends base class
// 'MultiMenuButton'.
export class ComboButton extends MultiMenuButton {
  constructor() {
    super();

    // TODO(lucmult): Add type definition see `DropdownItem` in
    // task_controller.ts.
    /** @private @type {?*} */
    this.defaultItem_ = null;

    /** @private @type {?Element} */
    this.trigger_ = null;

    /** @private @type {?Element} */
    this.actionNode_ = null;

    /** @private @type {boolean} */
    this.disabled = false;

    /** @private @type {boolean} */
    this.multiple = false;
  }

  /**
   * Truncates drop-down list.
   */
  clear() {
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'ComboButton'.
    this.menu.clear();
    this.multiple = false;
  }

  // @ts-ignore: error TS7006: Parameter 'item' implicitly has an 'any' type.
  addDropDownItem(item) {
    this.multiple = true;
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'ComboButton'.
    const menuitem = /** @type {!MenuItem} */ (this.menu.addMenuItem(item));

    // If menu is files-menu, decorate menu item as FilesMenuItem.
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'ComboButton'.
    if (this.menu.classList.contains('files-menu')) {
      decorate(menuitem, FilesMenuItem);
      /** @type {!FilesMenuItem} */ (menuitem).toggleManagedIcon(
          /*visible=*/ item.isPolicyDefault);
      if (item.isDefault) {
        /** @type {!FilesMenuItem} */ (menuitem).setIsDefaultAttribute();
      }
    }

    // @ts-ignore: error TS2339: Property 'data' does not exist on type
    // 'MenuItem'.
    menuitem.data = item;
    // Move backgroundImage from the menu item container to the child icon.
    // @ts-ignore: error TS2339: Property 'iconStartImage' does not exist on
    // type 'MenuItem'.
    menuitem.iconStartImage = menuitem.style.backgroundImage;
    menuitem.style.backgroundImage = '';

    if (item.iconType) {
      // @ts-ignore: error TS2339: Property 'iconStartImage' does not exist on
      // type 'MenuItem'.
      menuitem.iconStartImage = '';
      // @ts-ignore: error TS2339: Property 'iconStartFileType' does not exist
      // on type 'MenuItem'.
      menuitem.iconStartFileType = item.iconType;
    }
    menuitem.toggleAttribute('disabled', !!item.isDlpBlocked);
    return menuitem;
  }

  /**
   * Adds separator to drop-down list.
   */
  addSeparator() {
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'ComboButton'.
    this.menu.addSeparator();
  }

  /**
   * Default item to fire on combobox click
   */
  get defaultItem() {
    return this.defaultItem_;
  }
  // @ts-ignore: error TS7006: Parameter 'defaultItem' implicitly has an 'any'
  // type.
  setDefaultItem_(defaultItem) {
    this.defaultItem_ = defaultItem;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
   * @override
   */
  static decorate(el) {
    // Add the ComboButton methods to the element we're
    // decorating, leaving it's prototype chain intact.
    // Don't copy 'constructor' or property get/setters.
    Object.getOwnPropertyNames(ComboButton.prototype).forEach(name => {
      if (name !== 'constructor' && name !== 'multiple' &&
          name !== 'disabled') {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'ComboButton'.
        el[name] = ComboButton.prototype[name];
      }
    });
    Object.getOwnPropertyNames(MultiMenuButton.prototype).forEach(name => {
      if (name !== 'constructor' &&
          !Object.getOwnPropertyDescriptor(el, name)) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'MultiMenuButton'.
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
    // @ts-ignore: error TS2339: Property 'addBooleanProperty_' does not exist
    // on type 'Element'.
    el.addBooleanProperty_('multiple');
    // @ts-ignore: error TS2339: Property 'addBooleanProperty_' does not exist
    // on type 'Element'.
    el.addBooleanProperty_('disabled');
    el = /** @type {!ComboButton} */ (el);
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Element'.
    el.decorate();
    // @ts-ignore: error TS2740: Type 'Element' is missing the following
    // properties from type 'ComboButton': defaultItem_, trigger_, actionNode_,
    // disabled, and 151 more.
    return el;
  }

  /**
   * Initializes the element.
   * @override
   */
  decorate() {
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'MultiMenuButton'.
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

    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'ComboButton'.
    this.menu.addEventListener('activate', this.handleMenuActivate_.bind(this));

    // Remove mousedown event listener created by MultiMenuButton::decorate,
    // and move it down to trigger_.
    // @ts-ignore: error TS2769: No overload matches this call.
    this.removeEventListener('mousedown', this);
    // @ts-ignore: error TS2769: No overload matches this call.
    this.trigger_.addEventListener('mousedown', this);
  }

  /**
   * Handles the keydown event for the menu button.
   */
  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleKeyDown(e) {
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on
        // type 'ComboButton'.
        if (!this.isMenuShown()) {
          this.showMenu(false);
        }
        e.preventDefault();
        break;
      case 'Escape':  // Maybe this is remote desktop playing a prank?
        // @ts-ignore: error TS2339: Property 'hideMenu' does not exist on type
        // 'ComboButton'.
        this.hideMenu();
        break;
    }
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  handleTriggerClicked_(event) {
    event.stopPropagation();
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  handleMenuActivate_(event) {
    this.dispatchSelectEvent(event.target.data);
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  handleButtonClick_(event) {
    if (this.multiple) {
      // When there are multiple choices just show/hide menu.
      // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
      // 'ComboButton'.
      if (this.isMenuShown()) {
        // @ts-ignore: error TS2339: Property 'hideMenu' does not exist on type
        // 'ComboButton'.
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

  // @ts-ignore: error TS7006: Parameter 'item' implicitly has an 'any' type.
  dispatchSelectEvent(item) {
    const selectEvent = new Event('select');
    // @ts-ignore: error TS2339: Property 'item' does not exist on type 'Event'.
    selectEvent.item = item;
    this.dispatchEvent(selectEvent);
  }
}
