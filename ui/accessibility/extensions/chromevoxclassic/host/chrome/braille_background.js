// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */

goog.provide('cvox.BrailleBackground');

goog.require('ChromeVoxState');
goog.require('cvox.BrailleDisplayManager');
goog.require('cvox.BrailleInputHandler');
goog.require('cvox.BrailleInterface');
goog.require('cvox.BrailleKeyEvent');
goog.require('cvox.BrailleTranslatorManager');


/**
 * @constructor
 * @param {cvox.BrailleDisplayManager=} opt_displayManagerForTest
 *        Display manager (for mocking in tests).
 * @param {cvox.BrailleInputHandler=} opt_inputHandlerForTest Input handler
 *        (for mocking in tests).
 * @param {cvox.BrailleTranslatorManager=} opt_translatorManagerForTest
 *        Braille translator manager (for mocking in tests)
 * @implements {cvox.BrailleInterface}
 */
cvox.BrailleBackground = function(opt_displayManagerForTest,
                                  opt_inputHandlerForTest,
                                  opt_translatorManagerForTest) {
  /**
   * @type {!cvox.BrailleTranslatorManager}
   * @private*/
  this.translatorManager_ = opt_translatorManagerForTest ||
      new cvox.BrailleTranslatorManager();
  /**
   * @type {cvox.BrailleDisplayManager}
   * @private
   */
  this.displayManager_ = opt_displayManagerForTest ||
      new cvox.BrailleDisplayManager(this.translatorManager_);
  this.displayManager_.setCommandListener(this.onBrailleKeyEvent_.bind(this));
  /**
   * @type {cvox.NavBraille}
   * @private
   */
  this.lastContent_ = null;
  /**
   * @type {?string}
   * @private
   */
  this.lastContentId_ = null;
  /**
   * @type {!cvox.BrailleInputHandler}
   * @private
   */
  this.inputHandler_ = opt_inputHandlerForTest ||
      new cvox.BrailleInputHandler(this.translatorManager_);
  this.inputHandler_.init();
};


/** @override */
cvox.BrailleBackground.prototype.write = function(params) {
  this.setContent_(params, null);
};


/**
 * @return {cvox.BrailleTranslatorManager} The translator manager used by this
 *     instance.
 */
cvox.BrailleBackground.prototype.getTranslatorManager = function() {
  return this.translatorManager_;
};


/**
 * Called when a Braille message is received from a page content script.
 * @param {Object} msg The Braille message.
 */
cvox.BrailleBackground.prototype.onBrailleMessage = function(msg) {
  if (msg['action'] == 'write') {
    this.setContent_(cvox.NavBraille.fromJson(msg['params']),
                     msg['contentId']);
  }
};


/**
 * @param {!cvox.NavBraille} newContent
 * @param {?string} newContentId
 * @private
 */
cvox.BrailleBackground.prototype.setContent_ = function(
    newContent, newContentId) {
  var updateContent = function() {
    this.lastContent_ = newContentId ? newContent : null;
    this.lastContentId_ = newContentId;
    this.displayManager_.setContent(
        newContent, this.inputHandler_.getExpansionType());
  }.bind(this);
  this.inputHandler_.onDisplayContentChanged(newContent.text, updateContent);
  updateContent();
};


/**
 * Handles braille key events by dispatching either to the input handler,
 * ChromeVox next's background object or ChromeVox classic's content script.
 * @param {!cvox.BrailleKeyEvent} brailleEvt The event.
 * @param {!cvox.NavBraille} content Content of display when event fired.
 * @private
 */
cvox.BrailleBackground.prototype.onBrailleKeyEvent_ = function(
    brailleEvt, content) {
  if (this.inputHandler_.onBrailleKeyEvent(brailleEvt)) {
    return;
  }
  if (ChromeVoxState.instance &&
      ChromeVoxState.instance.onBrailleKeyEvent(brailleEvt, content)) {
    return;
  }
  this.sendCommand_(brailleEvt, content);
};


/**
 * Dispatches braille input commands to the content script.
 * @param {!cvox.BrailleKeyEvent} brailleEvt The event.
 * @param {cvox.NavBraille} content Content of display when event fired.
 * @private
 */
cvox.BrailleBackground.prototype.sendCommand_ =
    function(brailleEvt, content) {
  var msg = {
    'message': 'BRAILLE',
    'args': brailleEvt
  };
  if (content === this.lastContent_) {
    msg.contentId = this.lastContentId_;
  }
  cvox.ExtensionBridge.send(msg);
};
