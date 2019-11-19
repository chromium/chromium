// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple container object for the description of a
 * navigation from one object to another.
 *
 */


goog.provide('cvox.NavDescription');

goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.CursorSelection');
goog.require('cvox.QueueMode');

/**
 * A class representing the description of navigation from one object to
 * another.
 * @param {{context: (undefined|string),
 *          text: (string),
 *          userValue: (undefined|string),
 *          annotation: (undefined|string),
 *          earcons: (undefined|Array<cvox.Earcon>),
 *          personality: (undefined|Object),
 *          hint: (undefined|string),
            category: (undefined|string)}} kwargs The arguments for this
 *  description.
 *  context The context, for example descriptions of objects
 *     that were crossed into, like "Toolbar" or "Menu Bar" or "List with
 *     5 items". This is all spoken with an annotation voice.
 *  text The text of the object itself, including text from
 *     titles, labels, etc.
 *  userValue The text that the user has entered.
 *  annotation The role and state of the object.
 *  earcons A list of the earcon ids to play along
 *     with the spoken description of this object.
 *  personality Optional TTS personality to use for the text.
 *  hint Optional Hint text (.e.g. aria-describedby).
 *  category Optional category (for speech queueing behavior).
 * @constructor
 */
cvox.NavDescription = function(kwargs) {
  this.context = kwargs.context ? kwargs.context : '';
  this.text = kwargs.text ? kwargs.text : '';
  this.userValue = kwargs.userValue ? kwargs.userValue : '';
  this.annotation = kwargs.annotation ? kwargs.annotation : '';
  this.earcons = kwargs.earcons ? kwargs.earcons : [];
  this.personality = kwargs.personality;
  this.hint = kwargs.hint ? kwargs.hint : '';
  this.category = kwargs.category ? kwargs.category : null;
};


/**
 * @return {boolean} true if this description is empty.
 */
cvox.NavDescription.prototype.isEmpty = function() {
  return (this.context.length == 0 &&
          this.earcons.length == 0 &&
          this.text.length == 0 &&
          this.userValue.length == 0 &&
          this.annotation.length == 0);
};


/**
 * @return {string} A string representation of this object.
 */
cvox.NavDescription.prototype.toString = function() {
  return 'NavDescription(context="' + this.context + '" ' +
         ' text="' + this.text + '" ' +
         ' userValue="' + this.userValue + '" ' +
         ' annotation="' + this.annotation +
         (this.category ? '" category="' + this.category + '")' : '') +
         '")';
};


/**
 * Modifies the earcon to play along with the spoken description of the object.
 * @param {cvox.Earcon} earconId An earcon id to be pushed on to the list of
 * earcon ids to play along with the spoken description of this object.
 */
cvox.NavDescription.prototype.pushEarcon = function(earconId) {
  this.earcons.push(earconId);
};


/**
 * Speak this nav description with the given queue mode.
 * @param {cvox.QueueMode=} queueMode The queue mode.
 * @param {function()=} opt_startCallback Function called when this
 *     starts speaking.
 * @param {function()=} opt_endCallback Function called when this ends speaking.
 */
cvox.NavDescription.prototype.speak = function(
    queueMode, opt_startCallback, opt_endCallback) {
  /**
   * Return a deep copy of PERSONALITY_ANNOTATION for modifying.
   * @return {Object} The newly created properties object.
   */
  function makeAnnotationProps() {
    var properties = {};
    var src = cvox.AbstractTts.PERSONALITY_ANNOTATION;
    for (var key in src) {
      properties[key] = src[key];
    }
    return properties;
  }

  var speakArgs = new Array();
  if (this.context) {
    speakArgs.push([this.context, queueMode, makeAnnotationProps()]);
    queueMode = cvox.QueueMode.QUEUE;
  }

  speakArgs.push([this.text,
                  queueMode,
                  this.personality ? this.personality : {}]);
  queueMode = cvox.QueueMode.QUEUE;

  if (this.userValue) {
    speakArgs.push([this.userValue, queueMode, {}]);
  }

  if (this.annotation) {
    speakArgs.push([this.annotation, queueMode, makeAnnotationProps()]);
  }

  if (this.hint) {
    speakArgs.push([this.hint, queueMode, makeAnnotationProps()]);
  }

  var length = speakArgs.length;
  for (var i = 0; i < length; i++) {
    if (i == 0 && opt_startCallback) {
      speakArgs[i][2]['startCallback'] = opt_startCallback;
    }
    if (i == length - 1 && opt_endCallback) {
      speakArgs[i][2]['endCallback'] = opt_endCallback;
    }
    if (this.category) {
      speakArgs[i][2]['category'] = this.category;
    }
    cvox.ChromeVox.tts.speak.apply(cvox.ChromeVox.tts, speakArgs[i]);
  }
};


/**
 * Compares two NavDescriptions.
 * @param {cvox.NavDescription} that A NavDescription.
 * @return {boolean} True if equal.
 */
cvox.NavDescription.prototype.equals = function(that) {
  return this.context == that.context &&
      this.text == that.text &&
      this.userValue == that.userValue &&
      this.annotation == that.annotation;
};
