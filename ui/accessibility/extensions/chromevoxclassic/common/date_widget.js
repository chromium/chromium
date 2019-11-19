// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxHTMLDateWidget');

goog.require('Msgs');

/**
 * @fileoverview Gives the user spoken feedback as they interact with the date
 * widget (input type=date).
 *
 */

/**
 * A class containing the information needed to speak
 * a text change event to the user.
 *
 * @constructor
 * @param {Element} dateElem The time widget element.
 * @param {cvox.TtsInterface} tts The TTS object from ChromeVox.
 */
cvox.ChromeVoxHTMLDateWidget = function(dateElem, tts) {
  var self = this;
  /**
   * Currently selected field in the widget.
   * @type {number}
   * @private
   */
  this.pos_ = 0;
  var maxpos = 2;
  if (dateElem.type == 'month' || dateElem.type == 'week') {
    maxpos = 1;
  }
  /**
   * The maximum number of fields in the widget.
   * @type {number}
   * @private
   */
  this.maxPos_ = maxpos;
  /**
   * The HTML node of the widget.
   * @type {Node}
   * @private
   */
  this.dateElem_ = dateElem;
  /**
   * A handle to the ChromeVox TTS object.
   * @type {Object}
   * @private
   */
  this.dateTts_ = tts;
  /**
   * The previous value of the year field.
   * @type {number}
   * @private
   */
  this.pYear_ = -1;
  /**
   * The previous value of the month field.
   * @type {number}
   * @private
   */
  this.pMonth_ = -1;
  /**
   * The previous value of the week field.
   * @type {number}
   * @private
   */
  this.pWeek_ = -1;
  /**
   * The previous value of the day field.
   * @type {number}
   * @private
   */
  this.pDay_ = -1;

  // Use listeners to make this work when running tests inside of ChromeVox.
  this.keyListener_ = function(evt) {
    self.eventHandler_(evt);
  };
  this.blurListener_ = function(evt) {
    self.shutdown();
  };

  // Ensure we have a reasonable value to start with.
  if (this.dateElem_.value.length == 0) {
    this.forceInitTime_();
  }

  // Move the cursor to the first position so that we are guaranteed to start
  // off at the hours position.
  for (var i = 0; i < this.maxPos_; i++) {
    var evt = document.createEvent('KeyboardEvent');
    evt.initKeyboardEvent(
          'keydown', true, true, window, 'Left', 0, false, false, false, false);
    this.dateElem_.dispatchEvent(evt);
    evt = document.createEvent('KeyboardEvent');
    evt.initKeyboardEvent(
          'keyup', true, true, window, 'Left', 0, false, false, false, false);
    this.dateElem_.dispatchEvent(evt);
  }

  this.dateElem_.addEventListener('keydown', this.keyListener_, false);
  this.dateElem_.addEventListener('keyup', this.keyListener_, false);
  this.dateElem_.addEventListener('blur', this.blurListener_, false);
  this.update_(true);
};

/**
 * Removes the key listeners for the time widget.
 *
 */
cvox.ChromeVoxHTMLDateWidget.prototype.shutdown = function() {
  this.dateElem_.removeEventListener('blur', this.blurListener_, false);
  this.dateElem_.removeEventListener('keydown', this.keyListener_, false);
  this.dateElem_.removeEventListener('keyup', this.keyListener_, false);
};

/**
 * Forces a sensible default value so that there is something there that can
 * be inspected with JS.
 * @private
 */
cvox.ChromeVoxHTMLDateWidget.prototype.forceInitTime_ = function() {
  var currentDate = new Date();
  var valueString = '';
  var yearString = currentDate.getFullYear() + '';
  // Date.getMonth starts at 0, but the value for the HTML5 date widget needs to
  // start at 1.
  var monthString = currentDate.getMonth() + 1 + '';
  if (monthString.length < 2) {
    monthString = '0' + monthString; // Month format is MM.
  }
  var dayString = currentDate.getDate() + '';

  switch (this.dateElem_.type) {
    case 'month':
      valueString = yearString + '-' + monthString;
      break;
    case 'week':
      // Based on info from: http://www.merlyn.demon.co.uk/weekcalc.htm#WNR
      currentDate.setHours(0, 0, 0);
      // Set to nearest Thursday: current date + 4 - current day number
      // Make Sunday's day number 7
      currentDate.setDate(
          currentDate.getDate() + 4 - (currentDate.getDay() || 7));
      // Get first day of year
      var yearStart = new Date(currentDate.getFullYear(), 0, 1);
      // Calculate full weeks to nearest Thursday
      var weekString =
          Math.ceil((((currentDate - yearStart) / 86400000) + 1) / 7) + '';
      if (weekString.length < 2) {
        weekString = '0' + weekString; // Week format is WXX.
      }
      weekString = 'W' + weekString;
      valueString = yearString + '-' + weekString;
      break;
    default:
      valueString = yearString + '-' + monthString + '-' + dayString;
      break;
  }
  this.dateElem_.setAttribute('value', valueString);
};

