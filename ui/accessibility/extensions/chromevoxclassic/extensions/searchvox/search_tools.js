// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Uses ChromeVox API to access the search tools menu.
 */

goog.provide('cvox.SearchTool');

goog.require('cvox.ChromeVox');
goog.require('cvox.DomUtil');
goog.require('cvox.Search');
goog.require('cvox.SearchConstants');
goog.require('cvox.SearchUtil');

/**
 * @constructor
 */
cvox.SearchTool = function() {
};

/**
 * Index of the current menu in focus.
 * @type {number}
 */
cvox.SearchTool.menuIndex;

/**
 * Array of menus.
 * @type {Array<Node>}
 */
cvox.SearchTool.menus = [];

/**
 * Index of the current menu item in focus.
 * @type {number}
 */
cvox.SearchTool.menuItemIndex;

/**
 * Array of menu items for the current menu.
 * @type {Array<Node>}
 */
cvox.SearchTool.menuItems = [];

/**
 * Id of the clear button.
 * @type {string}
 */
cvox.SearchTool.CLEAR_ID = 'hdtb_rst';

/**
 * Toggles a menu open / close by simulating a click.
 */
cvox.SearchTool.toggleMenu = function() {
  var menu = cvox.SearchTool.menus[cvox.SearchTool.menuIndex];
  var menuDiv = menu.previousSibling;
  cvox.DomUtil.clickElem(menuDiv, false, false, false);
};

/**
 * Syncs the first item in the current menu to ChromeVox.
 */
cvox.SearchTool.syncToMenu = function() {
  cvox.SearchTool.menuItemIndex = 0;
  cvox.SearchTool.toggleMenu();
  cvox.SearchTool.populateMenuItems();
  cvox.SearchTool.syncToMenuItem();
};

/**
 * Syncs the current menu item to ChromeVox.
 */
cvox.SearchTool.syncToMenuItem = function() {
  var menuItem = cvox.SearchTool.menuItems[cvox.SearchTool.menuItemIndex];
  cvox.ChromeVox.syncToNode(menuItem, true);
};

/**
 * Fills in menuItems with the current menu's items.
 */
cvox.SearchTool.populateMenuItems = function() {
  var menu = cvox.SearchTool.menus[cvox.SearchTool.menuIndex];
  cvox.SearchTool.menuItems = [];
  /* For now, we just special case on the clear button. */
  if (menu.id !== cvox.SearchTool.CLEAR_ID) {
    var MENU_ITEM_SELECTOR = '.hdtbItm';
    var menuItemNodeList = menu.querySelectorAll(MENU_ITEM_SELECTOR);
    for (var i = 0; i < menuItemNodeList.length; i++) {
      cvox.SearchTool.menuItems.push(menuItemNodeList.item(i));
    }
  } else {
    cvox.SearchTool.menuItems = [];
    cvox.SearchTool.menuItems.push(menu);
  }
};

/**
 * Fills in menus with the menus of the page.
 */
cvox.SearchTool.populateMenus = function() {
  var MENU_SELECTOR = '.hdtbU';
  var menuDivs = document.querySelectorAll(MENU_SELECTOR);
  for (var i = 0; i < menuDivs.length; i++) {
    cvox.SearchTool.menus.push(menuDivs.item(i));
  }

  var clearDiv = document.getElementById(cvox.SearchTool.CLEAR_ID);
  if (clearDiv) {
    cvox.SearchTool.menus.push(clearDiv);
  }
};

/**
 * Switches focus to the tools interface, giving keyboard access.
 */
cvox.SearchTool.activateTools = function() {
  var MENU_BAR_SELECTOR = '#hdtbMenus';
  var menuBar = document.querySelector(MENU_BAR_SELECTOR);
  var MENUS_OPEN_CLASS = 'hdtb-td-o';
  menuBar.className = MENUS_OPEN_CLASS;

  cvox.SearchTool.populateMenus();
  cvox.SearchTool.menuIndex = 0;
  cvox.SearchTool.syncToMenu();
};

/**
 * Goes to the link of the current menu item action.
 */
cvox.SearchTool.gotoMenuItem = function() {
  var menuItem = cvox.SearchTool.menuItems[cvox.SearchTool.menuItemIndex];
  var LOCATION_INPUT_ID = '#lc-input';
  var input = menuItem.querySelector(LOCATION_INPUT_ID);
  /* Special case for setting location. */
  if (input) {
    input.focus();
    return;
  }

  /* Custom Date Range. */
  var CDR_ID = 'cdr_opt';
  switch (menuItem.id) {
  case cvox.SearchTool.CLEAR_ID:
    window.location = menuItem.dataset.url;
    break;
  case CDR_ID:
    var CDR_LINK_SELECTOR = '#cdrlnk';
    var cdrLink = menuItem.querySelector(CDR_LINK_SELECTOR);
    cvox.DomUtil.clickElem(cdrLink, false, false, false);
    cvox.SearchTool.toggleMenu();
    break;
  default:
    window.location = cvox.SearchUtil.extractURL(menuItem);
    break;
  }
};

/**
 * Handles key events for the tools interface.
 * @param {Event} evt Keydown event.
 * @return {boolean} True if key was handled, false otherwise.
 */
cvox.SearchTool.keyhandler = function(evt) {
  if (cvox.SearchUtil.isSearchWidgetActive()) {
    return false;
  }

  switch (evt.keyCode) {
  case cvox.SearchConstants.KeyCode.UP:
    cvox.SearchTool.menuItemIndex = cvox.SearchUtil.subOneWrap(
      cvox.SearchTool.menuItemIndex, cvox.SearchTool.menuItems.length);
    cvox.SearchTool.syncToMenuItem();
    break;

  case cvox.SearchConstants.KeyCode.DOWN:
    cvox.SearchTool.menuItemIndex = cvox.SearchUtil.addOneWrap(
      cvox.SearchTool.menuItemIndex, cvox.SearchTool.menuItems.length);
    cvox.SearchTool.syncToMenuItem();
    break;

  case cvox.SearchConstants.KeyCode.LEFT:
    cvox.SearchTool.toggleMenu();
    cvox.SearchTool.menuIndex = cvox.SearchUtil.subOneWrap(
      cvox.SearchTool.menuIndex, cvox.SearchTool.menus.length);
    cvox.SearchTool.syncToMenu();
    break;

  case cvox.SearchConstants.KeyCode.RIGHT:
    cvox.SearchTool.toggleMenu();
    cvox.SearchTool.menuIndex = cvox.SearchUtil.addOneWrap(
      cvox.SearchTool.menuIndex, cvox.SearchTool.menus.length);
    cvox.SearchTool.syncToMenu();
    break;

  case cvox.SearchConstants.KeyCode.ENTER:
    cvox.SearchTool.gotoMenuItem();
    break;

  default:
    return false;
  }
  evt.preventDefault();
  evt.stopPropagation();
  return true;
};
