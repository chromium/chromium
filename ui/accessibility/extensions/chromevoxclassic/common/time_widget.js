// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxHTMLTimeWidget');

/**
 * @fileoverview Gives the user spoken feedback as they interact with the time
 * widget (input type=time).
 *
 */

/**
 * A class containing the information needed to speak
 * a text change event to the user.
 *
 * @constructor
 * @param {Element} timeElem The time widget element.
 * @param {cvox.TtsInterface} tts The TTS object from ChromeVox.
 */
cvox.ChromeVoxHTMLTimeWidget = function(timeElem, tts) {
  var self = this;
  this.timeElem_ = timeElem;
  this.timeTts_ = tts;
  this.pHours_ = -1;
  this.pMinutes_ = -1;
  this.pSeconds_ = 0;
  this.pMilliseconds_ = 0;
  this.pAmpm_ = '';
  this.pos_ = 0;
  this.maxPos_ = 2;
  this.keyListener_ = function(evt) {
    self.eventHandler_(evt);
  };
  this.blurListener_ = function(evt) {
    self.shutdown();
  };
  if (this.timeElem_.hasAttribute('step')) {
    var step = this.timeElem_.getAttribute('step');
    if (step > 0) { // 0 or invalid values show hh:mm AM/PM
      if (step >= 1) {
        this.maxPos_ = 3; // Anything larger than 1 shows hh:mm:ss AM/PM
      } else {
        this.maxPos_ = 4; // Anything less than 1 shows hh:mm:ss.ms AM/PM
      }
    }
  }

  // Ensure we have a reasonable value to start with.
  if (this.timeElem_.value.length == 0) {
    this.forceInitTime_();
  }

  // Move the cursor to the first position so that we are guaranteed to start
  // off at the hours position.
  for (var i = 0; i < this.maxPos_; i++) {
    var evt = document.createEvent('KeyboardEvent');
    evt.initKeyboardEvent(
          'keydown', true, true, window, 'Left', 0, false, false, false, false);
    this.timeElem_.dispatchEvent(evt);
    evt = document.createEvent('KeyboardEvent');
    evt.initKeyboardEvent(
          'keyup', true, true, window, 'Left', 0, false, false, false, false);
    this.timeElem_.dispatchEvent(evt);
  }

  this.timeElem_.addEventListener('keydown', this.keyListener_, false);
  this.timeElem_.addEventListener('keyup', this.keyListener_, false);
  this.timeElem_.addEventListener('blur', this.blurListener_, false);
  this.update_(true);
};

/**
 * Removes the key listeners for the time widget.
 *
 */
cvox.ChromeVoxHTMLTimeWidget.prototype.shutdown = function() {
  this.timeElem_.removeEventListener('blur', this.blurListener_, false);
  this.timeElem_.removeEventListener('keydown', this.keyListener_, false);
  this.timeElem_.removeEventListener('keyup', this.keyListener_, false);
};

/**
 * Initialize to midnight.
 * @private
 */
cvox.ChromeVoxHTMLTimeWidget.prototype.forceInitTime_ = function() {
  this.timeElem_.setAttribute('value', '12:00');
};

/**
 * Called when the position changes.
 * @private
 */
cvox.ChromeVoxHTMLTimeWidget.prototype.handlePosChange_ = function() {
  if (this.pos_ < 0) {
    this.pos_ = 0;
  }
  if (this.pos_ > this.maxPos_) {
    this.pos_ = this.maxPos_;
  }
  // Reset the cached state of the new field so that the field will be spoken
  // in the update.
  if (this.pos_ == this.maxPos_) {
    this.pAmpm_ = '';
    return;
  }
  switch (this.pos_) {
    case 0:
      this.pHours_ = -1;
      break;
    case 1:
      this.pMinutes_ = -1;
      break;
    case 2:
      this.pSeconds_ = -1;
      break;
    case 3:
      this.pMilliseconds_ = -1;
      break;
  }
};

/**
 * @param {boolean} shouldSpeakLabel True if the label should be spoken.
 * @private
 */
cvox.ChromeVoxHTMLTimeWidget.prototype.update_ = function(shouldSpeakLabel) {
  var splitTime = this.timeElem_.value.split(':');
  if (splitTime.length < 1) {
    this.forceInitTime_();
    return;
  }

  var hours = splitTime[0];
  var minutes = -1;
  var seconds = 0;
  var milliseconds = 0;
  var ampm = Msgs.getMsg('timewidget_am');
  if (splitTime.length > 1) {
    minutes = splitTime[1];
  }
  if (splitTime.length > 2) {
    var splitSecondsAndMilliseconds = splitTime[2].split('.');
    seconds = splitSecondsAndMilliseconds[0];
    if (splitSecondsAndMilliseconds.length > 1) {
      milliseconds = splitSecondsAndMilliseconds[1];
    }
  }
  if (hours > 12) {
    hours = hours - 12;
    ampm = Msgs.getMsg('timewidget_pm');
  }
  if (hours == 12) {
    ampm = Msgs.getMsg('timewidget_pm');
  }
  if (hours == 0) {
    hours = 12;
    ampm = Msgs.getMsg('timewidget_am');
  }

  var changeMessage = '';

  if (shouldSpeakLabel) {
    changeMessage = cvox.DomUtil.getName(this.timeElem_, true, true) + '\n';
  }

  if (hours != this.pHours_) {
    changeMessage = changeMessage + hours + ' ' +
        Msgs.getMsg('timewidget_hours') + '\n';
    this.pHours_ = hours;
  }

  if (minutes != this.pMinutes_) {
    changeMessage = changeMessage + minutes + ' ' +
        Msgs.getMsg('timewidget_minutes') + '\n';
    this.pMinutes_ = minutes;
  }

  if (seconds != this.pSeconds_) {
    changeMessage = changeMessage + seconds + ' ' +
        Msgs.getMsg('timewidget_seconds') + '\n';
    this.pSeconds_ = seconds;
  }

  if (milliseconds != this.pMilliseconds_) {
    changeMessage = changeMessage + milliseconds + ' ' +
        Msgs.getMsg('timewidget_milliseconds') + '\n';
    this.pMilliseconds_ = milliseconds;
  }

  if (ampm != this.pAmpm_) {
    changeMessage = changeMessage + ampm;
    this.pAmpm_ = ampm;
  }

  if (changeMessage.length > 0) {
    this.timeTts_.speak(changeMessage, cvox.QueueMode.FLUSH, null);
  }
};

/**
 * @param {Object} evt The event to handle.
 * @private
 */
cvox.ChromeVoxHTMLTimeWidget.prototype.eventHandler_ = function(evt) {
  var shouldSpeakLabel = false;
  if (evt.type == 'keydown') {
    if (((evt.keyCode == 9) && !evt.shiftKey) || (evt.keyCode == 39)) {
      this.pos_++;
      this.handlePosChange_();
      shouldSpeakLabel = true;
    }
    if (((evt.keyCode == 9) && evt.shiftKey) || (evt.keyCode == 37)) {
      this.pos_--;
      this.handlePosChange_();
      shouldSpeakLabel = true;
    }
  }
  this.update_(shouldSpeakLabel);
};
