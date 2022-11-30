/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock of IframeIo for unit testing.
 */

goog.provide('goog.net.MockIFrameIo');
goog.require('goog.events.EventTarget');
goog.require('goog.net.ErrorCode');
goog.require('goog.net.EventType');
goog.require('goog.net.IframeIo');
goog.requireType('goog.Uri');
goog.requireType('goog.structs.Map');
goog.requireType('goog.testing.TestQueue');



/**
 * Mock implementation of goog.net.IframeIo. This doesn't provide a mock
 * implementation for all cases, but it's not too hard to add them as needed.
 * @param {goog.testing.TestQueue} testQueue Test queue for inserting test
 *     events.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @final
 * @deprecated Use goog.testing.net.MockIFrameIo instead.
 */
goog.net.MockIFrameIo = function(testQueue) {
  'use strict';
  goog.events.EventTarget.call(this);

  /**
   * Queue of events write to
   * @type {goog.testing.TestQueue}
   * @private
   */
  this.testQueue_ = testQueue;
};
goog.inherits(goog.net.MockIFrameIo, goog.events.EventTarget);


/**
 * Whether MockIFrameIo is active.
 * @type {boolean}
 * @private
 */
goog.net.MockIFrameIo.prototype.active_ = false;


/**
 * Last content.
 * @type {string}
 * @private
 */
goog.net.MockIFrameIo.prototype.lastContent_ = '';


/**
 * Last error code.
 * @type {goog.net.ErrorCode}
 * @private
 */
goog.net.MockIFrameIo.prototype.lastErrorCode_ = goog.net.ErrorCode.NO_ERROR;


/**
 * Last error message.
 * @type {string}
 * @private
 */
goog.net.MockIFrameIo.prototype.lastError_ = '';


/**
 * Last custom error.
 * @type {?Object}
 * @private
 */
goog.net.MockIFrameIo.prototype.lastCustomError_ = null;


/**
 * Last URI.
 * @type {?goog.Uri}
 * @private
 */
goog.net.MockIFrameIo.prototype.lastUri_ = null;


/** @private {Function} */
goog.net.MockIFrameIo.prototype.errorChecker_;


/** @private {boolean} */
goog.net.MockIFrameIo.prototype.success_;


/** @private {boolean} */
goog.net.MockIFrameIo.prototype.complete_;


/**
 * Simulates the iframe send.
 *
 * @param {goog.Uri|string} uri Uri of the request.
 * @param {string=} opt_method Default is GET, POST uses a form to submit the
 *     request.
 * @param {boolean=} opt_noCache Append a timestamp to the request to avoid
 *     caching.
 * @param {Object|goog.structs.Map=} opt_data Map of key-value pairs.
 */
goog.net.MockIFrameIo.prototype.send = function(
    uri, opt_method, opt_noCache, opt_data) {
  'use strict';
  if (this.active_) {
    throw new Error('[goog.net.IframeIo] Unable to send, already active.');
  }

  this.testQueue_.enqueue(['s', uri, opt_method, opt_noCache, opt_data]);
  this.complete_ = false;
  this.active_ = true;
};


/**
 * Simulates the iframe send from a form.
 * @param {Element} form Form element used to send the request to the server.
 * @param {string=} opt_uri Uri to set for the destination of the request, by
 *     default the uri will come from the form.
 * @param {boolean=} opt_noCache Append a timestamp to the request to avoid
 *     caching.
 */
goog.net.MockIFrameIo.prototype.sendFromForm = function(
    form, opt_uri, opt_noCache) {
  'use strict';
  if (this.active_) {
    throw new Error('[goog.net.IframeIo] Unable to send, already active.');
  }

  this.testQueue_.enqueue(['s', form, opt_uri, opt_noCache]);
  this.complete_ = false;
  this.active_ = true;
};


/**
 * Simulates aborting the current Iframe request.
 * @param {goog.net.ErrorCode=} opt_failureCode Optional error code to use -
 *     defaults to ABORT.
 */
