// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox panel and menus.
 */

goog.provide('Panel');

goog.require('ISearchUI');
goog.require('Msgs');
goog.require('PanelCommand');
goog.require('PanelMenu');
goog.require('PanelMenuItem');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.CommandStore');

/**
 * Class to manage the panel.
 * @constructor
 */
Panel = function() {};

/**
 * Initialize the panel.
 */
Panel.init = function() {
  /** @type {Element} @private */
  this.speechContainer_ = $('speech-container');

  /** @type {Element} @private */
  this.speechElement_ = $('speech');

  /** @type {Element} @private */
  this.brailleContainer_ = $('braille-container');

  /** @type {Element} @private */
  this.searchContainer_ = $('search-container');

  /** @type {Element} @private */
  this.searchInput_ = $('search');

  /** @type {Element} @private */
  this.brailleTextElement_ = $('braille-text');

  /** @type {Element} @private */
  this.brailleCellsElement_ = $('braille-cells');

  /**
   * The array of top-level menus.
   * @type {!Array<PanelMenu>}
   * @private
   */
  this.menus_ = [];

  /**
   * The currently active menu, if any.
   * @type {PanelMenu}
   * @private
   */
  this.activeMenu_ = null;

  /**
   * True if the menu button in the panel is enabled at all. It's disabled if
   * ChromeVox Next is not active.
   * @type {boolean}
   * @private
   */
  this.menusEnabled_ = false;

  /**
   * A callback function to be executed to perform the action from selecting
   * a menu item after the menu has been closed and focus has been restored
   * to the page or wherever it was previously.
   * @type {?Function}
   * @private
   */
  this.pendingCallback_ = null;

  /**
   * True if we're currently in incremental search mode.
   * @type {boolean}
   * @private
   */
  this.searching_ = false;

  Panel.updateFromPrefs();

  Msgs.addTranslatedMessagesToDom(document);

  window.addEventListener('storage', function(event) {
    if (event.key == 'brailleCaptions') {
      Panel.updateFromPrefs();
    }
  }, false);

  window.addEventListener('message', function(message) {
    var command = JSON.parse(message.data);
    Panel.exec(/** @type {PanelCommand} */ (command));
  }, false);

  $('menus_button').addEventListener('mousedown', Panel.onOpenMenus, false);
  $('options').addEventListener('click', Panel.onOptions, false);
  $('close').addEventListener('click', Panel.onClose, false);

  document.addEventListener('keydown', Panel.onKeyDown, false);
  document.addEventListener('mouseup', Panel.onMouseUp, false);

  Panel.searchInput_.addEventListener('blur', Panel.onSearchInputBlur, false);
};

/**
 * Update the display based on prefs.
 */
Panel.updateFromPrefs = function() {
  if (Panel.searching_) {
    this.speechContainer_.style.display = 'none';
    this.brailleContainer_.style.display = 'none';
    this.searchContainer_.style.display = 'block';
    return;
  }

  this.speechContainer_.style.display = 'block';
  this.brailleContainer_.style.display = 'block';
  this.searchContainer_.style.display = 'none';

  if (localStorage['brailleCaptions'] === String(true)) {
    this.speechContainer_.style.visibility = 'hidden';
    this.brailleContainer_.style.visibility = 'visible';
  } else {
    this.speechContainer_.style.visibility = 'visible';
    this.brailleContainer_.style.visibility = 'hidden';
  }
};

/**
 * Execute a command to update the panel.
 *
 * @param {PanelCommand} command The command to execute.
 */
