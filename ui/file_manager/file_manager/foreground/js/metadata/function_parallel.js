// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @class FunctionParallel to invoke steps in parallel.
 */
class FunctionParallel {
  /**
   * @param {string} name Name of the function.
   * @param {Array<Function>} steps Array of functions to invoke in parallel.
   * @param {MetadataParser} logger Logger object.
   * @param {function()} callback Callback to invoke on success.
   * @param {function(string)} failureCallback Callback to invoke on failure.
   */
  constructor(name, steps, logger, callback, failureCallback) {
    // Private variables hidden in closure
    this.currentStepIdx_ = -1;
    this.failed_ = false;
    this.steps_ = steps;
    this.callback_ = callback;
    this.failureCallback_ = failureCallback;
    this.logger = logger;
    this.name = name;

    this.remaining = this.steps_.length;

    this.nextStep = this.nextStep_.bind(this);
    this.onError = this.onError_.bind(this);
    this.apply = this.start.bind(this);
  }

  /**
   * Error handling function, which fires error callback.
   *
   * @param {string} err Error message.
   * @private
   */
  onError_(err) {
    if (!this.failed_) {
      this.failed_ = true;
      this.failureCallback_(err);
    }
  }

  /**
   * Advances to next step. This method should not be used externally. In
   * external cases should be used nextStep function, which is defined in
   * closure and thus has access to internal variables of functionsequence.
   *
   * @private
   */
  nextStep_() {
    if (--this.remaining == 0 && !this.failed_) {
      this.callback_();
    }
  }

  /**
   * This function should be called only once on start, so start all the
   * children at once
   * @param {...} var_args Arguments to be passed to all the steps.
   */
  start(var_args) {
    this.logger.vlog(
        'Starting [' + this.steps_.length + '] parallel tasks ' +
        'with ' + arguments.length + ' argument(s)');
    if (this.logger.verbose) {
      for (let j = 0; j < arguments.length; j++) {
        this.logger.vlog(arguments[j]);
      }
    }
    for (let i = 0; i < this.steps_.length; i++) {
      this.logger.vlog(
          'Attempting to start step [' + this.steps_[i].name + ']');
      try {
        this.steps_[i].apply(this, arguments);
      } catch (e) {
        this.onError(e.toString());
      }
    }
  }
}
