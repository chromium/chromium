// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An item in a drop-down menu in the ChromeVox panel.
 */

goog.provide('PanelMenuItem');

/**
 * @param {string} menuItemTitle The title of the menu item.
 * @param {string} menuItemShortcut The keystrokes to select this item.
 * @param {Function} callback The function to call if this item is selected.
 * @constructor
 */
PanelMenuItem = function(menuItemTitle, menuItemShortcut, callback) {
  this.callback = callback;

  this.element = document.createElement('tr');
  this.element.className = 'menu-item';
  this.element.tabIndex = -1;
  this.element.setAttribute('role', 'menuitem');

  this.element.addEventListener(
      'mouseover', (function(evt) {
                     this.element.focus();
                   }).bind(this),
      false);

  var title = document.createElement('td');
  title.className = 'menu-item-title';
  title.textContent = menuItemTitle;
  this.element.appendChild(title);

  var shortcut = document.createElement('td');
  shortcut.className = 'menu-item-shortcut';
  shortcut.textContent = menuItemShortcut;
  this.element.appendChild(shortcut);
};
