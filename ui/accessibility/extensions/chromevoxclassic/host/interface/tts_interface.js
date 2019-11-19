// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Tts interface.
 *
 * All TTS engines in ChromeVox conform to the this interface.
 *
 */

goog.provide('cvox.QueueMode');
goog.provide('cvox.TtsCapturingEventListener');
goog.provide('cvox.TtsCategory');
goog.provide('cvox.TtsInterface');

/**
 * Categories for a speech utterance. This can be used with the
 * CATEGORY_FLUSH queue mode, which flushes all utterances from a given
 * category but not other utterances.
 *
 * NAV: speech related to explicit navigation, or focus changing.
 * LIVE: speech coming from changes to live regions.
 *
 * @enum {string}
 */
cvox.TtsCategory = {
  LIVE: 'live',
  NAV: 'nav'
};

/**
 * Queue modes for calls to {@code cvox.TtsInterface.speak}.
 * @enum
 */
cvox.QueueMode = {
  /** Stop speech, clear everything, then speak this utterance. */
  FLUSH: 0,

  /** Append this utterance to the end of the queue. */
  QUEUE: 1,

  /**
   * Clear any utterances of the same category (as set by
   * properties['category']) from the queue, then enqueue this utterance.
   */
  CATEGORY_FLUSH: 2
};

/**
 * @interface
 * An interface for clients who want to get notified when an utterance
 * starts or ends from any source.
 */
cvox.TtsCapturingEventListener = function() { };

/**
 * Called when any utterance starts.
 */
cvox.TtsCapturingEventListener.prototype.onTtsStart = function() { };

/**
 * Called when any utterance ends.
 */
cvox.TtsCapturingEventListener.prototype.onTtsEnd = function() { };


/**
 * @interface
 */
cvox.TtsInterface = function() { };

/**
 * Speaks the given string using the specified queueMode and properties.
 * @param {string} textString The string of text to be spoken.
 * @param {cvox.QueueMode} queueMode The queue mode to use for speaking.
 * @param {Object=} properties Speech properties to use for this utterance.
 * @return {cvox.TtsInterface} A tts object useful for chaining speak calls.
 */
cvox.TtsInterface.prototype.speak =
    function(textString, queueMode, properties) { };


/**
 * Returns true if the TTS is currently speaking.
 * @return {boolean} True if the TTS is speaking.
 */
cvox.TtsInterface.prototype.isSpeaking = function() { };


/**
 * Stops speech.
 */
cvox.TtsInterface.prototype.stop = function() { };

/**
 * Adds a listener to get called whenever any utterance starts or ends.
 * @param {cvox.TtsCapturingEventListener} listener Listener to get called.
 */
cvox.TtsInterface.prototype.addCapturingEventListener = function(listener) { };

/**
 * Increases a TTS speech property.
 * @param {string} propertyName The name of the property to change.
 * @param {boolean} increase If true, increases the property value by one
 *     step size, otherwise decreases.
 */
cvox.TtsInterface.prototype.increaseOrDecreaseProperty =
    function(propertyName, increase) { };


/**
 * Converts an engine property value to a percentage from 0.00 to 1.00.
 * @param {string} property The property to convert.
 * @return {?number} The percentage of the property.
 */
cvox.TtsInterface.prototype.propertyToPercentage = function(property) { };


/**
 * Returns the default properties of the first tts that has default properties.
 * @param {string} property Name of property.
 * @return {?number} The default value.
 */
cvox.TtsInterface.prototype.getDefaultProperty = function(property) { };
