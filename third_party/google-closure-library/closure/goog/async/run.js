/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.provide('goog.async.run');

goog.require('goog.async.WorkQueue');
goog.require('goog.async.nextTick');
goog.require('goog.async.throwException');

/**
 * @define {boolean} If true, use the global Promise to implement goog.async.run
 * assuming either the native, or polyfill version will be used. Does still
 * permit tests to use forceNextTick.
 */
goog.ASSUME_NATIVE_PROMISE = goog.define('goog.ASSUME_NATIVE_PROMISE', false);

/**
 * Fires the provided callback just before the current callstack unwinds, or as
 * soon as possible after the current JS execution context.
 * @param {function(this:THIS)} callback
 * @param {THIS=} opt_context Object to use as the "this value" when calling
 *     the provided function.
 * @template THIS
 */
goog.async.run = function(callback, opt_context) {
  'use strict';
  if (!goog.async.run.schedule_) {
    goog.async.run.initializeRunner_();
  }
  if (!goog.async.run.workQueueScheduled_) {
    // Nothing is currently scheduled, schedule it now.
    goog.async.run.schedule_();
    goog.async.run.workQueueScheduled_ = true;
  }

  goog.async.run.workQueue_.add(callback, opt_context);
};


/**
 * Initializes the function to use to process the work queue.
 * @private
 */
goog.async.run.initializeRunner_ = function() {
  'use strict';
  if (goog.ASSUME_NATIVE_PROMISE ||
      (goog.global.Promise && goog.global.Promise.resolve)) {
    // Use goog.global.Promise instead of just Promise because the relevant
    // externs may be missing, and don't alias it because this could confuse the
    // compiler into thinking the polyfill is required when it should be treated
    // as optional.
    var promise = goog.global.Promise.resolve(undefined);
    goog.async.run.schedule_ = function() {
      'use strict';
      promise.then(goog.async.run.processWorkQueue);
    };
  } else {
    goog.async.run.schedule_ = function() {
      'use strict';
      goog.async.nextTick(goog.async.run.processWorkQueue);
    };
  }
};


/**
 * Forces goog.async.run to use nextTick instead of Promise.
 *
 * This should only be done in unit tests. It's useful because MockClock
 * replaces nextTick, but not the browser Promise implementation, so it allows
 * Promise-based code to be tested with MockClock.
 *
 * However, we also want to run promises if the MockClock is no longer in
 * control so we schedule a backup "setTimeout" to the unmocked timeout if
 * provided.
 *
 * @param {function(function())=} opt_realSetTimeout
 */
goog.async.run.forceNextTick = function(opt_realSetTimeout) {
  'use strict';
  goog.async.run.schedule_ = function() {
    'use strict';
    goog.async.nextTick(goog.async.run.processWorkQueue);
    if (opt_realSetTimeout) {
      opt_realSetTimeout(goog.async.run.processWorkQueue);
    }
  };
};


/**
 * The function used to schedule work asynchronousely.
 * @private {function()}
 */
goog.async.run.schedule_;


/** @private {boolean} */
goog.async.run.workQueueScheduled_ = false;


/** @private {!goog.async.WorkQueue} */
goog.async.run.workQueue_ = new goog.async.WorkQueue();


if (goog.DEBUG) {
  /**
   * Reset the work queue. Only available for tests in debug mode.
   */
  goog.async.run.resetQueue = function() {
    'use strict';
    goog.async.run.workQueueScheduled_ = false;
    goog.async.run.workQueue_ = new goog.async.WorkQueue();
  };


  /**
   * Resets the scheduler. Only available for tests in debug mode.
   */
  goog.async.run.resetSchedulerForTest = function() {
    goog.async.run.initializeRunner_();
  };
}


/**
 * Run any pending goog.async.run work items. This function is not intended
 * for general use, but for use by entry point handlers to run items ahead of
 * goog.async.nextTick.
 */
goog.async.run.processWorkQueue = function() {
  'use strict';
  // NOTE: additional work queue items may be added while processing.
  var item = null;
  while (item = goog.async.run.workQueue_.remove()) {
    try {
      item.fn.call(item.scope);
    } catch (e) {
      goog.async.throwException(e);
    }
    goog.async.run.workQueue_.returnUnused(item);
  }

  // There are no more work items, allow processing to be scheduled again.
  goog.async.run.workQueueScheduled_ = false;
};
