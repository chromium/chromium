/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the Tracer class and associated classes.
 *
 * @see ../demos/tracer.html
 * @suppress {strictMissingProperties}
 */

goog.provide('goog.debug.StopTraceDetail');
goog.provide('goog.debug.Trace');

goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.debug');
goog.require('goog.iter');
goog.require('goog.log');
goog.require('goog.structs.Map');
goog.require('goog.structs.SimplePool');



/**
 * Class used for singleton goog.debug.Trace.  Used for timing slow points in
 * the code. Based on the java Tracer class but optimized for javascript.
 * See com.google.common.tracing.Tracer.
 * It is also possible to bridge from this class to other tracer classes via
 * adding listeners.
 * @constructor
 * @private
 */
goog.debug.Trace_ = function() {
  'use strict';
  /**
   * Events in order.
   * @private {!Array<!goog.debug.Trace_.Event_>}
   */
  this.events_ = [];

  /**
   * Outstanding events that have started but haven't yet ended. The keys are
   * numeric ids and the values are goog.debug.Trace_.Event_ objects.
   * @private {!goog.structs.Map<number, !goog.debug.Trace_.Event_>}
   */
  this.outstandingEvents_ = new goog.structs.Map();

  /**
   * Start time of the event trace
   * @private {number}
   */
  this.startTime_ = 0;

  /**
   * Cummulative overhead of calls to startTracer
   * @private {number}
   */
  this.tracerOverheadStart_ = 0;

  /**
   * Cummulative overhead of calls to endTracer
   * @private {number}
   */
  this.tracerOverheadEnd_ = 0;

  /**
   * Cummulative overhead of calls to addComment
   * @private {number}
   */
  this.tracerOverheadComment_ = 0;

  /**
   * Keeps stats on different types of tracers. The keys are strings and the
   * values are goog.debug.Stat
   * @private {!goog.structs.Map}
   */
  this.stats_ = new goog.structs.Map();

  /**
   * Total number of traces created in the trace.
   * @private {number}
   */
  this.tracerCount_ = 0;

  /**
   * Total number of comments created in the trace.
   * @private {number}
   */
  this.commentCount_ = 0;

  /**
   * Next id to use for the trace.
   * @private {number}
   */
  this.nextId_ = 1;

  /**
   * A pool for goog.debug.Trace_.Event_ objects so we don't keep creating and
   * garbage collecting these (which is very expensive in IE6).
   * @private {!goog.structs.SimplePool}
   */
  this.eventPool_ = new goog.structs.SimplePool(0, 4000);
  this.eventPool_.createObject = function() {
    'use strict';
    return new goog.debug.Trace_.Event_();
  };


  /**
   * A pool for goog.debug.Trace_.Stat_ objects so we don't keep creating and
   * garbage collecting these (which is very expensive in IE6).
   * @private {!goog.structs.SimplePool}
   */
  this.statPool_ = new goog.structs.SimplePool(0, 50);
  this.statPool_.createObject = function() {
    'use strict';
    return new goog.debug.Trace_.Stat_();
  };

  var self = this;

  /** @private {!goog.structs.SimplePool<number>} */
  this.idPool_ = new goog.structs.SimplePool(0, 2000);
  this.idPool_.setCreateObjectFn(function() {
    'use strict';
    return self.nextId_++;
  });

  /**
   * Default threshold below which a tracer shouldn't be reported
   * @private {number}
   */
  this.defaultThreshold_ = 3;

  /**
   * An object containing three callback functions to be called when starting or
   * stopping a trace, or creating a comment trace.
   * @private {!goog.debug.Trace_.TracerCallbacks}
   */
  this.traceCallbacks_ = {};
};


/**
 * Logger for the tracer
 * @private @const {?goog.log.Logger}
 */
goog.debug.Trace_.prototype.logger_ = goog.log.getLogger('goog.debug.Trace');


/**
 * Maximum size of the trace before we discard events
 * @type {number}
 */
goog.debug.Trace_.prototype.MAX_TRACE_SIZE = 1000;


/**
 * Event type supported by tracer
 * @enum {number}
 */
goog.debug.Trace_.EventType = {
  /**
   * Start event type
   */
  START: 0,

  /**
   * Stop event type
   */
  STOP: 1,

  /**
   * Comment event type
   */
  COMMENT: 2
};



