// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Overlay that shows the current braille display contents,
 * both as text and braille, on screen in a document.
 */

goog.provide('cvox.BrailleOverlayWidget');

goog.require('cvox.ExtensionBridge');

/**
 * @constructor
 */
cvox.BrailleOverlayWidget = function() {
  /**
   * Whether the widget is active in the prefs.
   * @type {boolean}
   * @private
   */
  this.active_ = false;
  /**
   * @type {Element}
   * @private
   */
  this.containerNode_ = null;
  /**
   * @type {Element}
   * @private
   */
  this.contentNode_ = null;
  /**
   * @type {Element}
   * @private
   */
  this.brailleNode_ = null;
};
goog.addSingletonGetter(cvox.BrailleOverlayWidget);


/**
 * One-time initializer, to be called in a top-level document.  Adds a
 * listener for braille content messages from the background page.
 */
cvox.BrailleOverlayWidget.prototype.init = function() {
  cvox.ExtensionBridge.addMessageListener(goog.bind(
      this.onMessage_, this));
};


/**
 * Sets whether the overlay is active and hides it if it is not active.
 * @param {boolean} value Whether the overlay is active.
 */
cvox.BrailleOverlayWidget.prototype.setActive = function(value) {
  this.active_ = value;
  if (!value) {
    this.hide_();
  }
};


/**
 * @return {boolean} Whether the overlay is active according to prefs.
 */
cvox.BrailleOverlayWidget.prototype.isActive = function() {
  return this.active_;
};


/** @private */
cvox.BrailleOverlayWidget.prototype.show_ = function() {
  var containerNode = this.createContainerNode_();
  this.containerNode_ = containerNode;

  var overlayNode = this.createOverlayNode_();
  containerNode.appendChild(overlayNode);

  this.contentNode_ = document.createElement('div');
  this.brailleNode_ = document.createElement('div');

  overlayNode.appendChild(this.contentNode_);
  overlayNode.appendChild(this.brailleNode_);

  document.body.appendChild(containerNode);

  window.setTimeout(function() {
    containerNode.style['opacity'] = '1.0';
  }, 0);
};


/**
 * Hides the overlay if it is shown.
 * @private
 */
// TODO(plundblad): Call when chromevox is deactivated and on some
// window focus changes.
cvox.BrailleOverlayWidget.prototype.hide_ = function() {
  if (this.containerNode_) {
    var containerNode = this.containerNode_;
    containerNode.style.opacity = '0.0';
    window.setTimeout(function() {
      document.body.removeChild(containerNode);
    }, 1000);
    this.containerNode_ = null;
    this.contentNode_ = null;
    this.brailleNode_ = null;
  }
};


/**
 * @param {string} text The text represnting what was output on the display.
 * @param {string} brailleChars The Unicode characters representing the
 *        braille patterns currently on the display.
 * @private
 */
cvox.BrailleOverlayWidget.prototype.setContent_ = function(text, brailleChars) {
  if (!this.contentNode_) {
    this.show_();
  }
  this.contentNode_.textContent = text;
  this.brailleNode_.textContent = brailleChars;
};


/**
 * Create the container node for the braille overlay.
 *
 * @return {!Element} The new element, not yet added to the document.
 * @private
 */
cvox.BrailleOverlayWidget.prototype.createContainerNode_ = function() {
  var containerNode = document.createElement('div');
  containerNode.id = 'cvox-braille-overlay';
  containerNode.style['position'] = 'fixed';
  containerNode.style['top'] = '50%';
  containerNode.style['left'] = '50%';
  containerNode.style['-webkit-transition'] = 'all 0.3s ease-in';
  containerNode.style['opacity'] = '0.0';
  containerNode.style['z-index'] = '2147483647';
  containerNode.setAttribute('aria-hidden', 'true');
  return containerNode;
};


/**
 * Create the braille overlay.  This should be a child of the node
 * returned from createContainerNode.
 *
 * @return {!Element} The new element, not yet added to the document.
 * @private
 */
cvox.BrailleOverlayWidget.prototype.createOverlayNode_ = function() {
  var overlayNode = document.createElement('div');
  overlayNode.style['position'] = 'fixed';
  overlayNode.style['left'] = '40px';
  overlayNode.style['bottom'] = '20px';
  overlayNode.style['line-height'] = '1.2em';
  overlayNode.style['font-size'] = '20px';
  overlayNode.style['font-family'] = 'monospace';
  overlayNode.style['padding'] = '30px';
  overlayNode.style['min-width'] = '150px';
  overlayNode.style['color'] = '#fff';
  overlayNode.style['background-color'] = 'rgba(0, 0, 0, 0.7)';
  overlayNode.style['border-radius'] = '10px';
  return overlayNode;
};


/**
 * Listens for background page messages and show braille content when it
 * arrives.
 * @param {Object} msg Message from background page.
 * @private
 */
cvox.BrailleOverlayWidget.prototype.onMessage_ = function(msg) {
  if (msg['message'] == 'BRAILLE_CAPTION') {
    this.setContent_(msg['text'], msg['brailleChars']);
  }
};
