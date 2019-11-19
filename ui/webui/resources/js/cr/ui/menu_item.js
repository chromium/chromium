// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /** @const */ const Command = cr.ui.Command;

  /**
   * Creates a new menu item element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   * @implements {EventListener}
   */
  const MenuItem = cr.ui.define('cr-menu-item');

  /**
   * Creates a new menu separator element.
   * @return {!cr.ui.MenuItem} The new separator element.
   */
  MenuItem.createSeparator = function() {
    const el = /** @type {!cr.ui.MenuItem} */ (document.createElement('hr'));
    MenuItem.decorate(el);
    return el;
  };

  MenuItem.prototype = {
    __proto__: HTMLElement.prototype,

    /**
     * Initializes the menu item.
     */
    decorate: function() {
      let commandId;
      if ((commandId = this.getAttribute('command'))) {
        this.command = commandId;
      }

      this.addEventListener('mouseup', this.handleMouseUp_);

      // Adding the 'custom-appearance' class prevents widgets.css from changing
      // the appearance of this element.
      this.classList.add('custom-appearance');

      // Enable Text to Speech on the menu. Additionaly, ID has to be set, since
      // it is used in element's aria-activedescendant attribute.
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
     * @type {cr.ui.Command}
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

      if (typeof command == 'string' && command[0] == '#') {
        command = assert(this.ownerDocument.body.querySelector(command));
        cr.ui.decorate(command, Command);
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
    isSeparator: function() {
      return this.tagName == 'HR';
    },

    /**
     * Updates shortcut text according to associated command. If command has
     * multiple shortcuts, only first one is displayed.
     */
    updateShortcut_: function() {
      this.removeAttribute('shortcutText');

      if (!this.command_ || !this.command_.shortcut ||
          this.command_.hideShortcutText) {
        return;
      }

      const shortcuts = this.command_.shortcut.split(/\s+/);

      if (shortcuts.length == 0) {
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

      if (ident == ' ') {
        ident = 'Space';
      }

      if (ident.length != 1) {
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
    handleMouseUp_: function(e) {
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
            cr.ui.swallowDoubleClick(e);
          }
        }
      }
    },

    /**
     * Updates command according to the node on which this menu was invoked.
     * @param {Node=} opt_node Node on which menu was opened.
     */
    updateCommand: function(opt_node) {
      if (this.command_) {
        this.command_.canExecuteChange(opt_node);
      }
    },

    /**
     * Handles changes to the associated command.
     * @param {Event} e The event object.
     */
    handleEvent: function(e) {
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
    }
  };

  /**
   * Whether the menu item is disabled or not.
   */
  cr.defineProperty(MenuItem, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is hidden or not.
   */
  cr.defineProperty(MenuItem, 'hidden', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is selected or not.
   */
  cr.defineProperty(MenuItem, 'selected', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is checked or not.
   */
  cr.defineProperty(MenuItem, 'checked', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is checkable or not.
   */
  cr.defineProperty(MenuItem, 'checkable', cr.PropertyKind.BOOL_ATTR);

  // Export
  return {MenuItem: MenuItem};
});
