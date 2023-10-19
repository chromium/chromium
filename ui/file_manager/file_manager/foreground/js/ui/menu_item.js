// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';

import {Command} from './command.js';
import {define as crUiDefine, decorate, swallowDoubleClick} from '../../../common/js/ui.js';
// clang-format on


  /**
   * Creates a new menu item element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   * @implements {EventListener}
   */
// @ts-ignore: error TS8022: JSDoc '@implements' is not attached to a class.
export const MenuItem = crUiDefine('cr-menu-item');

/**
 * Creates a new menu separator element.
 * @return {!MenuItem} The new separator element.
 */
// @ts-ignore: error TS2339: Property 'createSeparator' does not exist on type
// '(arg0?: Object | undefined) => Element'.
MenuItem.createSeparator = function() {
  // @ts-ignore: error TS2352: Conversion of type 'HTMLHRElement' to type
  // 'MenuItem' may be a mistake because neither type sufficiently overlaps with
  // the other. If this was intentional, convert the expression to 'unknown'
  // first.
  const el = /** @type {!MenuItem} */ (document.createElement('hr'));
  // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
  // '(arg0?: Object | undefined) => Element'.
  if (MenuItem.decorate) {
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // '(arg0?: Object | undefined) => Element'.
    MenuItem.decorate(el);
  }
  return el;
};

