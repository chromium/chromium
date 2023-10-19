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
  export const MenuItem = crUiDefine('cr-menu-item');

  /**
   * Creates a new menu separator element.
   * @return {!MenuItem} The new separator element.
   */
  MenuItem.createSeparator = function() {
    const el = /** @type {!MenuItem} */ (document.createElement('hr'));
    if (MenuItem.decorate) {
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
      if ((commandId = this.getAttribute('command'))) {
        this.command = commandId;
      }

      this.addEventListener('mouseup', this.handleMouseUp_);

      // Adding the 'custom-appearance' class prevents widgets.css from changing
      // the appearance of this element.
      this.classList.add('custom-appearance');

      // Enable Text to Speech on the menu. Additionally, ID has to be set,
      // since it is used in element's aria-activedescendant attribute.
      if (!this.isSeparator()) {
        this.setAttribute('role', 'menuitem');
        this.setAttribute('tabindex', this.getAttribute('tabindex') || -1);
      }

      let iconUrl;
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
        command = assert(this.ownerDocument.body.querySelector(command));
        decorate(command, Command);
      }

      this.command_ = command;
      if (command) {
        if (command.id) {
          this.setAttribute('command', '#' + command.id);
        }

        if (typeof command.label === 'string') {
          this.label = command.label;
        }
        this.disabled = command.disabled;
        this.hidden = command.hidden;
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
      return this.textContent;
    },
    set label(label) {
      this.textContent = label;
    },

    /**
     * Menu icon.
     * @type {string}
     */
    get iconUrl() {
      return this.style.backgroundImage;
    },
    set iconUrl(url) {
      this.style.backgroundImage = 'url(' + url + ')';
    },

    /**
     * @return {boolean} Whether the menu item is a separator.
     */
    isSeparator() {
      return this.tagName === 'HR';
    },

    /**
     * Updates shortcut text according to associated command. If command has
     * multiple shortcuts, only first one is displayed.
     */
    updateShortcut_() {
      this.removeAttribute('shortcutText');

      if (!this.command_ || !this.command_.shortcut ||
          this.command_.hideShortcutText) {
        return;
      }

      const shortcuts = this.command_.shortcut.split(/\s+/);

      if (shortcuts.length === 0) {
        return;
      }

      const shortcut = shortcuts[0];
      const mods = {};
      let ident = '';
      shortcut.split('|').forEach(function(part) {
        const partUc = part.toUpperCase();
        switch (partUc) {
          case 'CTRL':
          case 'ALT':
          case 'SHIFT':
          case 'META':
            mods[partUc] = true;
            break;
          default:
            console.assert(!ident, 'Shortcut has two non-modifier keys');
            ident = part;
        }
      });

      let shortcutText = '';

      ['CTRL', 'ALT', 'SHIFT', 'META'].forEach(function(mod) {
        if (mods[mod]) {
          shortcutText += loadTimeData.getString('SHORTCUT_' + mod) + '+';
        }
      });

      if (ident === ' ') {
        ident = 'Space';
      }

      if (ident.length !== 1) {
        shortcutText +=
            loadTimeData.getString('SHORTCUT_' + ident.toUpperCase());
      } else {
        shortcutText += ident.toUpperCase();
      }

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
      if (e.button > 1) {
        return;
      }

      if (!this.disabled && !this.isSeparator() && this.selected) {
        // Store |contextElement| since it'll be removed by {Menu} on handling
        // 'activate' event.
        const contextElement =
            /** @type {{contextElement: Element}} */ (this.parentNode)
                .contextElement;
        const activationEvent = document.createEvent('Event');
        activationEvent.initEvent('activate', true, true);
        activationEvent.originalEvent = e;
        // Dispatch command event followed by executing the command object.
        if (this.dispatchEvent(activationEvent)) {
          const command = this.command;
          if (command) {
            command.execute(contextElement);
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
          this.disabled = this.command.disabled;
          break;
        case 'hiddenChange':
          this.hidden = this.command.hidden;
          break;
        case 'labelChange':
          this.label = this.command.label;
          break;
        case 'checkedChange':
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