goog.net.MockIFrameIo.prototype.abort = function(opt_failureCode) {
  'use strict';
  if (this.active_) {
    this.testQueue_.enqueue(['a', opt_failureCode]);
    this.complete_ = false;
    this.active_ = false;
    this.success_ = false;
    this.lastErrorCode_ = opt_failureCode || goog.net.ErrorCode.ABORT;
    this.dispatchEvent(goog.net.EventType.ABORT);
    this.simulateReady();
  }
};


/**
 * Simulates receive of incremental data.
 * @param {Object} data Data.
 */
goog.net.MockIFrameIo.prototype.simulateIncrementalData = function(data) {
  'use strict';
  this.dispatchEvent(new goog.net.IframeIo.IncrementalDataEvent(data));
};


/**
 * Simulates the iframe is done.
 * @param {goog.net.ErrorCode} errorCode The error code for any error that
 *     should be simulated.
 */
goog.net.MockIFrameIo.prototype.simulateDone = function(errorCode) {
  'use strict';
  if (errorCode) {
    this.success_ = false;
    this.lastErrorCode_ = goog.net.ErrorCode.HTTP_ERROR;
    this.lastError_ = this.getLastError();
    this.dispatchEvent(goog.net.EventType.ERROR);
  } else {
    this.success_ = true;
    this.lastErrorCode_ = goog.net.ErrorCode.NO_ERROR;
    this.dispatchEvent(goog.net.EventType.SUCCESS);
  }
  this.complete_ = true;
  this.dispatchEvent(goog.net.EventType.COMPLETE);
};


/**
 * Simulates the IFrame is ready for the next request.
 */
goog.net.MockIFrameIo.prototype.simulateReady = function() {
  'use strict';
  this.dispatchEvent(goog.net.EventType.READY);
};


/**
 * @return {boolean} True if transfer is complete.
 */
goog.net.MockIFrameIo.prototype.isComplete = function() {
  'use strict';
  return this.complete_;
};


/**
 * @return {boolean} True if transfer was successful.
 */
goog.net.MockIFrameIo.prototype.isSuccess = function() {
  'use strict';
  return this.success_;
};


/**
 * @return {boolean} True if a transfer is in progress.
 */
goog.net.MockIFrameIo.prototype.isActive = function() {
  'use strict';
  return this.active_;
};


/**
 * Returns the last response text (i.e. the text content of the iframe).
 * Assumes plain text!
 * @return {string} Result from the server.
 */
goog.net.MockIFrameIo.prototype.getResponseText = function() {
  'use strict';
  return this.lastContent_;
};


/**
 * Parses the content as JSON. This is a safe parse and may throw an error
 * if the response is malformed.
 * @return {!Object} The parsed content.
 */
goog.net.MockIFrameIo.prototype.getResponseJson = function() {
  'use strict';
  return /** @type {!Object} */ (JSON.parse(this.lastContent_));
};


/**
 * Get the uri of the last request.
 * @return {goog.Uri} Uri of last request.
 */
goog.net.MockIFrameIo.prototype.getLastUri = function() {
  'use strict';
  return this.lastUri_;
};


/**
 * Gets the last error code.
 * @return {goog.net.ErrorCode} Last error code.
 */
goog.net.MockIFrameIo.prototype.getLastErrorCode = function() {
  'use strict';
  return this.lastErrorCode_;
};


/**
 * Gets the last error message.
 * @return {string} Last error message.
 */
goog.net.MockIFrameIo.prototype.getLastError = function() {
  'use strict';
  return goog.net.ErrorCode.getDebugMessage(this.lastErrorCode_);
};


/**
 * Gets the last custom error.
 * @return {Object} Last custom error.
 */
goog.net.MockIFrameIo.prototype.getLastCustomError = function() {
  'use strict';
  return this.lastCustomError_;
};


/**
 * Sets the callback function used to check if a loaded IFrame is in an error
 * state.
 * @param {Function} fn Callback that expects a document object as it's single
 *     argument.
 */
goog.net.MockIFrameIo.prototype.setErrorChecker = function(fn) {
  'use strict';
  this.errorChecker_ = fn;
};


/**
 * Gets the callback function used to check if a loaded IFrame is in an error
 * state.
 * @return {Function} A callback that expects a document object as it's single
 *     argument.
 */
goog.net.MockIFrameIo.prototype.getErrorChecker = function() {
  'use strict';
  return this.errorChecker_;
};
