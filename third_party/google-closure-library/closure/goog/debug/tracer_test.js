/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.TraceTest');
goog.setTestOnly();

const StopTraceDetail = goog.requireType('goog.debug.StopTraceDetail');
const Trace = goog.require('goog.debug.Trace');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/** @type {!Function} */
const recorder = recordFunction();
/** @const {!StopTraceDetail} */
const TRACE_CANCELLED = {
  wasCancelled: true,
};
/** @const {!StopTraceDetail} */
const NORMAL_STOP = {};

/**
 * Checks if the actual log of a fake listener matches the expectations.
 * @param {!Array<!Array<*>>} expected The expected log from the fake listener.
 * @param {!Function} recorder The output of `recordFunction` for logging all
 *     calls to the fake listener.
 * @return {boolean} True if the recorder's log is expected.
 * @suppress {strictMissingProperties,missingReturn} suppression added to enable
 * type checking
 */
function validateRecordedListener(expected, recorder) {
  assertObjectEquals(
      expected,
      googArray.map(recorder.getCalls(), (call) => call.getArguments()));
}

testSuite({
  /**
     @suppress {strictMissingProperties,checkTypes} suppression added to enable
     type checking
   */
  setUp() {
    Trace.initCurrentTrace();
    Trace.removeAllListeners();
    recorder.reset();
  },

  testProperEventReleaseViaResetForComment() {
    Trace.startTracer('foo');
    // The Start event and its id are released due to calling the reset method.
    Trace.clearCurrentTrace();

    // Recycling the last Start event.
    Trace.addComment('abc');
    Trace.startTracer('foo');
    Trace.clearCurrentTrace();

    const t1 = Trace.startTracer('f1');
    const t2 = Trace.startTracer('f2');
    assertNotEquals('The trace ids cannot repeat.', t1, t2);
  },

  testProperEventReleaseViaThresholdForComment() {
    const t3 = Trace.startTracer('foo');
    // The Start event and its id are released due to 1000ms threshold.
    Trace.stopTracer(t3, 1000);

    // Recycling the last Start event.
    Trace.addComment('abc');
    Trace.startTracer('foo');
    Trace.clearCurrentTrace();

    const t1 = Trace.startTracer('f1');
    const t2 = Trace.startTracer('f2');
    assertNotEquals('The trace ids cannot repeat.', t1, t2);
  },

  testProperEventReleaseViaResetForStop() {
    Trace.startTracer('foo');
    Trace.startTracer('foo');
    // The Start events and their ids are released because of reseting.
    Trace.clearCurrentTrace();

    // Recycling the last two Start events.
    const t0 = Trace.startTracer('foo');
    Trace.stopTracer(t0);
    Trace.clearCurrentTrace();

    const t1 = Trace.startTracer('fa');
    Trace.startTracer('fb');
    const t2 = Trace.startTracer('fc');
    // No id is repeated.
    assertNotEquals('The trace ids cannot repeat.', t1, t2);
  },

  testProperEventReleaseViaThresholdForStop() {
    let t1 = Trace.startTracer('f1');
    let t2 = Trace.startTracer('f2');
    // The Start events and their ids are released due to 1000ms threshold.
    Trace.stopTracer(t2, 1000);
    Trace.stopTracer(t1, 1000);

    // Recycling the last two Start events.
    const t0 = Trace.startTracer('foo');
    Trace.stopTracer(t0);
    Trace.clearCurrentTrace();

    t1 = Trace.startTracer('fa');
    Trace.startTracer('fb');
    t2 = Trace.startTracer('fc');
    // No id is repeated.
    assertNotEquals('The trace ids cannot repeat.', t1, t2);
  },

  testTracer() {
    const t = Trace.startTracer('foo');
    let sum = 0;
    for (let i = 0; i < 100000; i++) {
      sum += i;
    }
    Trace.stopTracer(t);
    const trace = Trace.getFormattedTrace();
    const lines = trace.split('\n');
    assertEquals(8, lines.length);
    assertNotNull(lines[0].match(/^\s*\d+\.\d+\s+Start\s+foo$/));
    assertNotNull(lines[1].match(/^\s*\d+\s+\d+\.\d+\s+Done\s+\d+ ms\s+foo$/));
  },

  testListenerTooManyOpenTraces() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
    });
    const expected = [];
    const openTraces = [];
    for (let i = 0; 2 * i <= Trace.MAX_TRACE_SIZE; i++) {
      const t = Trace.startTracer('trace');
      expected.push(['start', t, 'trace']);
      openTraces.push(t);
    }
    // Triggering the giant thread warning to clear open traces.
    const t = Trace.startTracer('last');
    for (let j = 0; 2 * j <= Trace.MAX_TRACE_SIZE; j++) {
      expected.push(['stop', openTraces[j], TRACE_CANCELLED]);
    }
    expected.push(['start', t, 'last']);
    validateRecordedListener(expected, recorder);
  },

  testListenerGiantThread() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
    });
    const t1 = Trace.startTracer('first');
    const expected = [['start', t1, 'first']];
    let t;
    for (let i = 0; 2 * i < Trace.MAX_TRACE_SIZE; i++) {
      t = Trace.startTracer('trace');
      Trace.stopTracer(t);
      expected.push(['start', t, 'trace'], ['stop', t, NORMAL_STOP]);
    }
    // Triggering the giant thread warning.
    const t2 = Trace.startTracer('last');
    expected.push(['start', t2, 'last']);
    // Make sure that the last id of the giant thread is released after
    // clearing.
    assertEquals('The last id is not recycled!', t, t2);
    // Make sure that t1 and t2 are not stopped/cancelled.
    validateRecordedListener(expected, recorder);
  },

  testListenerReset() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
    });
    const t1 = Trace.startTracer('1st');
    const t2 = Trace.startTracer('2nd');
    Trace.stopTracer(t2);
    const t3 = Trace.startTracer('3rd');
    // Forcing t1 and t3 to be cancelled.
    Trace.clearCurrentTrace();

    const expected = [
      ['start', t1, '1st'],
      ['start', t2, '2nd'],
      ['stop', t2, NORMAL_STOP],
      ['start', t3, '3rd'],
      ['stop', t1, TRACE_CANCELLED],
      ['stop', t3, TRACE_CANCELLED],
    ];
    validateRecordedListener(expected, recorder);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRecord() {
    /** @type {number} */
    const a = 10;
    Trace.addTraceCallbacks(a);
  },

  testListenerStopTracerSilence() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
      comment: goog.partial(recorder, 'comment'),
    });
    const t = Trace.startTracer('first');
    // 1000ms should be enough for silencing the tracer.
    Trace.stopTracer(t, 1000);
    validateRecordedListener(
        [['start', t, 'first'], ['stop', t, NORMAL_STOP]], recorder);
  },

  testListenerStartTracerType() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
      comment: goog.partial(recorder, 'comment'),
    });
    const t = Trace.startTracer('first', 'New Type');
    Trace.stopTracer(t);
    validateRecordedListener(
        [['start', t, '[New Type] first'], ['stop', t, NORMAL_STOP]], recorder);
  },

  testListenerCommentTracerType() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
      comment: goog.partial(recorder, 'comment'),
    });
    Trace.addComment('foo', 'bar');
    validateRecordedListener([['comment', '[bar] foo']], recorder);
  },

  testListenerCommentTracerTime() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
      comment: goog.partial(recorder, 'comment'),
    });
    const timestamp = goog.now() - 10;
    Trace.addComment('first', null, timestamp);
    validateRecordedListener([['comment', 'first', timestamp]], recorder);
  },

  testListenerCommentTracerAlone() {
    Trace.addTraceCallbacks({
      comment: goog.partial(recorder, 'comment'),
    });
    Trace.addComment('foo');
    validateRecordedListener([['comment', 'foo']], recorder);
  },

  testListenerCommentTracerNoLog() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
    });
    Trace.addComment('foo');
    validateRecordedListener([], recorder);
  },

  testListenerStartStopTracerOnly() {
    Trace.addTraceCallbacks({
      start: goog.partial(recorder, 'start'),
      stop: goog.partial(recorder, 'stop'),
    });
    const t = Trace.startTracer('bar');
    Trace.stopTracer(t);
    validateRecordedListener(
        [['start', t, 'bar'], ['stop', t, NORMAL_STOP]], recorder);
  },

  testTwoListeners() {
    const r1 = recordFunction();
    Trace.addTraceCallbacks({
      start: goog.partial(r1, 'start'),
      stop: goog.partial(r1, 'stop'),
    });
    const t0 = Trace.startTracer('first');
    Trace.stopTracer(t0);
    const expected1 = [['start', t0, 'first'], ['stop', t0, NORMAL_STOP]];
    validateRecordedListener(expected1, r1);

    const r2 = recordFunction();
    Trace.addTraceCallbacks({
      start: goog.partial(r2, 'start'),
      stop: goog.partial(r2, 'stop'),
      comment: goog.partial(r2, 'comment'),
    });
    const t1 = Trace.startTracer('second', 'XType');
    const t2 = Trace.startTracer('third');
    Trace.addComment('NoTime');
    const currentTime = goog.now();
    Trace.addComment('WithTime', null, currentTime);
    Trace.addComment('NoTime', 'YType');
    Trace.stopTracer(t2);
    Trace.stopTracer(t1);

    const expected2 = [
      ['start', t1, '[XType] second'],
      ['start', t2, 'third'],
      ['comment', 'NoTime'],
      ['comment', 'WithTime', currentTime],
      ['comment', '[YType] NoTime'],
      ['stop', t2, NORMAL_STOP],
      ['stop', t1, NORMAL_STOP],
    ];
    validateRecordedListener(expected2, r2);

    const expectedNoComment = [
      ['start', t0, 'first'],
      ['stop', t0, NORMAL_STOP],
      ['start', t1, '[XType] second'],
      ['start', t2, 'third'],
      ['stop', t2, NORMAL_STOP],
      ['stop', t1, NORMAL_STOP],
    ];
    validateRecordedListener(expectedNoComment, r1);
  },
});