/**
 * Class to keep track of a stat of a single tracer type. Stores the count
 * and cumulative time.
 * @constructor
 * @private
 */
goog.debug.Trace_.Stat_ = function() {
  'use strict';
  /**
   * Number of tracers
   * @type {number}
   */
  this.count = 0;

  /**
   * Cumulative time of traces
   * @type {number}
   */
  this.time = 0;
};


/**
 * @type {string|null|undefined}
 */
goog.debug.Trace_.Stat_.prototype.type;


/**
 * @return {string} A string describing the tracer stat.
 * @override
 */
goog.debug.Trace_.Stat_.prototype.toString = function() {
  'use strict';
  var sb = [];
  sb.push(
      this.type, ' ', this.count, ' (', Math.round(this.time * 10) / 10,
      ' ms)');
  return sb.join('');
};



/**
 * Private class used to encapsulate a single event, either the start or stop
 * of a tracer.
 * @constructor
 * @private
 */
goog.debug.Trace_.Event_ = function() {
  // the fields are different for different events - see usage in code
};


/**
 * @type {string|null|undefined}
 */
goog.debug.Trace_.Event_.prototype.type;


/**
 * @type {goog.debug.Trace_.EventType|undefined}
 */
goog.debug.Trace_.Event_.prototype.eventType;


/**
 * @type {number|undefined}
 */
goog.debug.Trace_.Event_.prototype.id;


/**
 * @type {string|undefined}
 */
goog.debug.Trace_.Event_.prototype.comment;

/**
 * @type {number|undefined}
 */
goog.debug.Trace_.Event_.prototype.eventTime;

/**
 * @type {number|undefined}
 */
goog.debug.Trace_.Event_.prototype.startTime;

/**
 * @type {number|undefined}
 */
goog.debug.Trace_.Event_.prototype.stopTime;


/**
 * Returns a formatted string for the event.
 * @param {number} startTime The start time of the trace to generate relative
 * times.
 * @param {number} prevTime The completion time of the previous event or -1.
 * @param {string} indent Extra indent for the message
 *     if there was no previous event.
 * @return {string} The formatted tracer string.
 */
goog.debug.Trace_.Event_.prototype.toTraceString = function(
    startTime, prevTime, indent) {
  'use strict';
  var sb = [];

  goog.asserts.assertNumber(
      this.eventTime, 'eventTime missing - call startTracer?');
  if (prevTime == -1) {
    sb.push('    ');
  } else {
    sb.push(goog.debug.Trace_.longToPaddedString_(this.eventTime - prevTime));
  }

  sb.push(' ', goog.debug.Trace_.formatTime_(this.eventTime - startTime));
  if (this.eventType == goog.debug.Trace_.EventType.START) {
    sb.push(' Start        ');
  } else if (this.eventType == goog.debug.Trace_.EventType.STOP) {
    sb.push(' Done ');
    goog.asserts.assertNumber(
        this.startTime, 'startTime missing - startTracer not called?');
    goog.asserts.assertNumber(
        this.stopTime, 'stopTime missing - stopTracer not called?');
    var delta = this.stopTime - this.startTime;
    sb.push(goog.debug.Trace_.longToPaddedString_(delta), ' ms ');
  } else {
    sb.push(' Comment      ');
  }

  sb.push(indent, this);
  return sb.join('');
};


/**
 * @return {string} A string describing the tracer event.
 * @override
 */
goog.debug.Trace_.Event_.prototype.toString = function() {
  'use strict';
  if (this.type == null) {
    return goog.asserts.assert(this.comment);
  } else {
    return '[' + this.type + '] ' + this.comment;
  }
};


/**
 * A class to specify the types of the callback functions used by
 * `addTraceCallbacks`.
 * @record
 */
goog.debug.Trace_.TracerCallbacks = function() {
  'use strict';
  /**
   * A callback function to be called at `startTrace` with two parameters:
   * a number as the started trace id and a string as the comment on the trace.
   * @type {function(number, string)|undefined}
   */
  this.start;
  /**
   * A callback function to be called when a trace should be stopped either at
   * `startTrace` or `clearOutstandingEvents_` with two parameters:
   * a number as the id of the trace being stopped and an object containing
   * extra information about stopping the trace (e.g. if it is cancelled).
   * @type {function(number, !goog.debug.StopTraceDetail)|undefined}
   */
  this.stop;
  /**
   * A callback function to be called at `addComment` with two parameters:
   * a string as the comment on the trace and an optional time stamp number (in
   * milliseconds since epoch) when the comment should be added as a trace.
   * @type {function(string, number=)|undefined}
   */
  this.comment;
};


