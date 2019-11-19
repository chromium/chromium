// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A composite TTS sends allows ChromeVox to use
 * multiple TTS engines at the same time.
 *
 */

goog.provide('cvox.CompositeTts');

goog.require('cvox.TtsInterface');

/**
 * A Composite Tts
 * @constructor
 * @implements {cvox.TtsInterface}
 */
cvox.CompositeTts = function() {
  /**
   * @type {Array<cvox.TtsInterface>}
   * @private
   */
  this.ttsEngines_ = [];
};


/**
 * Adds a TTS engine to the composite TTS
 * @param {cvox.TtsInterface} tts The TTS to add.
 * @return {cvox.CompositeTts} this.
 */
cvox.CompositeTts.prototype.add = function(tts) {
  this.ttsEngines_.push(tts);
  return this;
};


/**
 * @override
 */
cvox.CompositeTts.prototype.speak =
    function(textString, queueMode, properties) {
  this.ttsEngines_.forEach(function(engine) {
    engine.speak(textString, queueMode, properties);
  });
  return this;
};


/**
 * Returns true if any of the TTSes are still speaking.
 * @override
 */
cvox.CompositeTts.prototype.isSpeaking = function() {
  return this.ttsEngines_.some(function(engine) {
    return engine.isSpeaking();
  });
};


/**
 * @override
 */
cvox.CompositeTts.prototype.stop = function() {
  this.ttsEngines_.forEach(function(engine) {
    engine.stop();
  });
};


/**
 * @override
 */
cvox.CompositeTts.prototype.addCapturingEventListener = function(listener) {
  this.ttsEngines_.forEach(function(engine) {
    engine.addCapturingEventListener(listener);
  });
};


/**
 * @override
 */
cvox.CompositeTts.prototype.increaseOrDecreaseProperty =
    function(propertyName, increase) {
  this.ttsEngines_.forEach(function(engine) {
    engine.increaseOrDecreaseProperty(propertyName, increase);
  });
};


/**
 * @override
 */
cvox.CompositeTts.prototype.propertyToPercentage = function(property) {
  for (var i = 0, engine; engine = this.ttsEngines_[i]; i++) {
    var value = engine.propertyToPercentage(property);
    if (value !== undefined)
      return value;
  }
  return null;
};


/**
 * @override
 */
cvox.CompositeTts.prototype.getDefaultProperty = function(property) {
  for (var i = 0, engine; engine = this.ttsEngines_[i]; i++) {
    var value = engine.getDefaultProperty(property);
    if (value !== undefined)
      return value;
  }
  return null;
};