MenuItem.prototype = {
  __proto__: HTMLElement.prototype,

  /**
   * Initializes the menu item.
   */
  decorate() {
    let commandId;
    // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    if ((commandId = this.getAttribute('command'))) {
      this.command = commandId;
    }

    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
    // command: Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.addEventListener('mouseup', this.handleMouseUp_);

    // Adding the 'custom-appearance' class prevents widgets.css from changing
    // the appearance of this element.
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.classList.add('custom-appearance');

    // Enable Text to Speech on the menu. Additionally, ID has to be set,
    // since it is used in element's aria-activedescendant attribute.
    if (!this.isSeparator()) {
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
      // command: Command; label: string; iconUrl: string; isSeparator():
      // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      this.setAttribute('role', 'menuitem');
      // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
      // command: Command; label: string; iconUrl: string; isSeparator():
      // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      this.setAttribute('tabindex', this.getAttribute('tabindex') || -1);
    }

    let iconUrl;
    // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    if ((iconUrl = this.getAttribute('icon'))) {
      this.iconUrl = iconUrl;
    }
  },

  /**
   * The command associated with this menu item. If this is set to a string
   * of the form "#element-id" then the element is looked up in the document
   * of the command.
   * @type {Command}
   */
  // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'Command'.
  command_: null,
  get command() {
    return this.command_;
  },
  set command(command) {
    if (this.command_) {
      this.command_.removeEventListener('labelChange', this);
      this.command_.removeEventListener('disabledChange', this);
      this.command_.removeEventListener('hiddenChange', this);
      this.command_.removeEventListener('checkedChange', this);
    }

    if (typeof command === 'string' && command[0] === '#') {
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
      // command: Command; label: string; iconUrl: string; isSeparator():
      // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      command = assert(this.ownerDocument.body.querySelector(command));
      decorate(command, Command);
    }

    this.command_ = command;
    if (command) {
      if (command.id) {
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
        // command: Command; label: string; iconUrl: string; isSeparator():
        // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
        // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
        // Event): void; }'.
        this.setAttribute('command', '#' + command.id);
      }

      if (typeof command.label === 'string') {
        this.label = command.label;
      }
      // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
      // __proto__: HTMLElement; decorate(): void; command_: Command; command:
      // Command; label: string; iconUrl: string; isSeparator(): boolean;
      // updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      this.disabled = command.disabled;
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type '{
      // __proto__: HTMLElement; decorate(): void; command_: Command; command:
      // Command; label: string; iconUrl: string; isSeparator(): boolean;
      // updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      this.hidden = command.hidden;
      // @ts-ignore: error TS2339: Property 'checked' does not exist on type '{
      // __proto__: HTMLElement; decorate(): void; command_: Command; command:
      // Command; label: string; iconUrl: string; isSeparator(): boolean;
      // updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      this.checked = command.checked;

      this.command_.addEventListener('labelChange', this);
      this.command_.addEventListener('disabledChange', this);
      this.command_.addEventListener('hiddenChange', this);
      this.command_.addEventListener('checkedChange', this);
    }

    this.updateShortcut_();
  },

  /**
   * The text label.
   * @type {string}
   */
  get label() {
    // @ts-ignore: error TS2339: Property 'textContent' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    return this.textContent;
  },
  set label(label) {
    // @ts-ignore: error TS2339: Property 'textContent' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.textContent = label;
  },

  /**
   * Menu icon.
   * @type {string}
   */
  get iconUrl() {
    // @ts-ignore: error TS2339: Property 'style' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    return this.style.backgroundImage;
  },
  set iconUrl(url) {
    // @ts-ignore: error TS2339: Property 'style' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.style.backgroundImage = 'url(' + url + ')';
  },

  /**
   * @return {boolean} Whether the menu item is a separator.
   */
  isSeparator() {
    // @ts-ignore: error TS2339: Property 'tagName' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    return this.tagName === 'HR';
  },

  /**
   * Updates shortcut text according to associated command. If command has
   * multiple shortcuts, only first one is displayed.
   */
  updateShortcut_() {
    // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
    // command: Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.removeAttribute('shortcutText');

    // @ts-ignore: error TS2339: Property 'shortcut' does not exist on type
    // 'Command'.
    if (!this.command_ || !this.command_.shortcut ||
        this.command_.hideShortcutText) {
      return;
    }

    // @ts-ignore: error TS2339: Property 'shortcut' does not exist on type
    // 'Command'.
    const shortcuts = this.command_.shortcut.split(/\s+/);

    if (shortcuts.length === 0) {
      return;
    }

    const shortcut = shortcuts[0];
    const mods = {};
    let ident = '';
    // @ts-ignore: error TS7006: Parameter 'part' implicitly has an 'any' type.
    shortcut.split('|').forEach(function(part) {
      const partUc = part.toUpperCase();
      switch (partUc) {
        case 'CTRL':
        case 'ALT':
        case 'SHIFT':
        case 'META':
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'any' can't be used to index type '{}'.
          mods[partUc] = true;
          break;
        default:
          console.assert(!ident, 'Shortcut has two non-modifier keys');
          ident = part;
      }
    });

    let shortcutText = '';

    ['CTRL', 'ALT', 'SHIFT', 'META'].forEach(function(mod) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      if (mods[mod]) {
        shortcutText += loadTimeData.getString('SHORTCUT_' + mod) + '+';
      }
    });

    if (ident === ' ') {
      ident = 'Space';
    }

    if (ident.length !== 1) {
      shortcutText += loadTimeData.getString('SHORTCUT_' + ident.toUpperCase());
    } else {
      shortcutText += ident.toUpperCase();
    }

    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    this.setAttribute('shortcutText', shortcutText);
  },

  /**
   * Handles mouseup events. This dispatches an activate event; if there is an
   * associated command, that command is executed.
   * @param {!Event} e The mouseup event object.
   * @private
   */
  handleMouseUp_(e) {
    e = /** @type {!MouseEvent} */ (e);
    // Only dispatch an activate event for left or middle click.
    // @ts-ignore: error TS2339: Property 'button' does not exist on type
    // 'Event'.
    if (e.button > 1) {
      return;
    }

    // @ts-ignore: error TS2339: Property 'selected' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; command_: Command; command:
    // Command; label: string; iconUrl: string; isSeparator(): boolean;
    // updateShortcut_(): void; handleMouseUp_(e: Event): void;
    // updateCommand(opt_node?: Node | undefined): void; handleEvent(e: Event):
    // void; }'.
    if (!this.disabled && !this.isSeparator() && this.selected) {
      // Store |contextElement| since it'll be removed by {Menu} on handling
      // 'activate' event.
      const contextElement =
          // @ts-ignore: error TS2339: Property 'parentNode' does not exist on
          // type '{ __proto__: HTMLElement; decorate(): void; command_:
          // Command; command: Command; label: string; iconUrl: string;
          // isSeparator(): boolean; updateShortcut_(): void; handleMouseUp_(e:
          // Event): void; updateCommand(opt_node?: Node | undefined): void;
          // handleEvent(e: Event): void; }'.
          /** @type {{contextElement: Element}} */ (this.parentNode)
              .contextElement;
      const activationEvent = document.createEvent('Event');
      activationEvent.initEvent('activate', true, true);
      // @ts-ignore: error TS2339: Property 'originalEvent' does not exist on
      // type 'Event'.
      activationEvent.originalEvent = e;
      // Dispatch command event followed by executing the command object.
      // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; command_: Command;
      // command: Command; label: string; iconUrl: string; isSeparator():
      // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
      // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
      // Event): void; }'.
      if (this.dispatchEvent(activationEvent)) {
        const command = this.command;
        if (command) {
          // @ts-ignore: error TS2345: Argument of type 'Element' is not
          // assignable to parameter of type 'HTMLElement'.
          command.execute(contextElement);
          // @ts-ignore: error TS2345: Argument of type 'Event' is not
          // assignable to parameter of type 'MouseEvent'.
          swallowDoubleClick(e);
        }
      }
    }
  },

  /**
   * Updates command according to the node on which this menu was invoked.
   * @param {Node=} opt_node Node on which menu was opened.
   */
  updateCommand(opt_node) {
    if (this.command_) {
      this.command_.canExecuteChange(opt_node);
    }
  },

  /**
   * Handles changes to the associated command.
   * @param {Event} e The event object.
   */
  handleEvent(e) {
    switch (e.type) {
      case 'disabledChange':
        // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
        // '{ __proto__: HTMLElement; decorate(): void; command_: Command;
        // command: Command; label: string; iconUrl: string; isSeparator():
        // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
        // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
        // Event): void; }'.
        this.disabled = this.command.disabled;
        break;
      case 'hiddenChange':
        // @ts-ignore: error TS2339: Property 'hidden' does not exist on type '{
        // __proto__: HTMLElement; decorate(): void; command_: Command; command:
        // Command; label: string; iconUrl: string; isSeparator(): boolean;
        // updateShortcut_(): void; handleMouseUp_(e: Event): void;
        // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
        // Event): void; }'.
        this.hidden = this.command.hidden;
        break;
      case 'labelChange':
        this.label = this.command.label;
        break;
      case 'checkedChange':
        // @ts-ignore: error TS2339: Property 'checked' does not exist on type
        // '{ __proto__: HTMLElement; decorate(): void; command_: Command;
        // command: Command; label: string; iconUrl: string; isSeparator():
        // boolean; updateShortcut_(): void; handleMouseUp_(e: Event): void;
        // updateCommand(opt_node?: Node | undefined): void; handleEvent(e:
        // Event): void; }'.
        this.checked = this.command.checked;
        break;
    }
  },
};
/**
 * Whether the menu item is disabled or not.
 * @type {boolean}
 */
MenuItem.prototype.disabled;
Object.defineProperty(
    MenuItem.prototype, 'disabled',
    getPropertyDescriptor('disabled', PropertyKind.BOOL_ATTR));

/**
 * Whether the menu item is hidden or not.
 */
Object.defineProperty(
    MenuItem.prototype, 'hidden',
    getPropertyDescriptor('hidden', PropertyKind.BOOL_ATTR));

/**
 * Whether the menu item is selected or not.
 * @type {boolean}
 */
MenuItem.prototype.selected;
Object.defineProperty(
    MenuItem.prototype, 'selected',
    getPropertyDescriptor('selected', PropertyKind.BOOL_ATTR));

/**
 * Whether the menu item is checked or not.
 * @type {boolean}
 */
MenuItem.prototype.checked;
Object.defineProperty(
    MenuItem.prototype, 'checked',
    getPropertyDescriptor('checked', PropertyKind.BOOL_ATTR));

/**
 * Whether the menu item is checkable or not.
 * @type {boolean}
 */
MenuItem.prototype.checkable;
Object.defineProperty(
    MenuItem.prototype, 'checkable',
    getPropertyDescriptor('checkable', PropertyKind.BOOL_ATTR));
