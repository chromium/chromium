/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.async.DebouncerTest');
goog.setTestOnly();

const Debouncer = goog.require('goog.async.Debouncer');
const MockClock = goog.require('goog.testing.MockClock');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testDebouncerCommandSequences() {
    // Encoded sequences of commands to perform mapped to expected # of calls.
    //   f: fire
    //   w: wait (for the debouncing timer to elapse)
    //   p: pause
    //   r: resume
    //   s: stop
    const expectedCommandSequenceCalls = {
      'f': 0,
      'ff': 0,
      'fw': 1,
      'ffw': 1,
      'fpr': 0,
      'fsf': 0,
      'fsw': 0,
      'fprw': 1,
      'fpwr': 1,
      'fsfw': 1,
      'fswf': 0,
      'fprfw': 1,
      'fprsw': 0,
      'fpswr': 0,
      'fpwfr': 0,
      'fpwsr': 0,
      'fswfw': 1,
      'fpswrw': 0,
      'fpwfrw': 1,
      'fpwsfr': 0,
      'fpwsrw': 0,
      'fspwrw': 0,
      'fpwsfrw': 1,
      'ffwfwfffw': 3,
    };
    const interval = 500;

    const mockClock = new MockClock(true);
    for (let commandSequence in expectedCommandSequenceCalls) {
      const recordFn = recordFunction();
      const debouncer = new Debouncer(recordFn, interval);

      for (let i = 0; i < commandSequence.length; ++i) {
        switch (commandSequence[i]) {
          case 'f':
            debouncer.fire();
            break;
          case 'w':
            mockClock.tick(interval);
            break;
          case 'p':
            debouncer.pause();
            break;
          case 'r':
            debouncer.resume();
            break;
          case 's':
            debouncer.stop();
            break;
        }
      }

      const expectedCalls = expectedCommandSequenceCalls[commandSequence];
      assertEquals(
          `Expected ${expectedCalls} calls for command sequence "` +
              commandSequence + '" (' +
              Array.prototype.map
                  .call(
                      commandSequence,
                      command => {
                        switch (command) {
                          case 'f':
                            return 'fire';
                          case 'w':
                            return 'wait';
                          case 'p':
                            return 'pause';
                          case 'r':
                            return 'resume';
                          case 's':
                            return 'stop';
                        }
                      })
                  .join(' -> ') +
              ')',
          expectedCalls, recordFn.getCallCount());
      debouncer.dispose();
    }
    mockClock.uninstall();
  },

  testDebouncerScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'y': 0};
    const debouncer = new Debouncer(function() {
      ++this['y'];
    }, interval, x);
    debouncer.fire();
    assertEquals(0, x['y']);

    mockClock.tick(interval);
    assertEquals(1, x['y']);

    mockClock.uninstall();
  },

  testDebouncerArgumentBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    let calls = 0;
    const debouncer = new Debouncer((a, b, c) => {
      ++calls;
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval);

    debouncer.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(1, calls);

    // fire should always pass the last arguments passed to it into the
    // decorated function, even if called multiple times.
    debouncer.fire();
    mockClock.tick(interval / 2);
    debouncer.fire(8, null, true);
    debouncer.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, calls);

    mockClock.uninstall();
  },

  testDebouncerArgumentAndScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'calls': 0};
    const debouncer = new Debouncer(function(a, b, c) {
      ++this['calls'];
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval, x);

    debouncer.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(1, x['calls']);

    // fire should always pass the last arguments passed to it into the
    // decorated function, even if called multiple times.
    debouncer.fire();
    mockClock.tick(interval / 2);
    debouncer.fire(8, null, true);
    debouncer.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, x['calls']);

    mockClock.uninstall();
  },
});
