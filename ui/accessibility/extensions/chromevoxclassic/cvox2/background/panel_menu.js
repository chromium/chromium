// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A drop-down menu in the ChromeVox panel.
 */

goog.provide('PanelMenu');
goog.provide('PanelNodeMenu');

goog.require('PanelMenuItem');
goog.require('constants');

/**
 * @param {string} menuMsg The msg id of the menu.
 * @constructor
 */
PanelMenu = function(menuMsg) {
  /** @type {string} */
  this.menuMsg = menuMsg;
  // The item in the menu bar containing the menu's title.
  this.menuBarItemElement = document.createElement('div');
  this.menuBarItemElement.className = 'menu-bar-item';
  this.menuBarItemElement.setAttribute('role', 'menu');
  var menuTitle = Msgs.getMsg(menuMsg);
  this.menuBarItemElement.textContent = menuTitle;

  // The container for the menu. This part is fixed and scrolls its
  // contents if necessary.
  this.menuContainerElement = document.createElement('div');
  this.menuContainerElement.className = 'menu-container';
  this.menuContainerElement.style.visibility = 'hidden';

  // The menu itself. It contains all of the items, and it scrolls within
  // its container.
  this.menuElement = document.createElement('table');
  this.menuElement.className = 'menu';
  this.menuElement.setAttribute('role', 'menu');
  this.menuElement.setAttribute('aria-label', menuTitle);
  this.menuContainerElement.appendChild(this.menuElement);

  /**
   * The items in the menu.
   * @type {Array<PanelMenuItem>}
   * @private
   */
  this.items_ = [];

  /**
   * The return value from window.setTimeout for a function to update the
   * scroll bars after an item has been added to a menu. Used so that we
   * don't re-layout too many times.
   * @type {?number}
   * @private
   */
  this.updateScrollbarsTimeout_ = null;

  /**
   * The current active menu item index, or -1 if none.
   * @type {number}
   * @private
   */
  this.activeIndex_ = -1;
};

PanelMenu.prototype = {
  /**
   * @param {string} menuItemTitle The title of the menu item.
   * @param {string} menuItemShortcut The keystrokes to select this item.
   * @param {Function} callback The function to call if this item is selected.
   * @return {!PanelMenuItem} The menu item just created.
   */
  addMenuItem: function(menuItemTitle, menuItemShortcut, callback) {
    var menuItem = new PanelMenuItem(menuItemTitle, menuItemShortcut, callback);
    this.items_.push(menuItem);
    this.menuElement.appendChild(menuItem.element);

    // Sync the active index with focus.
    menuItem.element.addEventListener(
        'focus', (function(index, event) {
                   this.activeIndex_ = index;
                 }).bind(this, this.items_.length - 1),
        false);

    // Update the container height, adding a scroll bar if necessary - but
    // to avoid excessive layout, schedule this once per batch of adding
    // menu items rather than after each add.
    if (!this.updateScrollbarsTimeout_) {
      this.updateScrollbarsTimeout_ = window.setTimeout(
          (function() {
            var menuBounds = this.menuElement.getBoundingClientRect();
            var maxHeight = window.innerHeight - menuBounds.top;
            this.menuContainerElement.style.maxHeight = maxHeight + 'px';
            this.updateScrollbarsTimeout_ = null;
          }).bind(this),
          0);
    }

    return menuItem;
  },

  /**
   * Activate this menu, which means showing it and positioning it on the
   * screen underneath its title in the menu bar.
   */
  activate: function() {
    this.menuContainerElement.style.visibility = 'visible';
    this.menuContainerElement.style.opacity = 1;
    this.menuBarItemElement.classList.add('active');
    var barBounds =
        this.menuBarItemElement.parentElement.getBoundingClientRect();
    var titleBounds = this.menuBarItemElement.getBoundingClientRect();
    var menuBounds = this.menuElement.getBoundingClientRect();

    this.menuElement.style.minWidth = titleBounds.width + 'px';
    this.menuContainerElement.style.minWidth = titleBounds.width + 'px';
    if (titleBounds.left + menuBounds.width < barBounds.width) {
      this.menuContainerElement.style.left = titleBounds.left + 'px';
    } else {
      this.menuContainerElement.style.left =
          (titleBounds.right - menuBounds.width) + 'px';
    }

    // Make the first item active.
    this.activateItem(0);
  },

  /**
   * Hide this menu. Make it invisible first to minimize spurious
   * accessibility events before the next menu activates.
   */
  deactivate: function() {
    this.menuContainerElement.style.opacity = 0.001;
    this.menuBarItemElement.classList.remove('active');
    this.activeIndex_ = -1;

    window.setTimeout(
        (function() {
          this.menuContainerElement.style.visibility = 'hidden';
        }).bind(this),
        0);
  },

  /**
   * Make a specific menu item index active.
   * @param {number} itemIndex The index of the menu item.
   */
  activateItem: function(itemIndex) {
    this.activeIndex_ = itemIndex;
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length)
      this.items_[this.activeIndex_].element.focus();
  },

  /**
   * Advanced the active menu item index by a given number.
   * @param {number} delta The number to add to the active menu item index.
   */
  advanceItemBy: function(delta) {
    if (this.activeIndex_ >= 0) {
      this.activeIndex_ += delta;
      this.activeIndex_ =
          (this.activeIndex_ + this.items_.length) % this.items_.length;
    } else {
      if (delta >= 0)
        this.activeIndex_ = 0;
      else
        this.activeIndex_ = this.menus_.length - 1;
    }

    this.items_[this.activeIndex_].element.focus();
  },

  /**
   * Get the callback for the active menu item.
   * @return {Function} The callback.
   */
  getCallbackForCurrentItem: function() {
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      return this.items_[this.activeIndex_].callback;
    }
    return null;
  },

  /**
   * Get the callback for a menu item given its DOM element.
   * @param {Element} element The DOM element.
   * @return {Function} The callback.
   */
  getCallbackForElement: function(element) {
    for (var i = 0; i < this.items_.length; i++) {
      if (element == this.items_[i].element)
        return this.items_[i].callback;
    }
    return null;
  }
};

/**
 * @param {string} menuMsg The msg id of the menu.
 * @param {chrome.automation.AutomationNode} node ChromeVox's current position.
 * @param {AutomationPredicate.Unary} pred Filter to use on the document.
 * @extends {PanelMenu}
 * @constructor
 */
PanelNodeMenu = function(menuMsg, node, pred) {
  PanelMenu.call(this, menuMsg);
  var nodes = [];
  var selectNext = false;
  var activeIndex = -1;
  AutomationUtil.findNodePre(
      node.root, constants.Dir.FORWARD,
      /** @type {AutomationPredicate.Unary} */ (function(n) {
        if (n === node)
          selectNext = true;

        if (pred(n)) {
          this.addMenuItem(n.name, '', function() {
            chrome.extension.getBackgroundPage()
                .ChromeVoxState.instance['navigateToRange'](
                    cursors.Range.fromNode(n));
          });
          if (selectNext) {
            activeIndex = this.items_.length - 1;
            selectNext = false;
          }
        }
      }).bind(this));

  if (!this.items_.length) {
    this.addMenuItem(Msgs.getMsg('panel_menu_item_none'), '', function() {});
    this.activateItem(0);
  }
  if (activeIndex >= 0)
    this.activateItem(activeIndex);
};

PanelNodeMenu.prototype = {
  __proto__: PanelMenu.prototype,

  /** @override */
  activate: function() {
    var activeItem = this.activeIndex_;
    PanelMenu.prototype.activate.call(this);
    this.activateItem(activeItem);
  }
};
