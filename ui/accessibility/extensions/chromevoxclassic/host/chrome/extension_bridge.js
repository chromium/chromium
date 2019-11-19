// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to aid in communication between a Chrome
 * background page and content script.
 *
 * It automatically figures out where it's being run and initializes itself
 * appropriately. Then just call send() to send a message from the background
 * to the page or vice versa, and addMessageListener() to provide a message
 * listener.  Messages can be any object that can be serialized using JSON.
 *
 */

goog.provide('cvox.ExtensionBridge');

goog.require('cvox.ChromeVoxJSON');

/**
 * @constructor
 */
cvox.ExtensionBridge = function() {};

/**
 * Initialize the extension bridge. Dynamically figure out whether we're in
 * the background page, content script, or in a page, and call the
 * corresponding function for more specific initialization.
 */
cvox.ExtensionBridge.init = function() {
  var self = cvox.ExtensionBridge;
  self.messageListeners = [];
  self.disconnectListeners = [];

  if (/^chrome-extension:\/\/.*background\.html$/.test(window.location.href)) {
    // This depends on the fact that the background page has a specific url. We
    // should never be loaded into another extension's background page, so this
    // is a safe check.
    self.context = self.BACKGROUND;
    self.initBackground();
    return;
  }

  if (chrome && chrome.extension) {
    self.context = self.CONTENT_SCRIPT;
    self.initContentScript();
  }
};

/**
 * Constant indicating we're in a background page.
 * @type {number}
 * @const
 */
cvox.ExtensionBridge.BACKGROUND = 0;

/**
 * Constant indicating we're in a content script.
 * @type {number}
 * @const
 */
cvox.ExtensionBridge.CONTENT_SCRIPT = 1;

/**
 * The name of the port between the content script and background page.
 * @type {string}
 * @const
 */
cvox.ExtensionBridge.PORT_NAME = 'cvox.ExtensionBridge.Port';

/**
 * The name of the message between the content script and background to
 * see if they're connected.
 * @type {string}
 * @const
 */
cvox.ExtensionBridge.PING_MSG = 'cvox.ExtensionBridge.Ping';

/**
 * The name of the message between the background and content script to
 * confirm that they're connected.
 * @type {string}
 * @const
 */
cvox.ExtensionBridge.PONG_MSG = 'cvox.ExtensionBridge.Pong';

/**
 * Send a message. If the context is a page, sends a message to the
 * extension background page. If the context is a background page, sends
 * a message to the current active tab (not all tabs).
 *
 * @param {Object} message The message to be sent.
 */
cvox.ExtensionBridge.send = function(message) {
  var self = cvox.ExtensionBridge;
  switch (self.context) {
  case self.BACKGROUND:
    self.sendBackgroundToContentScript(message);
    break;
  case self.CONTENT_SCRIPT:
    self.sendContentScriptToBackground(message);
    break;
  }
};

/**
 * Provide a function to listen to messages. In page context, this
 * listens to messages from the background. In background context,
 * this listens to messages from all pages.
 *
 * The function gets called with two parameters: the message, and a
 * port that can be used to send replies.
 *
 * @param {function(Object, Port)} listener The message listener.
 */
cvox.ExtensionBridge.addMessageListener = function(listener) {
  cvox.ExtensionBridge.messageListeners.push(listener);
};

/**
 * Provide a function to be called when the connection is
 * disconnected.
 *
 * @param {function()} listener The listener.
 */
cvox.ExtensionBridge.addDisconnectListener = function(listener) {
  cvox.ExtensionBridge.disconnectListeners.push(listener);
};

/**
 * Removes all message listeners from the extension bridge.
 */
cvox.ExtensionBridge.removeMessageListeners = function() {
  cvox.ExtensionBridge.messageListeners.length = 0;
};

/**
 * Returns a unique id for this instance of the script.
 *
 * @return {number}
 */
cvox.ExtensionBridge.uniqueId = function() {
  return cvox.ExtensionBridge.id_;
};

/**
 * Initialize the extension bridge in a background page context by registering
 * a listener for connections from the content script.
 */
cvox.ExtensionBridge.initBackground = function() {
  var self = cvox.ExtensionBridge;

  /** @type {!Array<Port>} @private */
  self.portCache_ = [];
  /** @type {number} */
  self.nextPongId_ = 1;
  /** @type {number} */
  self.id_ = 0;

  var onConnectHandler = function(port) {
    if (port.name != self.PORT_NAME) {
      return;
    }

    self.portCache_.push(port);

    port.onMessage.addListener(function(message) {
      if (message[cvox.ExtensionBridge.PING_MSG]) {
        var pongMessage = {};
        pongMessage[cvox.ExtensionBridge.PONG_MSG] = self.nextPongId_++;
        port.postMessage(pongMessage);
        return;
      }

      for (var i = 0; i < self.messageListeners.length; i++) {
        self.messageListeners[i](message, port);
      }
    });

    port.onDisconnect.addListener(function(message) {
      for (var i = 0; i < self.portCache_.length; i++) {
        if (self.portCache_[i] == port) {
          self.portCache_.splice(i, 1);
          break;
        }
      }
    });
  };

  chrome.extension.onConnect.addListener(onConnectHandler);
};