Panel.exec = function(command) {
  /**
   * Escape text so it can be safely added to HTML.
   * @param {*} str Text to be added to HTML, will be cast to string.
   * @return {string} The escaped string.
   */
  function escapeForHtml(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/\>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;')
        .replace(/\//g, '&#x2F;');
  }

  switch (command.type) {
    case PanelCommandType.CLEAR_SPEECH:
      this.speechElement_.innerHTML = '';
      break;
    case PanelCommandType.ADD_NORMAL_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML +=
          '<span class="usertext">' + escapeForHtml(command.data) + '</span>';
      break;
    case PanelCommandType.ADD_ANNOTATION_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML += escapeForHtml(command.data);
      break;
    case PanelCommandType.UPDATE_BRAILLE:
      this.brailleTextElement_.textContent = command.data.text;
      this.brailleCellsElement_.textContent = command.data.braille;
      break;
    case PanelCommandType.ENABLE_MENUS:
      Panel.onEnableMenus();
      break;
    case PanelCommandType.DISABLE_MENUS:
      Panel.onDisableMenus();
      break;
    case PanelCommandType.OPEN_MENUS:
      Panel.onOpenMenus(undefined, command.data);
      break;
    case PanelCommandType.SEARCH:
      Panel.onSearch();
      break;
  }
};

/**
 * Enable the ChromeVox Menus.
 */
Panel.onEnableMenus = function() {
  Panel.menusEnabled_ = true;
  $('menus_button').disabled = false;
  $('triangle').style.display = '';
};

/**
 * Disable the ChromeVox Menus.
 */
Panel.onDisableMenus = function() {
  Panel.menusEnabled_ = false;
  $('menus_button').disabled = true;
  $('triangle').style.display = 'none';
};

/**
 * Open / show the ChromeVox Menus.
 * @param {Event=} opt_event An optional event that triggered this.
 * @param {*=} opt_activateMenuTitle Title msg id of menu to open.
 */
Panel.onOpenMenus = function(opt_event, opt_activateMenuTitle) {
  // Don't open the menu if it's not enabled, such as when ChromeVox Next
  // is not active.
  if (!Panel.menusEnabled_)
    return;

  // Eat the event so that a mousedown isn't turned into a drag, allowing
  // users to click-drag-release to select a menu item.
  if (opt_event) {
    opt_event.stopPropagation();
    opt_event.preventDefault();
  }

  // Change the url fragment to 'fullscreen', which signals the native
  // host code to make the window fullscreen, revealing the menus.
  window.location = '#fullscreen';

  // Clear any existing menus and clear the callback.
  Panel.clearMenus();
  Panel.pendingCallback_ = null;

  // Build the top-level menus.
  var jumpMenu = Panel.addMenu('panel_menu_jump');
  var speechMenu = Panel.addMenu('panel_menu_speech');
  var tabsMenu = Panel.addMenu('panel_menu_tabs');
  var chromevoxMenu = Panel.addMenu('panel_menu_chromevox');

  // Create a mapping between categories from CommandStore, and our
  // top-level menus. Some categories aren't mapped to any menu.
  var categoryToMenu = {
    'navigation': jumpMenu,
    'jump_commands': jumpMenu,
    'controlling_speech': speechMenu,
    'modifier_keys': chromevoxMenu,
    'help_commands': chromevoxMenu,

    'information': null,  // Get link URL, get page title, etc.
    'overview': null,     // Headings list, etc.
    'tables': null,       // Table navigation.
    'braille': null,
    'developer': null
  };

  // Get the key map from the background page.
  var bkgnd = chrome.extension.getBackgroundPage();
  var keymap = bkgnd['cvox']['KeyMap']['fromCurrentKeyMap']();

  // Make a copy of the key bindings, get the localized title of each
  // command, and then sort them.
  var sortedBindings = keymap.bindings().slice();
  sortedBindings.forEach(goog.bind(function(binding) {
    var command = binding.command;
    var keySeq = binding.sequence;
    binding.keySeq = cvox.KeyUtil.keySequenceToString(keySeq, true);
    var titleMsgId = cvox.CommandStore.messageForCommand(command);
    if (!titleMsgId) {
      console.error('No localization for: ' + command);
      binding.title = '';
      return;
    }
    var title = Msgs.getMsg(titleMsgId);
    // Convert to title case.
    title = title.replace(/\w\S*/g, function(word) {
      return word.charAt(0).toUpperCase() + word.substr(1);
    });
    binding.title = title;
  }, this));
  sortedBindings.sort(function(binding1, binding2) {
    return binding1.title.localeCompare(binding2.title);
  });

  // Insert items from the bindings into the menus.
  sortedBindings.forEach(goog.bind(function(binding) {
    var category = cvox.CommandStore.categoryForCommand(binding.command);
    var menu = category ? categoryToMenu[category] : null;
    if (binding.title && menu) {
      menu.addMenuItem(binding.title, binding.keySeq, function() {
        var bkgnd =
            chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
        bkgnd['onGotCommand'](binding.command);
      });
    }
  }, this));

  // Add all open tabs to the Tabs menu.
  bkgnd.chrome.windows.getLastFocused(function(lastFocusedWindow) {
    bkgnd.chrome.windows.getAll({'populate': true}, function(windows) {
      for (var i = 0; i < windows.length; i++) {
        var tabs = windows[i].tabs;
        for (var j = 0; j < tabs.length; j++) {
          var title = tabs[j].title;
          if (tabs[j].active && windows[i].id == lastFocusedWindow.id)
            title += ' ' + Msgs.getMsg('active_tab');
          tabsMenu.addMenuItem(title, '', (function(win, tab) {
                                            bkgnd.chrome.windows.update(
                                                win.id, {focused: true},
                                                function() {
                                                  bkgnd.chrome.tabs.update(
                                                      tab.id, {active: true});
                                                });
                                          }).bind(this, windows[i], tabs[j]));
        }
      }
    });
  });

  // Add a menu item that disables / closes ChromeVox.
  chromevoxMenu.addMenuItem(
      Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', function() {
        Panel.onClose();
      });

  // Add menus for various role types.
  var node = bkgnd.ChromeVoxState.instance.getCurrentRange().start.node;
  Panel.addNodeMenu('role_heading', node, AutomationPredicate.heading);
  Panel.addNodeMenu('role_landmark', node, AutomationPredicate.landmark);
  Panel.addNodeMenu('role_link', node, AutomationPredicate.link);
  Panel.addNodeMenu('role_form', node, AutomationPredicate.formField);
  Panel.addNodeMenu('role_table', node, AutomationPredicate.table);

  // Activate either the specified menu or the first menu.
  var selectedMenu = Panel.menus_[0];
  for (var i = 0; i < Panel.menus_.length; i++) {
    if (this.menus_[i].menuMsg == opt_activateMenuTitle)
      selectedMenu = this.menus_[i];
  }
  Panel.activateMenu(selectedMenu);
};

