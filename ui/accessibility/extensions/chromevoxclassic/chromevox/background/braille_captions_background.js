// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview If the braille captions feature is enabled, sends
 * braille content to the Panel on Chrome OS, or a content script on
 * other platforms.
 */

goog.provide('cvox.BrailleCaptionsBackground');

goog.require('PanelCommand');
goog.require('cvox.BrailleDisplayState');
goog.require('cvox.ExtensionBridge');

/**
 * Key set in local storage when this feature is enabled.
 * @const
 */
cvox.BrailleCaptionsBackground.PREF_KEY = 'brailleCaptions';


/**
 * Unicode block of braille pattern characters.  A braille pattern is formed
 * from this value with the low order 8 bits set to the bits representing
 * the dots as per the ISO 11548-1 standard.
 * @const
 */
cvox.BrailleCaptionsBackground.BRAILLE_UNICODE_BLOCK_START = 0x2800;


/**
 * Called once to initialize the class.
 * @param {function()} stateCallback Called when the state of the captions
 *     feature changes.
 */
cvox.BrailleCaptionsBackground.init = function(stateCallback) {
  var self = cvox.BrailleCaptionsBackground;
  /**
   * @type {function()}
   * @private
   */
  self.stateCallback_ = stateCallback;
};


/**
 * Returns whether the braille captions feature is enabled.
 * @return {boolean}
 */
cvox.BrailleCaptionsBackground.isEnabled = function() {
  var self = cvox.BrailleCaptionsBackground;
  return localStorage[self.PREF_KEY] === String(true);
};


/**
 * @param {string} text Text of the shown braille.
 * @param {ArrayBuffer} cells Braille cells shown on the display.
 */
cvox.BrailleCaptionsBackground.setContent = function(text, cells) {
  var self = cvox.BrailleCaptionsBackground;
  // Convert the cells to Unicode braille pattern characters.
  var byteBuf = new Uint8Array(cells);
  var brailleChars = '';
  for (var i = 0; i < byteBuf.length; ++i) {
    brailleChars += String.fromCharCode(
        self.BRAILLE_UNICODE_BLOCK_START | byteBuf[i]);
  }

  if (cvox.ChromeVox.isChromeOS) {
    var data = {text: text, braille: brailleChars};
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
  } else {
    cvox.ExtensionBridge.send({
      message: 'BRAILLE_CAPTION',
      text: text,
      brailleChars: brailleChars
    });
  }
};


/**
 * Sets whether the overlay should be active.
 * @param {boolean} newValue The new value of the active flag.
 */
cvox.BrailleCaptionsBackground.setActive = function(newValue) {
  var self = cvox.BrailleCaptionsBackground;
  var oldValue = self.isEnabled();
  window['prefs'].setPref(self.PREF_KEY, String(newValue));
  if (oldValue != newValue) {
    if (self.stateCallback_) {
      self.stateCallback_();
    }
    var msg = newValue ?
        Msgs.getMsg('braille_captions_enabled') :
        Msgs.getMsg('braille_captions_disabled');
    cvox.ChromeVox.tts.speak(msg, cvox.QueueMode.QUEUE);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(msg));
  }
};


/**
 * Returns a display state representing the state of the captions feature.
 * This is used when no actual hardware display is connected.
 * @return {cvox.BrailleDisplayState}
 */
cvox.BrailleCaptionsBackground.getVirtualDisplayState = function() {
  var self = cvox.BrailleCaptionsBackground;
  if (self.isEnabled()) {
    return {available: true, textCellCount: 40};  // 40, why not?
  } else {
    return {available: false};
  }
};
