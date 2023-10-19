// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataParser} from './metadata_parser.js';

/**
 * @class FunctionSequence to invoke steps in sequence.
 */
export class FunctionSequence {
  /**
   * @param {string} name Name of the function.
   * @param {Array<Function>} steps Array of functions to invoke in sequence.
   * @param {MetadataParser} logger Logger object.
   * @param {function():void} callback Callback to invoke on success.
   * @param {function(string):void} failureCallback Callback to invoke on
   *     failure.
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
    /** @public @type {boolean} */
    this.started = false;

    this.onError = this.onError_.bind(this);
    this.finish = this.finish_.bind(this);
    this.nextStep = this.nextStep_.bind(this);
    this.apply = this.apply_.bind(this);
  }

  /**
   * Sets new callback
   *
   * @param {function():void} callback New callback to call on succeed.
   */
  setCallback(callback) {
    this.callback_ = callback;
  }

  /**
   * Sets new error callback
   *
   * @param {function(string):void} failureCallback New callback to call on
   *     failure.
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
        'Failed step: ' + this.steps_[this.currentStepIdx_]?.name + ': ' + err);
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
   * @param {...*} var_args Arguments to be passed to the next step.
   * @private
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  nextStep_(var_args) {
    if (this.failed_) {
      return;
    }

    if (++this.currentStepIdx_ >= this.steps_.length) {
      this.logger.vlog('Sequence ended');
      // @ts-ignore: error TS2345: Argument of type 'IArguments' is not
      // assignable to parameter of type '[]'.
      this.callback_.apply(this, arguments);
    } else {
      this.logger.vlog(
          'Attempting to start step [' +
          this.steps_[this.currentStepIdx_]?.name + ']');
      try {
        this.steps_[this.currentStepIdx_]?.apply(this, arguments);
      } catch (e) {
        // @ts-ignore: error TS18046: 'e' is of type 'unknown'.
        this.onError(e.toString());
      }
    }
  }

  /**
   * This function should be called only once on start, so start sequence
   * pipeline
   * @param {...*} var_args Arguments to be passed to the first step.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  start(var_args) {
    if (this.started) {
      throw new Error('"Start" method of FunctionSequence was called twice');
    }

    this.logger.log(
        'Starting sequence with ' + arguments.length + ' arguments');

    this.started = true;
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type 'any[]'.
    this.nextStep.apply(this, arguments);
  }

  /**
   * Add Function object mimics to FunctionSequence
   * @private
   * @param {*} obj Object.
   * @param {Array<*>} args Arguments.
   */
  // @ts-ignore: error TS6133: 'obj' is declared but its value is never read.
  apply_(obj, args) {
    this.start.apply(this, args);
  }
}
