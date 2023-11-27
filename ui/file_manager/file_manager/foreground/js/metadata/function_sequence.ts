// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataParser} from './metadata_parser.js';

/**
 * Invoke steps in sequence.
 */
export class FunctionSequence {
  private currentStepIdx_ = -1;
  private failed_ = false;

  started = false;
  onError: (err: string) => void;
  finish: () => void;
  nextStep: (...args: unknown[]) => void;

  /**
   * @param steps Array of functions to invoke in sequence.
   * @param logger Logger object.
   * @param callback Callback to invoke on success.
   * @param failureCallback Callback to invoke on failure.
   */
  constructor(
      private steps_: Function[], private logger: MetadataParser,
      private callback_: VoidCallback,
      private failureCallback_: (error: string) => void) {
    this.onError = this.onError_.bind(this);
    this.finish = this.finish_.bind(this);
    this.nextStep = this.nextStep_.bind(this);
  }

  /**
   * Sets new callback
   *
   * @param callback New callback to call on succeed.
   */
  setCallback(callback: VoidCallback) {
    this.callback_ = callback;
  }

  /**
   * Sets new error callback
   *
   * @param failureCallback New callback to call on failure.
   */
  setFailureCallback(failureCallback: (error: string) => void) {
    this.failureCallback_ = failureCallback;
  }


  /**
   * Error handling function, which traces current error step, stops sequence
   * advancing and fires error callback.
   *
   * @param err Error message.
   */
  private onError_(err: string) {
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
   */
  private finish_() {
    if (!this.failed_ && this.currentStepIdx_ < this.steps_.length) {
      this.currentStepIdx_ = this.steps_.length;
      this.callback_();
    }
  }

  /**
   * Advances to next step.
   * This method should not be used externally. In external cases should be used
   * nextStep function, which is defined in closure and thus has access to
   * internal variables of functionsequence.
   * @param args Arguments to be passed to the next step.
   */
  private nextStep_(...args: unknown[]) {
    if (this.failed_) {
      return;
    }

    if (++this.currentStepIdx_ >= this.steps_.length) {
      this.logger.vlog('Sequence ended');
      this.callback_.apply(this);
    } else {
      this.logger.vlog(
          'Attempting to start step [' +
          this.steps_[this.currentStepIdx_]?.name + ']');
      try {
        this.steps_[this.currentStepIdx_]?.apply(this, args);
      } catch (e) {
        this.onError(e!.toString());
      }
    }
  }

  /**
   * This function should be called only once on start, so start sequence
   * pipeline
   * @param args Arguments to be passed to the first step.
   */
  start(...args: unknown[]) {
    if (this.started) {
      throw new Error('"Start" method of FunctionSequence was called twice');
    }

    this.logger.log('Starting sequence with ' + args.length + ' arguments');

    this.started = true;
    this.nextStep.apply(this, args);
  }
}