/**
 * Ensure that the position stays within bounds.
 * @private
 */
cvox.ChromeVoxHTMLDateWidget.prototype.handlePosChange_ = function() {
  this.pos_ = Math.max(this.pos_, 0);
  this.pos_ = Math.min(this.pos_, this.maxPos_);
  // TODO (clchen, dtseng): Make this logic i18n once there is a way to
  // determine what the date format actually is. For now, assume that:
  // date  == mm/dd/yyyy
  // week  == ww/yyyy
  // month == mm/yyyy.
  switch (this.pos_) {
    case 0:
      if (this.dateElem_.type == 'week') {
        this.pWeek_ = -1;
      } else {
        this.pMonth_ = -1;
      }
      break;
    case 1:
      if (this.dateElem_.type == 'date') {
        this.pDay_ = -1;
      } else {
        this.pYear_ = -1;
      }
      break;
    case 2:
      this.pYear_ = -1;
      break;
  }
};

/**
 * Speaks any changes to the control.
 * @private
 * @param {boolean} shouldSpeakLabel Whether or not to speak the label.
 */
cvox.ChromeVoxHTMLDateWidget.prototype.update_ = function(shouldSpeakLabel) {
  var splitDate = this.dateElem_.value.split('-');
  if (splitDate.length < 1) {
    this.forceInitTime_();
    return;
  }

  var year = -1;
  var month = -1;
  var week = -1;
  var day = -1;

  year = parseInt(splitDate[0], 10);

  if (this.dateElem_.type == 'week') {
    week = parseInt(splitDate[1].replace('W', ''), 10);
  } else if (this.dateElem_.type == 'date') {
    month = parseInt(splitDate[1], 10);
    day = parseInt(splitDate[2], 10);
  } else {
    month = parseInt(splitDate[1], 10);
  }

  var changeMessage = '';

  if (shouldSpeakLabel) {
    changeMessage = cvox.DomUtil.getName(this.dateElem_, true, true) + '\n';
  }

  if (week != this.pWeek_) {
    changeMessage = changeMessage +
        Msgs.getMsg('datewidget_week') + week + '\n';
    this.pWeek_ = week;
  }

  if (month != this.pMonth_) {
    var monthName = '';
    switch (month) {
      case 1:
        monthName = Msgs.getMsg('datewidget_january');
        break;
      case 2:
        monthName = Msgs.getMsg('datewidget_february');
        break;
      case 3:
        monthName = Msgs.getMsg('datewidget_march');
        break;
      case 4:
        monthName = Msgs.getMsg('datewidget_april');
        break;
      case 5:
        monthName = Msgs.getMsg('datewidget_may');
        break;
      case 6:
        monthName = Msgs.getMsg('datewidget_june');
        break;
      case 7:
        monthName = Msgs.getMsg('datewidget_july');
        break;
      case 8:
        monthName = Msgs.getMsg('datewidget_august');
        break;
      case 9:
        monthName = Msgs.getMsg('datewidget_september');
        break;
      case 10:
        monthName = Msgs.getMsg('datewidget_october');
        break;
      case 11:
        monthName = Msgs.getMsg('datewidget_november');
        break;
      case 12:
        monthName = Msgs.getMsg('datewidget_december');
        break;
    }
    changeMessage = changeMessage + monthName + '\n';
    this.pMonth_ = month;
  }

  if (day != this.pDay_) {
    changeMessage = changeMessage + day + '\n';
    this.pDay_ = day;
  }

  if (year != this.pYear_) {
    changeMessage = changeMessage + year + '\n';
    this.pYear_ = year;
  }

  if (changeMessage.length > 0) {
    this.dateTts_.speak(changeMessage, 0, null);
  }
};

/**
 * Handles user key events.
 * @private
 * @param {Event} evt The event to be handled.
 */
cvox.ChromeVoxHTMLDateWidget.prototype.eventHandler_ = function(evt) {
  var shouldSpeakLabel = false;
  if (evt.type == 'keydown') {
    // Handle tab/right arrow
    if (((evt.keyCode == 9) && !evt.shiftKey) || (evt.keyCode == 39)) {
      this.pos_++;
      this.handlePosChange_();
      shouldSpeakLabel = true;
    }
    // Handle shift+tab/left arrow
    if (((evt.keyCode == 9) && evt.shiftKey) || (evt.keyCode == 37)) {
      this.pos_--;
      this.handlePosChange_();
      shouldSpeakLabel = true;
    }
    // For all other cases, fall through and let update_ decide if there are any
    // changes that need to be spoken.
  }
  this.update_(shouldSpeakLabel);
};
