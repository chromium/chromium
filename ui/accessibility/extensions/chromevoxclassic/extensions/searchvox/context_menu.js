// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Context Menu for Search.
 */

goog.provide('cvox.SearchContextMenu');

goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.KeySequence');
goog.require('cvox.Search');
goog.require('cvox.SearchTool');

/**
 * @constructor
 */
cvox.SearchContextMenu = function() {
};

/* Globals */
var Command = {
  TOOLS: 'tools',
  ADS: 'ads',
  MAIN: 'main'
};

/**
 * Current focus Search is in.
 */
cvox.SearchContextMenu.currState = Command.MAIN;

/**
 * Handles context menu events.
 * @param {Event} evt Event received.
 */
cvox.SearchContextMenu.contextMenuHandler = function(evt) {
  var cmd = evt.detail['customCommand'];
  switch (cmd) {
  case Command.TOOLS:
    cvox.SearchContextMenu.focusTools();
    break;

  case Command.ADS:
    cvox.SearchContextMenu.focusAds();
    break;

  case Command.MAIN:
    cvox.SearchContextMenu.focusMain();
    break;
  }
};

/**
 * Handles key events.
 * @param {Event} evt Event received.
 * @return {boolean} True if key was handled, false otherwise.
 */
cvox.SearchContextMenu.keyhandler = function(evt) {
  var ret = false;
  var keySeq = new cvox.KeySequence(evt);
  var command = cvox.ChromeVoxKbHandler.handlerKeyMap.commandForKey(keySeq);
  /* Handle if just default action. */
  if (!command || command === 'performDefaultAction') {
    switch (cvox.SearchContextMenu.currState) {
      case Command.TOOLS:
        ret = cvox.SearchTool.keyhandler(evt);
        break;
      case Command.ADS:
      case Command.MAIN:
        ret = cvox.Search.keyhandler(evt);
        break;
    }
  }
  return ret;
};

/**
 * Switch to main search results focus.
 */
cvox.SearchContextMenu.focusMain = function() {
  if (cvox.SearchContextMenu.currState === Command.TOOLS) {
    cvox.SearchTool.toggleMenu();
  }
  cvox.Search.populateResults();
  cvox.Search.index = 0;
  cvox.Search.syncToIndex();
  cvox.SearchContextMenu.currState = Command.MAIN;
};

/**
 * Switch to ads focus.
 */
cvox.SearchContextMenu.focusAds = function() {
  cvox.Search.populateAdResults();
  if (cvox.Search.results.length === 0) {
    cvox.SearchContextMenu.focusMain();
    return;
  }
  cvox.Search.index = 0;
  cvox.Search.syncToIndex();

  if (cvox.SearchContextMenu.currState === Command.TOOLS) {
    cvox.SearchTool.toggleMenu();
  }

  cvox.SearchContextMenu.currState = Command.ADS;
};

/**
 * Switch to tools focus.
 */
cvox.SearchContextMenu.focusTools = function() {
  if (cvox.SearchContextMenu.currState !== Command.TOOLS) {
    cvox.SearchTool.activateTools();
    cvox.SearchContextMenu.currState = Command.TOOLS;
  }
};

/**
 * Initializes the context menu.
 */
cvox.SearchContextMenu.init = function() {
  var ACTIONS = [
    { desc: 'Main Results', cmd: Command.MAIN },
    { desc: 'Search Tools', cmd: Command.TOOLS },
    { desc: 'Ads', cmd: Command.ADS }
  ];
  /* Attach ContextMenuActions. */
  var body = document.querySelector('body');
  body.setAttribute('contextMenuActions', JSON.stringify(ACTIONS));

  /* Listen for ContextMenu events. */
  body.addEventListener('ATCustomEvent',
    cvox.SearchContextMenu.contextMenuHandler, true);

  window.addEventListener('keydown', cvox.SearchContextMenu.keyhandler, true);
  cvox.Search.init();
};
