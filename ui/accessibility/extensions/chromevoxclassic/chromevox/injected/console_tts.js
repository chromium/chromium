// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to window.console.
 */

goog.provide('cvox.ConsoleTts');

goog.require('cvox.AbstractTts');
goog.require('cvox.TtsInterface');

/**
 * @constructor
 * @implements {cvox.TtsInterface}
 */
cvox.ConsoleTts = function() {
  /**
   * True if the console TTS is enabled by the user.
   * @type {boolean}
   * @private
   */
  this.enabled_ = false;
};
goog.addSingletonGetter(cvox.ConsoleTts);


/** @override */
cvox.ConsoleTts.prototype.speak = function(textString, queueMode, properties) {
  if (this.enabled_ && window['console']) {
    var logStr = 'Speak';
    if (queueMode == cvox.QueueMode.FLUSH) {
      logStr += ' (I)';
    } else if (queueMode == cvox.QueueMode.CATEGORY_FLUSH) {
      logStr += ' (C)';
    } else {
      logStr += ' (Q)';
    }
    if (properties && properties.category) {
      logStr += ' category=' + properties.category;
    }
    logStr += ' "' + textString + '"';
    console.log(logStr);
  }
  return this;
};

/** @override */
cvox.ConsoleTts.prototype.isSpeaking = function() { return false; };

/** @override */
cvox.ConsoleTts.prototype.stop = function() {
  if (this.enabled_) {
    console.log('Stop');
  }
};

/** @override */
cvox.ConsoleTts.prototype.addCapturingEventListener = function(listener) { };

/** @override */
cvox.ConsoleTts.prototype.increaseOrDecreaseProperty = function() { };

/** @override */
cvox.ConsoleTts.prototype.propertyToPercentage = function() { };

/**
 * Sets the enabled bit.
 * @param {boolean} enabled The new enabled bit.
 */
cvox.ConsoleTts.prototype.setEnabled = function(enabled) {
  this.enabled_ = enabled;
};

/** @override */
cvox.ConsoleTts.prototype.getDefaultProperty = function(property) { };