/** @private @const {!goog.debug.StopTraceDetail} */
goog.debug.Trace_.TRACE_CANCELLED_ = {
  wasCancelled: true
};


/** @private @const {!goog.debug.StopTraceDetail} */
goog.debug.Trace_.NORMAL_STOP_ = {};


/**
 * A function that combines two function with the same parameters in a sequence.
 * @param {!Function|undefined} fn1 The first function to be combined.
 * @param {!Function|undefined} fn2 The second function to be combined.
 * @return {!Function|undefined} A function that calls the inputs in sequence.
 * @private
 */
goog.debug.Trace_.TracerCallbacks.sequence_ = function(fn1, fn2) {
  'use strict';
  return !fn1 ? fn2 :
      !fn2    ? fn1 :
                function() {
               'use strict';
               fn1.apply(undefined, arguments);
               fn2.apply(undefined, arguments);
             };
};


/**
 * Removes all registered callback functions. Mainly used for testing.
 */
goog.debug.Trace_.prototype.removeAllListeners = function() {
  'use strict';
  this.traceCallbacks_ = {};
};


/**
 * Adds up to three callback functions which are called on `startTracer`,
 * `stopTracer`, `clearOutstandingEvents_` and `addComment` in
 * order to bridge from the Closure tracer singleton object to any tracer class.
 * @param {!goog.debug.Trace_.TracerCallbacks} callbacks An object literal
 *   containing the callback functions.
 */
goog.debug.Trace_.prototype.addTraceCallbacks = function(callbacks) {
  'use strict';
  this.traceCallbacks_.start = goog.debug.Trace_.TracerCallbacks.sequence_(
      this.traceCallbacks_.start, callbacks.start);
  this.traceCallbacks_.stop = goog.debug.Trace_.TracerCallbacks.sequence_(
      this.traceCallbacks_.stop, callbacks.stop);
  this.traceCallbacks_.comment = goog.debug.Trace_.TracerCallbacks.sequence_(
      this.traceCallbacks_.comment, callbacks.comment);
};


/**
 * Add the ability to explicitly set the start time. This is useful for example
 * for measuring initial load time where you can set a variable as soon as the
 * main page of the app is loaded and then later call this function when the
 * Tracer code has been loaded.
 * @param {number} startTime The start time to set.
 */
goog.debug.Trace_.prototype.setStartTime = function(startTime) {
  'use strict';
  this.startTime_ = startTime;
};


/**
 * Initializes and resets the current trace
 * @param {number} defaultThreshold The default threshold below which the
 * tracer output will be suppressed. Can be overridden on a per-Tracer basis.
 */
goog.debug.Trace_.prototype.initCurrentTrace = function(defaultThreshold) {
  'use strict';
  this.reset(defaultThreshold);
};


/**
 * Clears the current trace
 */
goog.debug.Trace_.prototype.clearCurrentTrace = function() {
  'use strict';
  this.reset(0);
};


/**
 * Clears the open traces and calls stop callback for them.
 * @private
 */
goog.debug.Trace_.prototype.clearOutstandingEvents_ = function() {
  'use strict';
  if (this.traceCallbacks_.stop) {
    goog.iter.forEach(this.outstandingEvents_, function(startEvent) {
      'use strict';
      this.traceCallbacks_.stop(
          startEvent.id, goog.debug.Trace_.TRACE_CANCELLED_);
    }, this);
  }
  this.outstandingEvents_.clear();
};


/**
 * Resets the trace.
 * @param {number} defaultThreshold The default threshold below which the
 * tracer output will be suppressed. Can be overridden on a per-Tracer basis.
 */
