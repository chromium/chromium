/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Static utilities for collecting stats associated with
 * ChannelRequest.
 *
 */


goog.provide('goog.labs.net.webChannel.requestStats');
goog.provide('goog.labs.net.webChannel.requestStats.Event');
goog.provide('goog.labs.net.webChannel.requestStats.ServerReachability');
goog.provide('goog.labs.net.webChannel.requestStats.ServerReachabilityEvent');
goog.provide('goog.labs.net.webChannel.requestStats.Stat');
goog.provide('goog.labs.net.webChannel.requestStats.StatEvent');
goog.provide('goog.labs.net.webChannel.requestStats.TimingEvent');

goog.require('goog.events.Event');
goog.require('goog.events.EventTarget');


goog.scope(function() {
'use strict';
const requestStats = goog.labs.net.webChannel.requestStats;


/**
 * Events fired.
 * @const
 */
requestStats.Event = {};


/**
 * Singleton event target for firing stat events
 * @type {?goog.events.EventTarget}
 * @private
 */
requestStats.eventTarget_ = null;

/**
 * Singleton event target for firing stat events
 * @return {!goog.events.EventTarget}
 * @private
 */
requestStats.getStatEventTarget_ = function() {
  'use strict';
  requestStats.eventTarget_ =
      requestStats.eventTarget_ || new goog.events.EventTarget();
  return requestStats.eventTarget_;
};

/**
 * The type of event that occurs every time some information about how reachable
 * the server is is discovered.
 */
requestStats.Event.SERVER_REACHABILITY_EVENT = 'serverreachability';


/**
 * Types of events which reveal information about the reachability of the
 * server.
 * @enum {number}
 */
requestStats.ServerReachability = {
  REQUEST_MADE: 1,
  REQUEST_SUCCEEDED: 2,
  REQUEST_FAILED: 3,
  BACK_CHANNEL_ACTIVITY: 4  // any response data received
};



/**
 * Event class for SERVER_REACHABILITY_EVENT.
 *
 * @param {goog.events.EventTarget} target The stat event target for
       the channel.
 * @param {requestStats.ServerReachability} reachabilityType
 *     The reachability event type.
 * @constructor
 * @extends {goog.events.Event}
 */
requestStats.ServerReachabilityEvent = function(target, reachabilityType) {
  'use strict';
  goog.events.Event.call(
      this, requestStats.Event.SERVER_REACHABILITY_EVENT, target);

  /**
   * @type {requestStats.ServerReachability}
   */
  this.reachabilityType = reachabilityType;
};
goog.inherits(requestStats.ServerReachabilityEvent, goog.events.Event);


/**
 * Notify the channel that a particular fine grained network event has occurred.
 * Should be considered package-private.
 * @param {requestStats.ServerReachability} reachabilityType
 *     The reachability event type.
 */
requestStats.notifyServerReachabilityEvent = function(reachabilityType) {
  'use strict';
  const target = requestStats.getStatEventTarget_();
  target.dispatchEvent(
      new requestStats.ServerReachabilityEvent(target, reachabilityType));
};


/**
 * Stat Event that fires when things of interest happen that may be useful for
 * applications to know about for stats or debugging purposes.
 */
requestStats.Event.STAT_EVENT = 'statevent';


/**
 * Enum that identifies events for statistics that are interesting to track.
 * @enum {number}
 */
requestStats.Stat = {
  /** Event indicating a new connection attempt. */
  CONNECT_ATTEMPT: 0,

  /** Event indicating a connection error due to a general network problem. */
  ERROR_NETWORK: 1,

  /**
   * Event indicating a connection error that isn't due to a general network
   * problem.
   */
  ERROR_OTHER: 2,

  /** Event indicating the start of test stage one. */
  TEST_STAGE_ONE_START: 3,

  /** Event indicating the start of test stage two. */
  TEST_STAGE_TWO_START: 4,

  /** Event indicating the first piece of test data was received. */
  TEST_STAGE_TWO_DATA_ONE: 5,

  /**
   * Event indicating that the second piece of test data was received and it was
   * received separately from the first.
   */
  TEST_STAGE_TWO_DATA_TWO: 6,

  /** Event indicating both pieces of test data were received simultaneously. */
  TEST_STAGE_TWO_DATA_BOTH: 7,

  /** Event indicating stage one of the test request failed. */
  TEST_STAGE_ONE_FAILED: 8,

  /** Event indicating stage two of the test request failed. */
  TEST_STAGE_TWO_FAILED: 9,

  /**
   * Event indicating that a buffering proxy is likely between the client and
   * the server.
   */
  PROXY: 10,

  /**
   * Event indicating that no buffering proxy is likely between the client and
   * the server.
   */
  NOPROXY: 11,

  /** Event indicating an unknown SID error. */
  REQUEST_UNKNOWN_SESSION_ID: 12,

  /** Event indicating a bad status code was received. */
  REQUEST_BAD_STATUS: 13,

  /** Event indicating incomplete data was received */
  REQUEST_INCOMPLETE_DATA: 14,

  /** Event indicating bad data was received */
  REQUEST_BAD_DATA: 15,

  /** Event indicating no data was received when data was expected. */
  REQUEST_NO_DATA: 16,

  /** Event indicating a request timeout. */
  REQUEST_TIMEOUT: 17,

  /**
   * Event indicating that the server never received our hanging GET and so it
   * is being retried.
   */
  BACKCHANNEL_MISSING: 18,

  /**
   * Event indicating that we have determined that our hanging GET is not
   * receiving data when it should be. Thus it is dead dead and will be retried.
   */
  BACKCHANNEL_DEAD: 19,

  /**
   * The browser declared itself offline during the lifetime of a request, or
   * was offline when a request was initially made.
   */
  BROWSER_OFFLINE: 20
};



/**
 * Event class for STAT_EVENT.
 *
 * @param {goog.events.EventTarget} eventTarget The stat event target for
       the channel.
 * @param {requestStats.Stat} stat The stat.
 * @constructor
 * @extends {goog.events.Event}
 */
requestStats.StatEvent = function(eventTarget, stat) {
  'use strict';
  goog.events.Event.call(this, requestStats.Event.STAT_EVENT, eventTarget);

  /**
   * The stat
   * @type {requestStats.Stat}
   */
  this.stat = stat;
};
goog.inherits(requestStats.StatEvent, goog.events.Event);


/**
 * Returns the singleton event target for stat events.
 * @return {!goog.events.EventTarget} The event target for stat events.
 */
requestStats.getStatEventTarget = function() {
  'use strict';
  return requestStats.getStatEventTarget_();
};


/**
 * Helper function to call the stat event callback.
 * @param {requestStats.Stat} stat The stat.
 */
requestStats.notifyStatEvent = function(stat) {
  'use strict';
  const target = requestStats.getStatEventTarget_();
  target.dispatchEvent(new requestStats.StatEvent(target, stat));
};


/**
 * An event that fires when POST requests complete successfully, indicating
 * the size of the POST and the round trip time.
 */
requestStats.Event.TIMING_EVENT = 'timingevent';



/**
 * Event class for requestStats.Event.TIMING_EVENT
 *
 * @param {goog.events.EventTarget} target The stat event target for
       the channel.
 * @param {number} size The number of characters in the POST data.
 * @param {number} rtt The total round trip time from POST to response in MS.
 * @param {number} retries The number of times the POST had to be retried.
 * @constructor
 * @extends {goog.events.Event}
 */
requestStats.TimingEvent = function(target, size, rtt, retries) {
  'use strict';
  goog.events.Event.call(this, requestStats.Event.TIMING_EVENT, target);

  /**
   * @type {number}
   */
  this.size = size;

  /**
   * @type {number}
   */
  this.rtt = rtt;

  /**
   * @type {number}
   */
  this.retries = retries;
};
goog.inherits(requestStats.TimingEvent, goog.events.Event);


/**
 * Helper function to notify listeners about POST request performance.
 *
 * @param {number} size Number of characters in the POST data.
 * @param {number} rtt The amount of time from POST start to response.
 * @param {number} retries The number of times the POST had to be retried.
 */
requestStats.notifyTimingEvent = function(size, rtt, retries) {
  'use strict';
  const target = requestStats.getStatEventTarget_();
  target.dispatchEvent(
      new requestStats.TimingEvent(target, size, rtt, retries));
};


/**
 * Allows the application to set an execution hooks for when a channel
 * starts processing requests. This is useful to track timing or logging
 * special information. The function takes no parameters and return void.
 * @param {Function} startHook  The function for the start hook.
 */
requestStats.setStartThreadExecutionHook = function(startHook) {
  'use strict';
  requestStats.startExecutionHook_ = startHook;
};


/**
 * Allows the application to set an execution hooks for when a channel
 * stops processing requests. This is useful to track timing or logging
 * special information. The function takes no parameters and return void.
 * @param {Function} endHook  The function for the end hook.
 */
requestStats.setEndThreadExecutionHook = function(endHook) {
  'use strict';
  requestStats.endExecutionHook_ = endHook;
};


/**
 * Application provided execution hook for the start hook.
 *
 * @type {Function}
 * @private
 */
requestStats.startExecutionHook_ = function() {};


/**
 * Application provided execution hook for the end hook.
 *
 * @type {Function}
 * @private
 */
requestStats.endExecutionHook_ = function() {};


/**
 * Helper function to call the start hook
 */
requestStats.onStartExecution = function() {
  'use strict';
  requestStats.startExecutionHook_();
};


/**
 * Helper function to call the end hook
 */
requestStats.onEndExecution = function() {
  'use strict';
  requestStats.endExecutionHook_();
};


/**
 * Wrapper around SafeTimeout which calls the start and end execution hooks
 * with a try...finally block.
 * @param {Function} fn The callback function.
 * @param {number} ms The time in MS for the timer.
 * @return {number} The ID of the timer.
 */
requestStats.setTimeout = function(fn, ms) {
  'use strict';
  if (typeof fn !== 'function') {
    throw new Error('Fn must not be null and must be a function');
  }
  return goog.global.setTimeout(function() {
    'use strict';
    requestStats.onStartExecution();
    try {
      fn();
    } finally {
      requestStats.onEndExecution();
    }
  }, ms);
};
});  // goog.scope
