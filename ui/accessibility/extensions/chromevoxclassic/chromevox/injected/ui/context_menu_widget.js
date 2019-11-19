// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Widget presenting context menus.
 */

goog.provide('cvox.ContextMenuWidget');

goog.require('cvox.ChromeVox');
goog.require('cvox.OverlayWidget');
goog.require('cvox.UserEventDetail');

var CONTEXT_MENU_ATTR = 'contextMenuActions';
/**
 * Return the list from a node or an ancestor.
 * Note: If there are multiple lists, this well return the closest.
 * @private
 * @param {Object} node Node to extract from.
 * @return {*} Extracted list.
 */
var extractMenuList_ = function(node) {
  var curr = node;
  while (curr !== document) {
    var menuListJSON = curr.getAttribute(CONTEXT_MENU_ATTR);
    if (menuListJSON) {
      return JSON.parse(menuListJSON);
    }
    curr = curr.parentNode;
  }
  return null;
};

/**
 * Gets the current element node.
 * @private
 * @return {Node} Current element node.
 */
var getCurrentElement_ = function() {
  var currNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  while (currNode.nodeType !== Node.ELEMENT_NODE) {
    currNode = currNode.parentNode;
  }
  return currNode;
};

/**
 * @constructor
 * @extends {cvox.OverlayWidget}
 */
cvox.ContextMenuWidget = function() {
  goog.base(this, '');
  this.container_ = document.createElement('div');

  /**
   * The element that triggered the ContextMenu.
   * @private
   * @type {Node}
   */
  this.triggerElement_ = getCurrentElement_();

  /**
   * List of menu items in the context menu.
   */
  this.menuList = extractMenuList_(this.triggerElement_);

  if (!this.menuList) {
    console.log('No context menu found.');
    return;
  }

  this.menuList.forEach(goog.bind(function(menuItem) {
    if (menuItem['desc'] || menuItem['cmd']) {
      var desc = menuItem['desc'];
      var cmd = menuItem['cmd'];

      var menuElem = document.createElement('p');
      menuElem.id = cmd;
      menuElem.textContent = desc;
      menuElem.setAttribute('role', 'menuitem');
      this.container_.appendChild(menuElem);
    }
  }, this));
};
goog.inherits(cvox.ContextMenuWidget, cvox.OverlayWidget);

/**
 * @override
 */
cvox.ContextMenuWidget.prototype.show = function() {
  if (this.menuList) {
    goog.base(this, 'show');
    this.host_.appendChild(this.container_);
  }
};

/**
 * @override
 */
cvox.ContextMenuWidget.prototype.getNameMsg = function() {
  return ['context_menu_intro'];
};

/**
 * @override
 */
cvox.ContextMenuWidget.prototype.onKeyDown = function(evt) {
  var ENTER_KEYCODE = 13;
  if (evt.keyCode == ENTER_KEYCODE) {
    var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
    var cmd = currentNode.parentNode.id;

    /* Dispatch the event. */
    var detail = new cvox.UserEventDetail({customCommand: cmd});
    var userEvt = detail.createEventObject();
    this.triggerElement_.dispatchEvent(userEvt);
    this.hide();

    evt.preventDefault();
    evt.stopPropagation();
    return true;
  } else {
    return goog.base(this, 'onKeyDown', evt);
  }
};
