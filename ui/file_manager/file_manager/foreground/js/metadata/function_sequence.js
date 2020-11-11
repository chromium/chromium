// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @class FunctionSequence to invoke steps in sequence.
 */
class FunctionSequence {
  /**
   * @param {string} name Name of the function.
   * @param {Array} steps Array of functions to invoke in sequence.
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
    /** @public {boolean} */
    this.started = false;

    this.onError = this.onError_.bind(this);
    this.finish = this.finish_.bind(this);
    this.nextStep = this.nextStep_.bind(this);
    this.apply = this.apply_.bind(this);
  }

  /**
   * Sets new callback
   *
   * @param {function()} callback New callback to call on succeed.
   */
  setCallback(callback) {
    this.callback_ = callback;
  }

  /**
   * Sets new error callback
   *
   * @param {function(string)} failureCallback New callback to call on failure.
   */
  setFailureCallback(failureCallback) {
    this.failureCallback_ = failureCallback;
  }


  /**
   * Error handling function, which traces current error step, stops sequence
   * advancing and fires error callback.
   *
   * @param {string} err Error message.
   * @private
   */
  onError_(err) {
    this.logger.vlog(
        'Failed step: ' + this.steps_[this.currentStepIdx_].name + ': ' + err);
    if (!this.failed_) {
      this.failed_ = true;
      this.failureCallback_(err);
    }
  }

  /**
   * Finishes sequence processing and jumps to the last step.
   * This method should not be used externally. In external
   * cases should be used finish function, which is defined in closure and thus
   * has access to internal variables of functionsequence.
   * @private
   */
  finish_() {
    if (!this.failed_ && this.currentStepIdx_ < this.steps_.length) {
      this.currentStepIdx_ = this.steps_.length;
      this.callback_();
    }
  }

  /**
   * Advances to next step.
   * This method should not be used externally. In external
   * cases should be used nextStep function, which is defined in closure and
   * thus has access to internal variables of functionsequence.
   * @param {...} var_args Arguments to be passed to the next step.
   * @private
   */
  nextStep_(var_args) {
    if (this.failed_) {
      return;
    }

    if (++this.currentStepIdx_ >= this.steps_.length) {
      this.logger.vlog('Sequence ended');
      this.callback_.apply(this, arguments);
    } else {
      this.logger.vlog(
          'Attempting to start step [' +
          this.steps_[this.currentStepIdx_].name + ']');
      try {
        this.steps_[this.currentStepIdx_].apply(this, arguments);
      } catch (e) {
        this.onError(e.toString());
      }
    }
  }

  /**
   * This function should be called only once on start, so start sequence
   * pipeline
   * @param {...} var_args Arguments to be passed to the first step.
   */
  start(var_args) {
    if (this.started) {
      throw new Error('"Start" method of FunctionSequence was called twice');
    }

    this.logger.log(
        'Starting sequence with ' + arguments.length + ' arguments');

    this.started = true;
    this.nextStep.apply(this, arguments);
  }

  /**
   * Add Function object mimics to FunctionSequence
   * @private
   * @param {*} obj Object.
   * @param {Array} args Arguments.
   */
  apply_(obj, args) {
    this.start.apply(this, args);
  }
}
