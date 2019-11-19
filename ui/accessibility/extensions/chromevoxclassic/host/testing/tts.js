// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dummy implementation of TTS for testing.
 *
 */

goog.provide('cvox.TestTts');

goog.require('cvox.AbstractTts');
goog.require('cvox.DomUtil');
goog.require('cvox.HostFactory');



/**
 * @constructor
 * @extends {cvox.AbstractTts}
 */
cvox.TestTts = function() {
  cvox.AbstractTts.call(this);
  this.utterances_ = [];
};
goog.inherits(cvox.TestTts, cvox.AbstractTts);


/**
 * @type {string}
 * @private
 */
cvox.TestTts.prototype.sentinelText_ = '@@@STOP@@@';


/**
 * @override
 */
cvox.TestTts.prototype.speak = function(text, queueMode, opt_properties) {
  this.utterances_.push(
      {text: text, queueMode: queueMode, properties: opt_properties});
  if (opt_properties && opt_properties['endCallback'] != undefined) {
    var len = this.utterances_.length;
    // 'After' is a sentinel value in the tests that tells TTS to stop and
    // ends callbacks being called.
    if (this.utterances_[len - 1].text != this.sentinelText_) {
      opt_properties['endCallback']();
    }
  }
};


/**
 * Creates a sentinel element that indicates when TTS should stop and callbacks
 * should stop being called.
 * @return {Element} The sentinel element.
 */
cvox.TestTts.prototype.createSentinel = function() {
  var sentinel = document.createElement('div');
  sentinel.textContent = this.sentinelText_;
  return sentinel;
};


/**
 * All calls to tts.speak are saved in an array of utterances.
 * Clear any utterances that were saved up to this point.
 */
cvox.TestTts.prototype.clearUtterances = function() {
  this.utterances_.length = 0;
};

/**
 * Return a string of what was spoken by tts.speak().
 * @return {string} A single string containing all utterances spoken
 *     since initialization or the last call to clearUtterances,
 *     concatenated together with all whitespace collapsed to single
 *     spaces.
 */
cvox.TestTts.prototype.getUtterancesAsString = function() {
  return cvox.DomUtil.collapseWhitespace(this.getUtteranceList().join(' '));
};

/**
 * Processes the utterances spoken the same way the speech queue does,
 * as if they were all generated one after another, with no delay between
 * them, and returns a list of strings that would be output.
 *
 * For example, if two strings were spoken with a queue mode of FLUSH,
 * only the second string will be returned.
 * @return {Array<string>} A list of strings representing the speech output.
 */
cvox.TestTts.prototype.getSpeechQueueOutput = function() {
  var queue = [];
  for (var i = 0; i < this.utterances_.length; i++) {
    var utterance = this.utterances_[i];
    switch (utterance.queueMode) {
      case cvox.AbstractTts.QUEUE_MODE_FLUSH:
        queue = [];
        break;
      case cvox.AbstractTts.QUEUE_MODE_QUEUE:
        break;
      case cvox.AbstractTts.QUEUE_MODE_CATEGORY_FLUSH:
        queue = queue.filter(function(u) {
          return (utterance.properties && utterance.properties.category) &&
              (!u.properties ||
               u.properties.category != utterance.properties.category);
        });
        break;
    }
    queue.push(utterance);
  }

  return queue.map(function(u) {
    return u.text;
  });
};

/**
 * Return a list of strings of what was spoken by tts.speak().
 * @return {Array<string>} A list of all utterances spoken since
 *     initialization or the last call to clearUtterances.
 */
cvox.TestTts.prototype.getUtteranceList = function() {
  var result = [];
  for (var i = 0; i < this.utterances_.length; i++) {
    result.push(this.utterances_[i].text);
  }
  return result;
};

/**
 * Return a list of strings of what was spoken by tts.speak().
 * @return {Array<{text: string, queueMode: cvox.QueueMode}>}
 *     A list of info about all utterances spoken since
 *     initialization or the last call to clearUtterances.
 */
cvox.TestTts.prototype.getUtteranceInfoList = function() {
  return this.utterances_;
};

/** @override */
cvox.HostFactory.ttsConstructor = cvox.TestTts;
