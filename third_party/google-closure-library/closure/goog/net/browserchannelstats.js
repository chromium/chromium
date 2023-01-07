/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of Statistics events for BrowserChannels.
 */

goog.module('goog.net.browserchannelinternal.stats');
goog.module.declareLegacyNamespace();

const Event = goog.require('goog.events.Event');
const EventTarget = goog.require('goog.events.EventTarget');

/**
 * Enum that identifies events for statistics that are interesting to track.
 * TODO(jonp) - Change name not to use Event or use EventTarget
 * @enum {number}
 */
const Stat = {
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


  /** Event indicating the channel is blocked by a network administrator. */
  CHANNEL_BLOCKED: 4,

  /** Event indicating the start of test stage two. */
  TEST_STAGE_TWO_START: 5,

  /** Event indicating the first piece of test data was received. */
  TEST_STAGE_TWO_DATA_ONE: 6,

  /**
   * Event indicating that the second piece of test data was received and it was
   * received separately from the first.
   */
  TEST_STAGE_TWO_DATA_TWO: 7,

  /** Event indicating both pieces of test data were received simultaneously. */
  TEST_STAGE_TWO_DATA_BOTH: 8,

  /** Event indicating stage one of the test request failed. */
  TEST_STAGE_ONE_FAILED: 9,

  /** Event indicating stage two of the test request failed. */
  TEST_STAGE_TWO_FAILED: 10,

  /**
   * Event indicating that a buffering proxy is likely between the client and
   * the server.
   */
  PROXY: 11,

  /**
   * Event indicating that no buffering proxy is likely between the client and
   * the server.
   */
  NOPROXY: 12,

  /** Event indicating an unknown SID error. */
  REQUEST_UNKNOWN_SESSION_ID: 13,

  /** Event indicating a bad status code was received. */
  REQUEST_BAD_STATUS: 14,

  /** Event indicating incomplete data was received */
  REQUEST_INCOMPLETE_DATA: 15,

  /** Event indicating bad data was received */
  REQUEST_BAD_DATA: 16,

  /** Event indicating no data was received when data was expected. */
  REQUEST_NO_DATA: 17,

  /** Event indicating a request timeout. */
  REQUEST_TIMEOUT: 18,

  /**
   * Event indicating that the server never received our hanging GET and so it
   * is being retried.
   */
  BACKCHANNEL_MISSING: 19,

  /**
   * Event indicating that we have determined that our hanging GET is not
   * receiving data when it should be. Thus it is dead dead and will be retried.
   */
  BACKCHANNEL_DEAD: 20,

  /**
   * The browser declared itself offline during the lifetime of a request, or
   * was offline when a request was initially made.
   */
  BROWSER_OFFLINE: 21,

  /** ActiveX is blocked by the machine's admin settings. */
  ACTIVE_X_BLOCKED: 22,
};
exports.Stat = Stat;

/**
 * Helper function to call the stat event callback.
 * @param {Stat} stat The stat.
 */
const notifyStatEvent = function(stat) {
  statEventTarget.dispatchEvent(new StatEvent(statEventTarget, stat));
};
exports.notifyStatEvent = notifyStatEvent;

/**
 * Returns the singleton event target for stat events.
 * @return {!EventTarget} The event target for stat events.
 */
const getStatEventTarget = function() {
  return statEventTarget;
};
exports.getStatEventTarget = getStatEventTarget;

/**
 * Singleton event target for firing stat events
 * @type {!EventTarget}
 * @private
 */
const statEventTarget = new EventTarget();


/**
 * Stat Event that fires when things of interest happen that may be useful for
 * applications to know about for stats or debugging purposes. This event fires
 * on the EventTarget returned by getStatEventTarget.
 */
const STAT_EVENT = 'statevent';
exports.STAT_EVENT = STAT_EVENT;


/**
 * Event class for goog.net.BrowserChannel.Event.STAT_EVENT
 * @final
 */
class StatEvent extends Event {
  /**
   * @param {EventTarget} eventTarget The stat event target for
   *    the browser channel.
   * @param {Stat} stat The stat.
   */
  constructor(eventTarget, stat) {
    super(STAT_EVENT, eventTarget);

    /**
     * The stat
     * @type {Stat}
     */
    this.stat = stat;
  }
}
exports.StatEvent = StatEvent;
