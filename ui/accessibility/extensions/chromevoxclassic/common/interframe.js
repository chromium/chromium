// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools for interframe communication. To use this class, every
 * window that wants to communicate with its child iframes should enumerate
 * them using document.getElementsByTagName('iframe'), create an ID to
 * associate with that iframe, then call cvox.Interframe.sendIdToIFrame
 * on each of them. Then use cvox.Interframe.sendMessageToIFrame to send
 * messages to that iframe and cvox.Interframe.addListener to receive
 * replies. When a reply is received, it will automatically contain the ID of
 * that iframe as a parameter.
 *
 */

goog.provide('cvox.Interframe');

goog.require('cvox.ChromeVoxJSON');
goog.require('cvox.DomUtil');

/**
 * @constructor
 */
cvox.Interframe = function() {
};

/**
 * The prefix of all interframe messages.
 * @type {string}
 * @const
 */
cvox.Interframe.IF_MSG_PREFIX = 'cvox.INTERFRAME:';

/**
 * The message used to set the ID of a child frame so that it can send replies
 * to its parent frame.
 * @type {string}
 * @const
 */
cvox.Interframe.SET_ID = 'cvox.INTERFRAME_SET_ID';

/**
 * The message used by a child frame to acknowledge an id was set (sent to its
 * parent frame.
 * @type {string}
 * @const
 */
cvox.Interframe.ACK_SET_ID = 'cvox.INTERFRAME_ACK_SET_ID';

/**
 * The ID of this window (relative to its parent farme).
 * @type {number|string|undefined}
 */
cvox.Interframe.id;

/**
 * Array of functions that have been registered as listeners to interframe
 * messages send to this window.
 * @type {Array<function(Object)>}
 */
cvox.Interframe.listeners = [];

/**
 * Maps an id to a function which gets called when a frame first sends an ack
 * for a set id msg.
 @dict {!Object<number|string, function()>}
 * @private
 */
cvox.Interframe.idToCallback_ = {};

/**
 * Flag for unit testing. When false, skips over iframe.contentWindow check
 * in sendMessageToIframe. This is needed because in the wild, ChromeVox may
 * not have access to iframe.contentWindow due to the same-origin security
 * policy. There is no reason to set this outside of a test.
 * @type {boolean}
 */
cvox.Interframe.allowAccessToIframeContentWindow = true;

/**
 * Initializes the cvox.Interframe module. (This is called automatically.)
 */
cvox.Interframe.init = function() {
  cvox.Interframe.messageListener = function(event) {
    if (typeof event.data === 'string' &&
        event.data.indexOf(cvox.Interframe.IF_MSG_PREFIX) == 0) {
      var suffix = event.data.substr(cvox.Interframe.IF_MSG_PREFIX.length);
      var message = /** @type {Object} */ (
          cvox.ChromeVoxJSON.parse(suffix));
      if (message['command'] == cvox.Interframe.SET_ID) {
        cvox.Interframe.id = message['id'];
        message['command'] = cvox.Interframe.ACK_SET_ID;
        cvox.Interframe.sendMessageToParentWindow(message);
      } else if (message['command'] == cvox.Interframe.ACK_SET_ID) {
        cvox.Interframe.id = message['id'];
        var callback = cvox.Interframe.idToCallback_[cvox.Interframe.id];
        callback();
      }
      for (var i = 0, listener; listener = cvox.Interframe.listeners[i]; i++) {
        listener(message);
      }
    }
    return false;
  };
  window.addEventListener('message', cvox.Interframe.messageListener, true);
};

/**
 * Unregister the main window event listener. Intended for clean unit testing;
 * normally there's no reason to call this outside of a test.
 */
cvox.Interframe.shutdown = function() {
  window.removeEventListener('message', cvox.Interframe.messageListener, true);
};

/**
 * Register a function to listen to all interframe communication messages.
 * Messages from a child frame will have a parameter 'id' that you assigned
 * when you called cvox.Interframe.sendIdToIFrame.
 * @param {function(Object)} listener The listener function.
 */
cvox.Interframe.addListener = function(listener) {
  cvox.Interframe.listeners.push(listener);
};