/** Open incremental search. */
Panel.onSearch = function() {
  Panel.clearMenus();
  Panel.pendingCallback_ = null;
  Panel.searching_ = true;
  Panel.updateFromPrefs();

  window.location = '#focus';

  ISearchUI.get(Panel.searchInput_);
};

/**
 * Clear any previous menus. The menus are all regenerated each time the
 * menus are opened.
 */
Panel.clearMenus = function() {
  while (this.menus_.length) {
    var menu = this.menus_.pop();
    $('menu-bar').removeChild(menu.menuBarItemElement);
    $('menus_background').removeChild(menu.menuContainerElement);
  }
  this.activeMenu_ = null;
};

/**
 * Create a new menu with the given name and add it to the menu bar.
 * @param {string} menuMsg The msg id of the new menu to add.
 * @return {PanelMenu} The menu just created.
 */
Panel.addMenu = function(menuMsg) {
  var menu = new PanelMenu(menuMsg);
  $('menu-bar').appendChild(menu.menuBarItemElement);
  menu.menuBarItemElement.addEventListener('mouseover', function() {
    Panel.activateMenu(menu);
  }, false);

  $('menus_background').appendChild(menu.menuContainerElement);
  this.menus_.push(menu);
  return menu;
};


/**
 * Create a new node menu with the given name and add it to the menu bar.
 * @param {string} menuMsg The msg id of the new menu to add.
 * @param {chrome.automation.AutomationNode} node
 * @param {AutomationPredicate.Unary} pred
 * @return {PanelMenu} The menu just created.
 */
Panel.addNodeMenu = function(menuMsg, node, pred) {
  var menu = new PanelNodeMenu(menuMsg, node, pred);
  $('menu-bar').appendChild(menu.menuBarItemElement);
  menu.menuBarItemElement.addEventListener('mouseover', function() {
    Panel.activateMenu(menu);
  }, false);

  $('menus_background').appendChild(menu.menuContainerElement);
  this.menus_.push(menu);
  return menu;
};

/**
 * Activate a menu, which implies hiding the previous active menu.
 * @param {PanelMenu} menu The new menu to activate.
 */
Panel.activateMenu = function(menu) {
  if (menu == this.activeMenu_)
    return;

  if (this.activeMenu_) {
    this.activeMenu_.deactivate();
    this.activeMenu_ = null;
  }

  this.activeMenu_ = menu;
  this.pendingCallback_ = null;

  if (this.activeMenu_) {
    this.activeMenu_.activate();
  }
};

/**
 * Advance the index of the current active menu by |delta|.
 * @param {number} delta The number to add to the active menu index.
 */