goog.debug.Trace_.prototype.reset = function(defaultThreshold) {
  'use strict';
  this.defaultThreshold_ = defaultThreshold;

  this.clearOutstandingEvents_();
  this.releaseEvents_();
  this.startTime_ = goog.debug.Trace_.now();
  this.tracerOverheadStart_ = 0;
  this.tracerOverheadEnd_ = 0;
  this.tracerOverheadComment_ = 0;
  this.tracerCount_ = 0;
  this.commentCount_ = 0;

  var keys = this.stats_.getKeys();
  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    var stat = this.stats_.get(key);
    stat.count = 0;
    stat.time = 0;
    this.statPool_.releaseObject(/** @type {Object} */ (stat));
  }
  this.stats_.clear();
};


/**
 * @private
 */
goog.debug.Trace_.prototype.releaseEvents_ = function() {
  'use strict';
  for (var i = 0; i < this.events_.length; i++) {
    var event = this.events_[i];
    if (event.id) {  // Only start events have id.
      // Only release the start event and its id if it is already stopped - this
      // is to avoid having multiple traces with the same id.
      if (!this.outstandingEvents_.containsKey(event.id)) {
        this.idPool_.releaseObject(event.id);
        this.eventPool_.releaseObject(event);
      }
    } else {  // Release stop and comment events.
      this.eventPool_.releaseObject(event);
    }
  }
  this.events_.length = 0;
};


/**
 * Starts a tracer
 * @param {string} comment A comment used to identify the tracer. Does not
 *     need to be unique.
 * @param {string=} opt_type Type used to identify the tracer. If a Trace is
 *     given a type (the first argument to the constructor) and multiple Traces
 *     are done on that type then a "TOTAL line will be produced showing the
 *     total number of traces and the sum of the time
 *     ("TOTAL Database 2 (37 ms)" in our example). These traces should be
 *     mutually exclusive or else the sum won't make sense (the time will
 *     be double counted if the second starts before the first ends).
 * @return {number} The identifier for the tracer that should be passed to the
 *     the stopTracer method.
 */
goog.debug.Trace_.prototype.startTracer = function(comment, opt_type) {
  'use strict';
  var tracerStartTime = goog.debug.Trace_.now();
  var outstandingEventCount = this.outstandingEvents_.getCount();
  if (this.events_.length + outstandingEventCount > this.MAX_TRACE_SIZE) {
    // This is less likely and probably indicates that a lot of traces
    // aren't being closed. We want to avoid unnecessarily clearing
    // this though in case the events do eventually finish.
    if (outstandingEventCount > this.MAX_TRACE_SIZE / 2) {
      goog.log.warning(
          this.logger_, 'Giant thread trace. Clearing outstanding events.');
      this.clearOutstandingEvents_();
    }
    // This is the more likely case. This usually means that we
    // either forgot to clear the trace or else we are performing a
    // very large number of events
    if (this.events_.length > this.MAX_TRACE_SIZE / 2) {
      goog.log.warning(
          this.logger_, 'Giant thread trace. Clearing to avoid memory leak.');
      this.releaseEvents_();
    }
  }

  /** @const */
  var event =
      /** @type {!goog.debug.Trace_.Event_} */ (this.eventPool_.getObject());
  event.stopTime = undefined;
  event.eventType = goog.debug.Trace_.EventType.START;
  event.id = this.idPool_.getObject();
  event.comment = comment;
  event.type = opt_type;
  this.events_.push(event);
  this.outstandingEvents_.set(String(event.id), event);
  this.tracerCount_++;
  var now = goog.debug.Trace_.now();
  event.startTime = event.eventTime = now;
  this.tracerOverheadStart_ += now - tracerStartTime;
  if (this.traceCallbacks_.start) {
    this.traceCallbacks_.start(event.id, event.toString());
  }
  return event.id;
};


/**
 * Stops a tracer
 * @param {number|undefined|null} id The id of the tracer that is ending.
 * @param {number=} opt_silenceThreshold Threshold below which the tracer is
 *    silenced.
 * @return {?number} The elapsed time for the tracer or null if the tracer
 *    identitifer was not recognized.
 */