/**
 * Send a message to another window.
 * @param {Object} message The message to send.
 * @param {Window} window The window to receive the message.
 */
cvox.Interframe.sendMessageToWindow = function(message, window) {
  var encodedMessage = cvox.Interframe.IF_MSG_PREFIX +
      cvox.ChromeVoxJSON.stringify(message, null, null);
  window.postMessage(encodedMessage, '*');
};

/**
 * Send a message to another iframe.
 * @param {Object} message The message to send. The message must have an 'id'
 *     parameter in order to be sent.
 * @param {HTMLIFrameElement} iframe The iframe to send the message to.
 */
cvox.Interframe.sendMessageToIFrame = function(message, iframe) {
  if (cvox.Interframe.allowAccessToIframeContentWindow &&
      iframe.contentWindow) {
    cvox.Interframe.sendMessageToWindow(message, iframe.contentWindow);
    return;
  }

  // A content script can't access window.parent, but the page can, so
  // inject a tiny bit of javascript into the page.
  var encodedMessage = cvox.Interframe.IF_MSG_PREFIX +
      cvox.ChromeVoxJSON.stringify(message, null, null);
  var script = document.createElement('script');
  script.type = 'text/javascript';

  // TODO: Make this logic more like makeNodeReference_ inside api.js
  // (line 126) so we can use an attribute instead of a classname
  if (iframe.hasAttribute('id') &&
      document.getElementById(iframe.id) == iframe) {
    // Ideally, try to send it based on the iframe's existing id.
    script.innerHTML =
        'document.getElementById(decodeURI(\'' +
        encodeURI(iframe.id) + '\')).contentWindow.postMessage(decodeURI(\'' +
        encodeURI(encodedMessage) + '\'), \'*\');';
  } else {
    // If not, add a style name and send it based on that.
    var styleName = 'cvox_iframe' + message['id'];
    if (iframe.className === '') {
      iframe.className = styleName;
    } else if (iframe.className.indexOf(styleName) == -1) {
      iframe.className += ' ' + styleName;
    }

    script.innerHTML =
        'document.getElementsByClassName(decodeURI(\'' +
        encodeURI(styleName) +
        '\'))[0].contentWindow.postMessage(decodeURI(\'' +
        encodeURI(encodedMessage) + '\'), \'*\');';
  }

  // Remove the script so we don't leave any clutter.
  document.head.appendChild(script);
  window.setTimeout(function() {
    document.head.removeChild(script);
  }, 1000);
};

/**
 * Send a message to the parent window of this window, if any. If the parent
 * assigned this window an ID, sends back the ID in the reply automatically.
 * @param {Object} message The message to send.
 */
cvox.Interframe.sendMessageToParentWindow = function(message) {
  if (!cvox.Interframe.isIframe()) {
    return;
  }

  message['sourceId'] = cvox.Interframe.id;
  if (window.parent) {
    cvox.Interframe.sendMessageToWindow(message, window.parent);
    return;
  }

  // A content script can't access window.parent, but the page can, so
  // use window.location.href to execute a simple line of javascript in
  // the page context.
  var encodedMessage = cvox.Interframe.IF_MSG_PREFIX +
      cvox.ChromeVoxJSON.stringify(message, null, null);
  window.location.href =
      'javascript:window.parent.postMessage(\'' +
      encodeURI(encodedMessage) + '\', \'*\');';
};

/**
 * Send the given ID to a child iframe.
 * @param {number|string} id The ID you want to receive in replies from
 *     this iframe.
 * @param {HTMLIFrameElement} iframe The iframe to assign.
 * @param {function()=} opt_callback Called when a ack msg arrives from the
 *frame.
 */
cvox.Interframe.sendIdToIFrame = function(id, iframe, opt_callback) {
  if (opt_callback) {
    cvox.Interframe.idToCallback_[id] = opt_callback;
  }
  var message = {'command': cvox.Interframe.SET_ID, 'id': id};
  cvox.Interframe.sendMessageToIFrame(message, iframe);
};

/**
 * Returns true if inside iframe
 * @return {boolean} true if inside iframe.
 */
cvox.Interframe.isIframe = function() {
  return (window != window.parent);
};

cvox.Interframe.init();
