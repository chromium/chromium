// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A minimal keyboard handler.
 */

goog.provide('KeyboardHandler');

// Implicit dependency on cvox.ChromeVoxKbHandler; cannot include here because
// ChromeVoxKbHandler redefines members.

/**
 * @constructor
 */
KeyboardHandler = function() {
  cvox.ChromeVoxKbHandler.commandHandler = this.handleCommand_.bind(this);
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();
  document.addEventListener('keydown', this.handleKeyDown_.bind(this), false);

  // Register for Classic pref changes to get sticky mode state.
  cvox.ExtensionBridge.addMessageListener(function(msg) {
    if (msg['prefs']) {
      var prefs = msg['prefs'];
      cvox.ChromeVox.isStickyPrefOn = prefs['sticky'] == 'true';
    }
  });

  // Make the initial request for prefs.
  cvox.ExtensionBridge.send({
    'target': 'Prefs',
    'action': 'getPrefs'
  });
};

KeyboardHandler.prototype = {
  /**
   * @param {Event} evt
   * @private
   */
  handleKeyDown_: function(evt) {
    cvox.ExtensionBridge.send({
      'target': 'next',
      'action': 'flushNextUtterance'
    });

    evt.stickyMode = cvox.ChromeVox.isStickyPrefOn;

    cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  },

  /**
   * @param {string} command
   * @private
   */
  handleCommand_: function(command) {
    cvox.ExtensionBridge.send({
      'target': 'next',
      'action': 'onCommand',
      'command': command
    });
  }
};
