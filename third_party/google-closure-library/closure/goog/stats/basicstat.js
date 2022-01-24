/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A basic statistics tracker.
 */

goog.provide('goog.stats.BasicStat');

goog.require('goog.asserts');
goog.require('goog.log');
goog.require('goog.string.format');
goog.require('goog.structs.CircularBuffer');



/**
 * Tracks basic statistics over a specified time interval.
 *
 * Statistics are kept in a fixed number of slots, each representing
 * an equal portion of the time interval.
 *
 * Most methods optionally allow passing in the current time, so that
 * higher level stats can synchronize operations on multiple child
 * objects.  Under normal usage, the default of Date.now() should be
 * sufficient.
 *
 * @param {number} interval The stat interval, in milliseconds.
 * @constructor
 * @final
 */
goog.stats.BasicStat = function(interval) {
  'use strict';
  goog.asserts.assert(interval > 50);

  /**
   * The time interval that this statistic aggregates over.
   * @type {number}
   * @private
   */
  this.interval_ = interval;

  /**
   * The number of milliseconds in each slot.
   * @type {number}
   * @private
   */
  this.slotInterval_ = Math.floor(interval / goog.stats.BasicStat.NUM_SLOTS_);

  /**
   * The array of slots.
   * @type {goog.structs.CircularBuffer}
   * @private
   */
  this.slots_ =
      new goog.structs.CircularBuffer(goog.stats.BasicStat.NUM_SLOTS_);
};


/**
 * The number of slots. This value limits the accuracy of the get()
 * method to (this.interval_ / NUM_SLOTS).  A 1-minute statistic would
 * be accurate to within 2 seconds.
 * @type {number}
 * @private
 */
goog.stats.BasicStat.NUM_SLOTS_ = 50;


/**
 * @type {goog.log.Logger}
 * @private
 */
goog.stats.BasicStat.prototype.logger_ =
    goog.log.getLogger('goog.stats.BasicStat');


/**
 * @return {number} The interval which over statistics are being
 *     accumulated, in milliseconds.
 */
goog.stats.BasicStat.prototype.getInterval = function() {
  'use strict';
  return this.interval_;
};


/**
 * Increments the count of this statistic by the specified amount.
 *
 * @param {number} amt The amount to increase the count by.
 * @param {number=} opt_now The time, in milliseconds, to be treated
 *     as the "current" time.  The current time must always be greater
 *     than or equal to the last time recorded by this stat tracker.
 */
goog.stats.BasicStat.prototype.incBy = function(amt, opt_now) {
  'use strict';
  const now = opt_now ? opt_now : Date.now();
  this.checkForTimeTravel_(now);
  let slot = /** @type {goog.stats.BasicStat.Slot_} */ (this.slots_.getLast());
  if (!slot || now >= slot.end) {
    slot = new goog.stats.BasicStat.Slot_(this.getSlotBoundary_(now));
    this.slots_.add(slot);
  }
  slot.count += amt;
  slot.min = Math.min(amt, slot.min);
  slot.max = Math.max(amt, slot.max);
};


/**
 * Returns the count of the statistic over its configured time
 * interval.
 * @param {number=} opt_now The time, in milliseconds, to be treated
 *     as the "current" time.  The current time must always be greater
 *     than or equal to the last time recorded by this stat tracker.
 * @return {number} The total count over the tracked interval.
 */
goog.stats.BasicStat.prototype.get = function(opt_now) {
  'use strict';
  return this.reduceSlots_(opt_now, function(sum, slot) {
    'use strict';
    return sum + slot.count;
  }, 0);
};


/**
 * Returns the magnitute of the largest atomic increment that occurred
 * during the watched time interval.
 * @param {number=} opt_now The time, in milliseconds, to be treated
 *     as the "current" time.  The current time must always be greater
 *     than or equal to the last time recorded by this stat tracker.
 * @return {number} The maximum count of this statistic.
 */