/**
 * Initialize the extension bridge in a content script context, listening
 * for messages from the background page.
 */
cvox.ExtensionBridge.initContentScript = function() {
  var self = cvox.ExtensionBridge;
  self.connected = false;
  self.pingAttempts = 0;
  self.queuedMessages = [];
  /** @type {number} */
  self.id_ = -1;

  var onMessageHandler = function(request, sender, sendResponse) {
    if (request && request['srcFile']) {
      // TODO (clchen, deboer): Investigate this further and come up with a
      // cleaner solution. The root issue is that this should never be run on
      // the background page, but it is in the Chrome OS case.
      return;
    }
    if (request[cvox.ExtensionBridge.PONG_MSG]) {
      self.gotPongFromBackgroundPage(request[cvox.ExtensionBridge.PONG_MSG]);
    } else {
      for (var i = 0; i < self.messageListeners.length; i++) {
        self.messageListeners[i](request, cvox.ExtensionBridge.backgroundPort);
      }
    }
    sendResponse({});
  };

  // Listen to requests from the background that don't come from
  // our connection port.
  chrome.extension.onMessage.addListener(onMessageHandler);

  self.setupBackgroundPort();

  self.tryToPingBackgroundPage();
};

/**
 * Set up the connection to the background page.
 */
cvox.ExtensionBridge.setupBackgroundPort = function() {
  // Set up the connection to the background page.
  var self = cvox.ExtensionBridge;
  self.backgroundPort = chrome.extension.connect({name: self.PORT_NAME});
  if (!self.backgroundPort) {
    return;
  }
  self.backgroundPort.onMessage.addListener(function(message) {
    if (message[cvox.ExtensionBridge.PONG_MSG]) {
      self.gotPongFromBackgroundPage(
          message[cvox.ExtensionBridge.PONG_MSG]);
    } else {
      for (var i = 0; i < self.messageListeners.length; i++) {
        self.messageListeners[i](message, self.backgroundPort);
      }
    }
  });
  self.backgroundPort.onDisconnect.addListener(function(event) {
    // If we're not connected yet, don't give up - try again.
    if (!self.connected) {
      self.backgroundPort = null;
      return;
    }

    for (var i = 0; i < self.disconnectListeners.length; i++) {
      self.disconnectListeners[i]();
    }
  });
};

/**
 * Try to ping the background page.
 */
cvox.ExtensionBridge.tryToPingBackgroundPage = function() {
  var self = cvox.ExtensionBridge;

  // If we already got a pong, great - we're done.
  if (self.connected) {
    return;
  }

  self.pingAttempts++;
  if (self.pingAttempts > 5) {
    // Could not connect after 5 ping attempts. Call the disconnect
    // handlers, which will disable ChromeVox.
    for (var i = 0; i < self.disconnectListeners.length; i++) {
      self.disconnectListeners[i]();
    }
    return;
  }

  // Send the ping.
  var msg = {};
  msg[cvox.ExtensionBridge.PING_MSG] = 1;
  if (!self.backgroundPort) {
    self.setupBackgroundPort();
  }
  if (self.backgroundPort) {
    self.backgroundPort.postMessage(msg);
  }

  // Check again in 500 ms in case we get no response.
  window.setTimeout(cvox.ExtensionBridge.tryToPingBackgroundPage, 500);
};

/**
 * Got pong from the background page, now we know the connection was
 * successful.
 * @param {number} pongId unique id assigned to us by the background page
 */
cvox.ExtensionBridge.gotPongFromBackgroundPage = function(pongId) {
  var self = cvox.ExtensionBridge;
  self.connected = true;
  self.id_ = pongId;

  while (self.queuedMessages.length > 0) {
    self.sendContentScriptToBackground(self.queuedMessages.shift());
  }
};

/**
 * Send a message from the content script to the background page.
 *
 * @param {Object} message The message to send.
 */
cvox.ExtensionBridge.sendContentScriptToBackground = function(message) {
  var self = cvox.ExtensionBridge;
  if (!self.connected) {
    // We're not connected to the background page, so queue this message
    // until we're connected.
    self.queuedMessages.push(message);
    return;
  }

  if (cvox.ExtensionBridge.backgroundPort) {
    cvox.ExtensionBridge.backgroundPort.postMessage(message);
  } else {
    chrome.extension.sendMessage(message);
  }
};

/**
 * Send a message from the background page to the content script of the
 * current selected tab.
 *
 * @param {Object} message The message to send.
 */
cvox.ExtensionBridge.sendBackgroundToContentScript = function(message) {
  cvox.ExtensionBridge.portCache_.forEach(function(port) {
    port.postMessage(message);
  });
};

cvox.ExtensionBridge.init();
