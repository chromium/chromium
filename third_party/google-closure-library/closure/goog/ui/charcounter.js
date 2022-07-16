/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Character counter widget implementation.
 *
 * @see ../demos/charcounter.html
 */

goog.provide('goog.ui.CharCounter');
goog.provide('goog.ui.CharCounter.Display');

goog.require('goog.dom');
goog.require('goog.events');
goog.require('goog.events.EventTarget');
goog.require('goog.events.InputHandler');
goog.requireType('goog.events.BrowserEvent');



/**
 * CharCounter widget. Counts the number of characters in a input field or a
 * text box and displays the number of additional characters that may be
 * entered before the maximum length is reached.
 *
 * @extends {goog.events.EventTarget}
 * @param {HTMLInputElement|HTMLTextAreaElement} elInput Input or text area
 *     element to count the number of characters in.
 * @param {Element} elCount HTML element to display the remaining number of
 *     characters in. You can pass in null for this if you don't want to expose
 *     the number of chars remaining.
 * @param {number} maxLength The maximum length.
 * @param {goog.ui.CharCounter.Display=} opt_displayMode Display mode for this
 *     char counter. Defaults to {@link goog.ui.CharCounter.Display.REMAINING}.
 * @constructor
 * @final
 */
goog.ui.CharCounter = function(elInput, elCount, maxLength, opt_displayMode) {
  'use strict';
  goog.events.EventTarget.call(this);

  /**
   * Input or text area element to count the number of characters in.
   * @type {HTMLInputElement|HTMLTextAreaElement}
   * @private
   */
  this.elInput_ = elInput;

  /**
   * HTML element to display the remaining number of characters in.
   * @type {Element}
   * @private
   */
  this.elCount_ = elCount;

  /**
   * The maximum length.
   * @type {number}
   * @private
   */
  this.maxLength_ = maxLength;

  /**
   * The display mode for this char counter.
   * @type {!goog.ui.CharCounter.Display}
   * @private
   */
  this.display_ = opt_displayMode || goog.ui.CharCounter.Display.REMAINING;

  elInput.removeAttribute('maxlength');

  /**
   * The input handler that provides the input event.
   * @type {goog.events.InputHandler}
   * @private
   */
  this.inputHandler_ = new goog.events.InputHandler(elInput);

  goog.events.listen(
      this.inputHandler_, goog.events.InputHandler.EventType.INPUT,
      this.onChange_, false, this);

  this.checkLength();
};
goog.inherits(goog.ui.CharCounter, goog.events.EventTarget);


/**
 * Display mode for the char counter.
 * @enum {number}
 */
goog.ui.CharCounter.Display = {
  /** Widget displays the number of characters remaining (the default). */
  REMAINING: 0,
  /** Widget displays the number of characters entered. */
  INCREMENTAL: 1
};


/**
 * Sets the maximum length.
 *
 * @param {number} maxLength The maximum length.
 */
goog.ui.CharCounter.prototype.setMaxLength = function(maxLength) {
  'use strict';
  this.maxLength_ = maxLength;
  this.checkLength();
};


/**
 * Returns the maximum length.
 *
 * @return {number} The maximum length.
 */
goog.ui.CharCounter.prototype.getMaxLength = function() {
  'use strict';
  return this.maxLength_;
};


/**
 * Sets the display mode.
 *
 * @param {!goog.ui.CharCounter.Display} displayMode The display mode.
 */
goog.ui.CharCounter.prototype.setDisplayMode = function(displayMode) {
  'use strict';
  this.display_ = displayMode;
  this.checkLength();
};


/**
 * Returns the display mode.
 *
 * @return {!goog.ui.CharCounter.Display} The display mode.
 */
goog.ui.CharCounter.prototype.getDisplayMode = function() {
  'use strict';
  return this.display_;
};


/**
 * Change event handler for input field.
 *
 * @param {goog.events.BrowserEvent} event Change event.
 * @private
 */
goog.ui.CharCounter.prototype.onChange_ = function(event) {
  'use strict';
  this.checkLength();
};


/**
 * Checks length of text in input field and updates the counter. Truncates text
 * if the maximum lengths is exceeded.
 */
goog.ui.CharCounter.prototype.checkLength = function() {
  'use strict';
  var count = this.elInput_.value.length;

  // There's no maxlength property for textareas so instead we truncate the
  // text if it gets too long. It's also used to truncate the text in a input
  // field if the maximum length is changed.
  if (count > this.maxLength_) {
    var scrollTop = this.elInput_.scrollTop;
    var scrollLeft = this.elInput_.scrollLeft;

    this.elInput_.value = this.elInput_.value.substring(0, this.maxLength_);
    count = this.maxLength_;

    this.elInput_.scrollTop = scrollTop;
    this.elInput_.scrollLeft = scrollLeft;
  }

  if (this.elCount_) {
    var incremental = this.display_ == goog.ui.CharCounter.Display.INCREMENTAL;
    goog.dom.setTextContent(
        this.elCount_, String(incremental ? count : this.maxLength_ - count));
  }
};


/** @override */
goog.ui.CharCounter.prototype.disposeInternal = function() {
  'use strict';
  goog.ui.CharCounter.superClass_.disposeInternal.call(this);
  delete this.elInput_;
  this.inputHandler_.dispose();
  this.inputHandler_ = null;
};