goog.stats.BasicStat.prototype.getMax = function(opt_now) {
  'use strict';
  return this.reduceSlots_(opt_now, function(max, slot) {
    'use strict';
    return Math.max(max, slot.max);
  }, Number.MIN_VALUE);
};


/**
 * Returns the magnitute of the smallest atomic increment that
 * occurred during the watched time interval.
 * @param {number=} opt_now The time, in milliseconds, to be treated
 *     as the "current" time.  The current time must always be greater
 *     than or equal to the last time recorded by this stat tracker.
 * @return {number} The minimum count of this statistic.
 */
goog.stats.BasicStat.prototype.getMin = function(opt_now) {
  'use strict';
  return this.reduceSlots_(opt_now, function(min, slot) {
    'use strict';
    return Math.min(min, slot.min);
  }, Number.MAX_VALUE);
};


/**
 * Passes each active slot into a function and accumulates the result.
 *
 * @param {number|undefined} now The current time, in milliseconds.
 * @param {function(number, goog.stats.BasicStat.Slot_): number} func
 *     The function to call for every active slot.  This function
 *     takes two arguments: the previous result and the new slot to
 *     include in the reduction.
 * @param {number} val The initial value for the reduction.
 * @return {number} The result of the reduction.
 * @private
 */
goog.stats.BasicStat.prototype.reduceSlots_ = function(now, func, val) {
  'use strict';
  now = now || Date.now();
  this.checkForTimeTravel_(now);
  let rval = val;
  const start = this.getSlotBoundary_(now) - this.interval_;
  for (let i = this.slots_.getCount() - 1; i >= 0; --i) {
    const slot = /** @type {goog.stats.BasicStat.Slot_} */ (this.slots_.get(i));
    if (slot.end <= start) {
      break;
    }
    rval = func(rval, slot);
  }
  return rval;
};


/**
 * Computes the end time for the slot that should contain the count
 * around the given time.  This method ensures that every bucket is
 * aligned on a "this.slotInterval_" millisecond boundary.
 * @param {number} time The time to compute a boundary for.
 * @return {number} The computed boundary.
 * @private
 */
goog.stats.BasicStat.prototype.getSlotBoundary_ = function(time) {
  'use strict';
  return this.slotInterval_ * (Math.floor(time / this.slotInterval_) + 1);
};


/**
 * Checks that time never goes backwards.  If it does (for example,
 * the user changes their system clock), the object state is cleared.
 * @param {number} now The current time, in milliseconds.
 * @private
 */
goog.stats.BasicStat.prototype.checkForTimeTravel_ = function(now) {
  'use strict';
  const slot =
      /** @type {goog.stats.BasicStat.Slot_} */ (this.slots_.getLast());
  if (slot) {
    const slotStart = slot.end - this.slotInterval_;
    if (now < slotStart) {
      goog.log.warning(
          this.logger_,
          goog.string.format(
              'Went backwards in time: now=%d, slotStart=%d.  Resetting state.',
              now, slotStart));
      this.reset_();
    }
  }
};


/**
 * Clears any statistics tracked by this object, as though it were
 * freshly created.
 * @private
 */
goog.stats.BasicStat.prototype.reset_ = function() {
  'use strict';
  this.slots_.clear();
};



/**
 * A struct containing information for each sub-interval.
 * @param {number} end The end time for this slot, in milliseconds.
 * @constructor
 * @private
 */
goog.stats.BasicStat.Slot_ = function(end) {
  'use strict';
  /**
   * End time of this slot, exclusive.
   * @type {number}
   */
  this.end = end;
};


/**
 * Aggregated count within this slot.
 * @type {number}
 */
goog.stats.BasicStat.Slot_.prototype.count = 0;


/**
 * The smallest atomic increment of the count within this slot.
 * @type {number}
 */
goog.stats.BasicStat.Slot_.prototype.min = Number.MAX_VALUE;


/**
 * The largest atomic increment of the count within this slot.
 * @type {number}
 */
goog.stats.BasicStat.Slot_.prototype.max = Number.MIN_VALUE;