goog.debug.Trace_.prototype.stopTracer = function(id, opt_silenceThreshold) {
  'use strict';
  // this used to call goog.isDef(opt_silenceThreshold) but that causes an
  // object allocation in IE for some reason (doh!). The following code doesn't
  // cause an allocation
  var now = goog.debug.Trace_.now();
  var silenceThreshold;
  if (opt_silenceThreshold === 0) {
    silenceThreshold = 0;
  } else if (opt_silenceThreshold) {
    silenceThreshold = opt_silenceThreshold;
  } else {
    silenceThreshold = this.defaultThreshold_;
  }

  var startEvent = this.outstandingEvents_.get(String(id));
  if (startEvent == null) {
    return null;
  }
  goog.asserts.assertNumber(id);
  if (this.traceCallbacks_.stop) {
    this.traceCallbacks_.stop(Number(id), goog.debug.Trace_.NORMAL_STOP_);
  }

  this.outstandingEvents_.remove(String(id));

  var stopEvent;
  var elapsed = now - startEvent.startTime;
  if (elapsed < silenceThreshold) {
    var count = this.events_.length;
    for (var i = count - 1; i >= 0; i--) {
      var nextEvent = this.events_[i];
      if (nextEvent == startEvent) {
        this.events_.splice(i, 1);
        this.idPool_.releaseObject(startEvent.id);
        this.eventPool_.releaseObject(/** @type {Object} */ (startEvent));
        break;
      }
    }
  } else {
    stopEvent =
        /** @type {goog.debug.Trace_.Event_} */ (this.eventPool_.getObject());
    stopEvent.id = undefined;
    stopEvent.eventType = goog.debug.Trace_.EventType.STOP;
    stopEvent.startTime = startEvent.startTime;
    stopEvent.comment = startEvent.comment;
    stopEvent.type = startEvent.type;
    stopEvent.stopTime = stopEvent.eventTime = now;

    this.events_.push(stopEvent);
  }

  var type = startEvent.type;
  var stat = null;
  if (type) {
    stat = this.getStat_(type);
    stat.count++;
    stat.time += elapsed;
  }
  var tracerFinishTime = goog.debug.Trace_.now();
  this.tracerOverheadEnd_ += tracerFinishTime - now;
  return elapsed;
};


/**
 * Adds a comment to the trace. Makes it possible to see when a specific event
 * happened in relation to the traces.
 * @param {string} comment A comment that is inserted into the trace.
 * @param {?string=} opt_type Type used to identify the tracer. If a comment is
 *     given a type and multiple comments are done on that type then a "TOTAL
 *     line will be produced showing the total number of comments of that type.
 * @param {?number=} opt_timeStamp The timestamp to insert the comment. If not
 *    specified, the current time wil be used.
 */
goog.debug.Trace_.prototype.addComment = function(
    comment, opt_type, opt_timeStamp) {
  'use strict';
  var now = goog.debug.Trace_.now();
  var timeStamp = opt_timeStamp ? opt_timeStamp : now;

  var eventComment =
      /** @type {goog.debug.Trace_.Event_} */ (this.eventPool_.getObject());
  eventComment.startTime = undefined;
  eventComment.stopTime = undefined;
  eventComment.id = undefined;
  eventComment.eventType = goog.debug.Trace_.EventType.COMMENT;
  eventComment.eventTime = timeStamp;
  eventComment.type = opt_type;
  eventComment.comment = comment;
  this.commentCount_++;

  if (opt_timeStamp) {
    if (this.traceCallbacks_.comment) {
      this.traceCallbacks_.comment(eventComment.toString(), opt_timeStamp);
    }
    var numEvents = this.events_.length;
    for (var i = 0; i < numEvents; i++) {
      var event = this.events_[i];
      var eventTime = event.eventTime;

      goog.asserts.assertNumber(
          eventTime, 'eventTime undefined - call startTracer?');
      if (eventTime > timeStamp) {
        goog.array.insertAt(this.events_, eventComment, i);
        break;
      }
    }
    if (i == numEvents) {
      this.events_.push(eventComment);
    }
  } else {  // No time_stamp
    if (this.traceCallbacks_.comment) {
      this.traceCallbacks_.comment(eventComment.toString());
    }
    this.events_.push(eventComment);
  }

  var type = eventComment.type;
  if (type) {
    var stat = this.getStat_(type);
    stat.count++;
  }

  this.tracerOverheadComment_ += goog.debug.Trace_.now() - now;
};


/**
 * Gets a stat object for a particular type. The stat object is created if it
 * hasn't yet been.
 * @param {string} type The type of stat.
 * @return {goog.debug.Trace_.Stat_} The stat object.
 * @private
 */
