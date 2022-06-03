/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Input Date Picker implementation.  Pairs a
 * goog.ui.PopupDatePicker with an input element and handles the input from
 * either.
 *
 * @see ../demos/inputdatepicker.html
 */

goog.provide('goog.ui.InputDatePicker');

goog.require('goog.date.DateTime');
goog.require('goog.dom');
goog.require('goog.dom.InputType');
goog.require('goog.dom.TagName');
goog.require('goog.i18n.DateTimeParse');
goog.require('goog.string');
goog.require('goog.ui.Component');
goog.require('goog.ui.DatePicker');
/** @suppress {extraRequire} */
goog.require('goog.ui.LabelInput');
goog.require('goog.ui.PopupBase');
goog.require('goog.ui.PopupDatePicker');
goog.requireType('goog.date.Date');
goog.requireType('goog.date.DateLike');
goog.requireType('goog.events.Event');
goog.requireType('goog.ui.DatePickerEvent');



/**
 * Input date picker widget.
 *
 * @param {!goog.ui.InputDatePicker.DateFormatter} dateTimeFormatter A formatter
 *     instance used to format the date picker's date for display in the input
 *     element.
 * @param {!goog.ui.InputDatePicker.DateParser} dateTimeParser A parser instance
 *     used to parse the input element's string as a date to set the picker.
 * @param {goog.ui.DatePicker=} opt_datePicker Optional DatePicker.  This
 *     enables the use of a custom date-picker instance.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper.
 * @extends {goog.ui.Component}
 * @constructor
 */
goog.ui.InputDatePicker = function(
    dateTimeFormatter, dateTimeParser, opt_datePicker, opt_domHelper) {
  'use strict';
  goog.ui.Component.call(this, opt_domHelper);

  this.dateTimeFormatter_ = dateTimeFormatter;
  this.dateTimeParser_ = dateTimeParser;

  this.popupDatePicker_ =
      new goog.ui.PopupDatePicker(opt_datePicker, opt_domHelper);
  this.addChild(this.popupDatePicker_);
  this.popupDatePicker_.setAllowAutoFocus(false);
};
goog.inherits(goog.ui.InputDatePicker, goog.ui.Component);


/**
 * Used to format the date picker's date for display in the input element.
 * @type {?goog.ui.InputDatePicker.DateFormatter}
 * @private
 */
goog.ui.InputDatePicker.prototype.dateTimeFormatter_ = null;


/**
 * Used to parse the input element's string as a date to set the picker.
 * @type {?goog.ui.InputDatePicker.DateParser}
 * @private
 */
goog.ui.InputDatePicker.prototype.dateTimeParser_ = null;


/**
 * The instance of goog.ui.PopupDatePicker used to pop up and select the date.
 * @type {?goog.ui.PopupDatePicker}
 * @private
 */
goog.ui.InputDatePicker.prototype.popupDatePicker_ = null;


/**
 * The element that the PopupDatePicker should be parented to. Defaults to the
 * body element of the page.
 * @type {?Element}
 * @private
 */
goog.ui.InputDatePicker.prototype.popupParentElement_ = null;


/**
 * Returns the PopupDatePicker's internal DatePicker instance.  This can be
 * used to customize the date picker's styling.
 *
 * @return {goog.ui.DatePicker} The internal DatePicker instance.
 */
goog.ui.InputDatePicker.prototype.getDatePicker = function() {
  'use strict';
  return this.popupDatePicker_.getDatePicker();
};


/**
 * Returns the PopupDatePicker instance.
 *
 * @return {goog.ui.PopupDatePicker} Popup instance.
 */
goog.ui.InputDatePicker.prototype.getPopupDatePicker = function() {
  'use strict';
  return this.popupDatePicker_;
};


/**
 * Returns the selected date, if any.  Compares the dates from the date picker
 * and the input field, causing them to be synced if different.
 * @return {goog.date.DateTime} The selected date, if any.
 */
goog.ui.InputDatePicker.prototype.getDate = function() {
  'use strict';
  // The user expectation is that the date be whatever the input shows.
  // This method biases towards the input value to conform to that expectation.

  var inputDate = this.getInputValueAsDate_();
  var pickerDate = this.popupDatePicker_.getDate();

  if (inputDate && pickerDate) {
    if (!inputDate.equals(pickerDate)) {
      this.popupDatePicker_.setDate(inputDate);
    }
  } else {
    this.popupDatePicker_.setDate(null);
  }

  return inputDate;
};


