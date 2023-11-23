// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MetadataParser} from './metadata_parser.js';

/**
 * To invoke steps in parallel.
 */
export class FunctionParallel {
  private failed_ = false;
  private remaining_: number;
  nextStep: () => void;
  onError: (err: string) => void;

  /**
   * @param steps_ Array of functions to invoke in parallel.
   * @param logger_ Logger object.
   * @param callback_ Callback to invoke on success.
   * @param failureCallback_ Callback to invoke on failure.
   */
  constructor(
      private steps_: Function[], private logger_: MetadataParser,
      private callback_: VoidCallback,
      private failureCallback_: (err: string) => void) {
    this.remaining_ = this.steps_.length;
    this.nextStep = this.nextStep_.bind(this);
    this.onError = this.onError_.bind(this);
  }

  /**
   * Error handling function, which fires error callback.
   * @param err Error message.
   */
  private onError_(err: string) {
    if (!this.failed_) {
      this.failed_ = true;
      this.failureCallback_(err);
    }
  }

  /**
   * Advances to next step. This method should not be used externally. In
   * external cases should be used nextStep function, which is defined in
   * closure and thus has access to internal variables of functionsequence.
   */
  private nextStep_() {
    if (--this.remaining_ == 0 && !this.failed_) {
      this.callback_();
    }
  }

  /**
   * This function should be called only once on start, so start all the
   * children at once
   * @param args Arguments to be passed to all the steps.
   */
  start(...args: object[]) {
    this.logger_.vlog(
        'Starting [' + this.steps_.length + '] parallel tasks ' +
        'with ' + args.length + ' argument(s)');
    if (this.logger_.verbose) {
      for (const arg of args) {
        this.logger_.vlog(arg);
      }
    }
    for (const step of this.steps_) {
      this.logger_.vlog('Attempting to start step [' + step.name + ']');
      try {
        step.apply(this, args);
      } catch (e) {
        this.onError(e!.toString());
      }
    }
  }
}