goog.debug.Trace_.prototype.getStat_ = function(type) {
  'use strict';
  var stat = this.stats_.get(type);
  if (!stat) {
    stat = /** @type {goog.debug.Trace_.Event_} */ (this.statPool_.getObject());
    stat.type = type;
    this.stats_.set(type, stat);
  }
  return /** @type {goog.debug.Trace_.Stat_} */ (stat);
};


/**
 * Returns a formatted string for the current trace
 * @return {string} A formatted string that shows the timings of the current
 *     trace.
 */
goog.debug.Trace_.prototype.getFormattedTrace = function() {
  'use strict';
  return this.toString();
};


/**
 * Returns a formatted string that describes the thread trace.
 * @return {string} A formatted string.
 * @override
 */
goog.debug.Trace_.prototype.toString = function() {
  'use strict';
  var sb = [];
  var etime = -1;
  var indent = [];
  for (var i = 0; i < this.events_.length; i++) {
    var e = this.events_[i];
    if (e.eventType == goog.debug.Trace_.EventType.STOP) {
      indent.pop();
    }
    sb.push(' ', e.toTraceString(this.startTime_, etime, indent.join('')));
    etime = /** @type {number} */ (e.eventTime);
    sb.push('\n');
    if (e.eventType == goog.debug.Trace_.EventType.START) {
      indent.push('|  ');
    }
  }

  if (this.outstandingEvents_.getCount() != 0) {
    var now = goog.debug.Trace_.now();

    sb.push(' Unstopped timers:\n');
    goog.iter.forEach(this.outstandingEvents_, function(startEvent) {
      'use strict';
      sb.push(
          '  ', startEvent, ' (', now - startEvent.startTime,
          ' ms, started at ',
          goog.debug.Trace_.formatTime_(startEvent.startTime), ')\n');
    });
  }

  var statKeys = this.stats_.getKeys();
  for (var i = 0; i < statKeys.length; i++) {
    var stat = this.stats_.get(statKeys[i]);
    if (stat.count > 1) {
      sb.push(' TOTAL ', stat, '\n');
    }
  }

  sb.push(
      'Total tracers created ', this.tracerCount_, '\n',
      'Total comments created ', this.commentCount_, '\n', 'Overhead start: ',
      this.tracerOverheadStart_, ' ms\n', 'Overhead end: ',
      this.tracerOverheadEnd_, ' ms\n', 'Overhead comment: ',
      this.tracerOverheadComment_, ' ms\n');

  return sb.join('');
};


/**
 * Converts 'v' to a string and pads it with up to 3 spaces for
 * improved alignment. TODO there must be a better way
 * @param {number} v A number.
 * @return {string} A padded string.
 * @private
 */
goog.debug.Trace_.longToPaddedString_ = function(v) {
  'use strict';
  v = Math.round(v);
  // todo (pupius) - there should be a generic string in goog.string for this
  var space = '';
  if (v < 1000) space = ' ';
  if (v < 100) space = '  ';
  if (v < 10) space = '   ';
  return space + v;
};


/**
 * Return the sec.ms part of time (if time = "20:06:11.566",  "11.566
 * @param {number} time The time in MS.
 * @return {string} A formatted string as sec.ms'.
 * @private
 */
goog.debug.Trace_.formatTime_ = function(time) {
  'use strict';
  time = Math.round(time);
  var sec = (time / 1000) % 60;
  var ms = time % 1000;

  // TODO their must be a nicer way to get zero padded integers
  return String(100 + sec).substring(1, 3) + '.' +
      String(1000 + ms).substring(1, 4);
};


/**
 * Returns the current time. Done through a wrapper function so it can be
 * overridden by application code. Gmail has an ActiveX extension that provides
 * higher precision timing info.
 * @return {number} The current time in milliseconds.
 */
goog.debug.Trace_.now = function() {
  'use strict';
  return goog.now();
};


/**
 * Singleton trace object
 * @type {goog.debug.Trace_}
 */
goog.debug.Trace = new goog.debug.Trace_();


/**
 * The detail of calling the stop callback for a trace.
 * @record
 */
goog.debug.StopTraceDetail = function() {
  'use strict';
  /**
   * The trace should be stopped since it has been cancelled. Note that this
   * field is optional so, not-specifying it is like setting it to false.
   * @type {boolean|undefined}
   */
  this.wasCancelled;
};