/**
 * Sets the selected date.  See goog.ui.PopupDatePicker.setDate().
 * @param {goog.date.Date} date The date to set.
 */
goog.ui.InputDatePicker.prototype.setDate = function(date) {
  'use strict';
  this.popupDatePicker_.setDate(date);
};


/**
 * Sets the value of the input element.  This can be overridden to support
 * alternative types of input setting.
 * @param {string} value The value to set.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.InputDatePicker.prototype.setInputValue = function(value) {
  'use strict';
  var el = this.getElement();
  if (el.labelInput_) {
    var labelInput = /** @type {goog.ui.LabelInput} */ (el.labelInput_);
    labelInput.setValue(value);
  } else {
    el.value = value;
  }
};


/**
 * Returns the value of the input element.  This can be overridden to support
 * alternative types of input getting.
 * @return {string} The input value.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.InputDatePicker.prototype.getInputValue = function() {
  'use strict';
  var el = this.getElement();
  if (el.labelInput_) {
    var labelInput = /** @type {goog.ui.LabelInput} */ (el.labelInput_);
    return labelInput.getValue();
  } else {
    return el.value;
  }
};


/**
 * Sets the value of the input element from date object.
 *
 * @param {?goog.date.Date} date The value to set.
 * @private
 */
goog.ui.InputDatePicker.prototype.setInputValueAsDate_ = function(date) {
  'use strict';
  this.setInputValue(date ? this.dateTimeFormatter_.format(date) : '');
};


/**
 * Gets the input element value and attempts to parse it as a date.
 *
 * @return {goog.date.DateTime} The date object is returned if the parse
 *      is successful, null is returned on failure.
 * @private
 */
goog.ui.InputDatePicker.prototype.getInputValueAsDate_ = function() {
  'use strict';
  var value = goog.string.trim(this.getInputValue());
  if (value) {
    var date = new goog.date.DateTime();
    // DateTime needed as parse assumes it can call getHours(), getMinutes(),
    // etc, on the date if hours and minutes aren't defined.
    if (this.dateTimeParser_.parse(value, date, {validate: true}) > 0) {
      // Parser with YYYY format string will interpret 1 as year 1 A.D.
      // However, datepicker.setDate() method will change it into 1901.
      // Same is true for any other pattern when number entered by user is
      // different from number of digits in the pattern. (YY and 1 will be 1AD).
      // See i18n/datetimeparse.js
      // Conversion happens in goog.date.Date/DateTime constructor
      // when it calls new Date(year...). See ui/datepicker.js.
      return date;
    }
  }

  return null;
};


/**
 * Creates an input element for use with the popup date picker.
 * @override
 */
goog.ui.InputDatePicker.prototype.createDom = function() {
  'use strict';
  this.setElementInternal(this.getDomHelper().createDom(
      goog.dom.TagName.INPUT, {'type': goog.dom.InputType.TEXT}));
  this.popupDatePicker_.createDom();
};


/**
 * Sets the element that the PopupDatePicker should be parented to. If not set,
 * defaults to the body element of the page.
 * @param {Element} el The element that the PopupDatePicker should be parented
 *     to.
 */
goog.ui.InputDatePicker.prototype.setPopupParentElement = function(el) {
  'use strict';
  this.popupParentElement_ = el;
};


/** @override */
goog.ui.InputDatePicker.prototype.enterDocument = function() {
  'use strict';
  // this.popupDatePicker_ has been added as a child even though it isn't really
  // a child (since its root element is not within InputDatePicker's DOM tree).
  // The PopupDatePicker will have its enterDocument method called as a result
  // of calling the superClass's enterDocument method. The PopupDatePicker needs
  // to be attached to the document *before* calling enterDocument so that when
  // PopupDatePicker decorates its element as a DatePicker, the element will be
  // in the document and enterDocument will be called for the DatePicker. Having
  // the PopupDatePicker's element in the document before calling enterDocument
  // will ensure that the event handlers for DatePicker are attached.
  //
  // An alternative could be to stop adding popupDatePicker_ as a child and
  // instead keep a reference to it and sync some event handlers, etc. but
  // appending the element to the document before calling enterDocument is a
  // less intrusive option.
  //
  // See cl/100837907 for more context and the discussion around this decision.
  (this.popupParentElement_ || this.getDomHelper().getDocument().body)
      .appendChild(/** @type {!Node} */ (this.popupDatePicker_.getElement()));

  goog.ui.InputDatePicker.superClass_.enterDocument.call(this);
  var el = this.getElement();

  this.popupDatePicker_.attach(el);

  // Set the date picker to have the input's initial value, if any.
  this.popupDatePicker_.setDate(this.getInputValueAsDate_());

  var handler = this.getHandler();
  handler.listen(
      this.popupDatePicker_, goog.ui.DatePicker.Events.CHANGE,
      this.onDateChanged_);
  handler.listen(
      this.popupDatePicker_, goog.ui.PopupBase.EventType.SHOW, this.onPopup_);
};