Panel.advanceActiveMenuBy = function(delta) {
  var activeIndex = -1;
  for (var i = 0; i < this.menus_.length; i++) {
    if (this.activeMenu_ == this.menus_[i]) {
      activeIndex = i;
      break;
    }
  }

  if (activeIndex >= 0) {
    activeIndex += delta;
    activeIndex = (activeIndex + this.menus_.length) % this.menus_.length;
  } else {
    if (delta >= 0)
      activeIndex = 0;
    else
      activeIndex = this.menus_.length - 1;
  }
  Panel.activateMenu(this.menus_[activeIndex]);
};

/**
 * Advance the index of the current active menu item by |delta|.
 * @param {number} delta The number to add to the active menu item index.
 */
Panel.advanceItemBy = function(delta) {
  if (this.activeMenu_)
    this.activeMenu_.advanceItemBy(delta);
};

/**
 * Called when the user releases the mouse button. If it's anywhere other
 * than on the menus button, close the menus and return focus to the page,
 * and if the mouse was released over a menu item, execute that item's
 * callback.
 * @param {Event} event The mouse event.
 */
Panel.onMouseUp = function(event) {
  var target = event.target;
  while (target && !target.classList.contains('menu-item')) {
    // Allow the user to click and release on the menu button and leave
    // the menu button. Otherwise releasing the mouse anywhere else will
    // close the menu.
    if (target.id == 'menus_button')
      return;

    target = target.parentElement;
  }

  if (target && Panel.activeMenu_)
    Panel.pendingCallback_ = Panel.activeMenu_.getCallbackForElement(target);
  Panel.closeMenusAndRestoreFocus();
};

/**
 * Called when a key is pressed. Handle arrow keys to navigate the menus,
 * Esc to close, and Enter/Space to activate an item.
 * @param {Event} event The key event.
 */
Panel.onKeyDown = function(event) {
  if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey)
    return;

  switch (event.key) {
    case 'ArrowLeft':
      Panel.advanceActiveMenuBy(-1);
      break;
    case 'ArrowRight':
      Panel.advanceActiveMenuBy(1);
      break;
    case 'ArrowUp':
      Panel.advanceItemBy(-1);
      break;
    case 'ArrowDown':
      Panel.advanceItemBy(1);
      break;
    case 'Escape':
      Panel.closeMenusAndRestoreFocus();
      break;
    case 'Enter':
    case ' ':  // Space
      Panel.pendingCallback_ = Panel.getCallbackForCurrentItem();
      Panel.closeMenusAndRestoreFocus();
      break;
    default:
      // Don't mark this event as handled.
      return;
  }

  event.preventDefault();
  event.stopPropagation();
};

/**
 * Called when focus leaves the search input.
 */
Panel.onSearchInputBlur = function() {
  if (Panel.searching_) {
    if (document.activeElement != Panel.searchInput_ || !document.hasFocus()) {
      Panel.searching_ = false;
      if (window.location == '#focus')
        window.location = '#';
      Panel.updateFromPrefs();
      Panel.searchInput_.value = '';
    }
  }
};

/**
 * Open the ChromeVox Options.
 */
Panel.onOptions = function() {
  var bkgnd =
      chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
  bkgnd['showOptionsPage']();
  window.location = '#';
};

/**
 * Exit ChromeVox.
 */
Panel.onClose = function() {
  window.location = '#close';
};

/**
 * Get the callback for whatever item is currently selected.
 * @return {Function} The callback for the current item.
 */
Panel.getCallbackForCurrentItem = function() {
  if (this.activeMenu_)
    return this.activeMenu_.getCallbackForCurrentItem();
  return null;
};

/**
 * Close the menus and restore focus to the page. If a menu item's callback
 * was queued, execute it once focus is restored.
 */
Panel.closeMenusAndRestoreFocus = function() {
  // Make sure we're not in full-screen mode.
  window.location = '#';

  var bkgnd =
      chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
  bkgnd['endExcursion'](Panel.pendingCallback_);
};

window.addEventListener('load', function() {
  Panel.init();
}, false);

window.addEventListener('hashchange', function() {
  if (location.hash == '#fullscreen' || location.hash == '#focus') {
    this.originalStickyState_ = cvox.ChromeVox.isStickyPrefOn;
    cvox.ChromeVox.isStickyPrefOn = false;
  } else {
    cvox.ChromeVox.isStickyPrefOn = this.originalStickyState_;
  }
}, false);