/** @override */
goog.ui.InputDatePicker.prototype.exitDocument = function() {
  'use strict';
  goog.ui.InputDatePicker.superClass_.exitDocument.call(this);
  var el = this.getElement();

  this.popupDatePicker_.detach(el);
  this.popupDatePicker_.exitDocument();
  goog.dom.removeNode(this.popupDatePicker_.getElement());
};


/** @override */
goog.ui.InputDatePicker.prototype.decorateInternal = function(element) {
  'use strict';
  goog.ui.InputDatePicker.superClass_.decorateInternal.call(this, element);

  this.popupDatePicker_.createDom();
};


/** @override */
goog.ui.InputDatePicker.prototype.disposeInternal = function() {
  'use strict';
  goog.ui.InputDatePicker.superClass_.disposeInternal.call(this);
  this.popupDatePicker_.dispose();
  this.popupDatePicker_ = null;
  this.popupParentElement_ = null;
};


/**
 * See goog.ui.PopupDatePicker.showPopup().
 * @param {Element} element Reference element for displaying the popup -- popup
 *     will appear at the bottom-left corner of this element.
 */
goog.ui.InputDatePicker.prototype.showForElement = function(element) {
  'use strict';
  this.popupDatePicker_.showPopup(element);
};


/**
 * See goog.ui.PopupDatePicker.hidePopup().
 */
goog.ui.InputDatePicker.prototype.hidePopup = function() {
  'use strict';
  this.popupDatePicker_.hidePopup();
};


/**
 * Event handler for popup date picker popup events.
 *
 * @param {goog.events.Event} e popup event.
 * @private
 */
goog.ui.InputDatePicker.prototype.onPopup_ = function(e) {
  'use strict';
  var inputValueAsDate = this.getInputValueAsDate_();
  this.setDate(inputValueAsDate);
  // don't overwrite the input value with empty date if input is not valid
  if (inputValueAsDate) {
    this.setInputValueAsDate_(this.getDatePicker().getDate());
  }
};


/**
 * Event handler for date change events.  Called when the date changes.
 *
 * @param {goog.ui.DatePickerEvent} e Date change event.
 * @private
 */
goog.ui.InputDatePicker.prototype.onDateChanged_ = function(e) {
  'use strict';
  this.setInputValueAsDate_(e.date);
};

/**
 * A DateFormatter implements functionality to convert a Date into
 * human-readable text. text into a Date. This interface is expected to accept
 * an instance of goog.i18n.DateTimeFormat directly, and as such the method
 * signatures directly match those found on that class.
 * @record
 */
goog.ui.InputDatePicker.DateFormatter = function() {};

/**
 * @param {!goog.date.DateLike} date The Date object that is being formatted.
 * @return {string} The formatted date value.
 */
goog.ui.InputDatePicker.DateFormatter.prototype.format = function(date) {};

/**
 * A DateParser implements functionality to parse text into a Date. This
 * interface is expected to accept an instance of goog.i18n.DateTimeParse
 * directly, and as such the method signatures directly match those found on
 * that class.
 * @record
 */
goog.ui.InputDatePicker.DateParser = function() {};

/**
 * @param {string} text The string being parsed.
 * @param {!goog.date.DateLike} date The Date object to hold the parsed date.
 * @param {!goog.i18n.DateTimeParse.ParseOptions=} options The options object.
 * @return {number} How many characters parser advanced.
 */
goog.ui.InputDatePicker.DateParser.prototype.parse = function(
    text, date, options) {};
