/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for animationFrame. */

goog.module('goog.dom.AnimationFrameTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const animationFrame = goog.require('goog.dom.animationFrame');
const testSuite = goog.require('goog.testing.testSuite');

const NEXT_FRAME = MockClock.REQUEST_ANIMATION_FRAME_TIMEOUT;
let mockClock;
let t0;
let t1;

let result;

testSuite({
  setUp() {
    mockClock = new MockClock(true);
    result = '';
    t0 = animationFrame.createTask({
      measure: function() {
        result += 'me0';
      },
      mutate: function() {
        result += 'mu0';
      },
    });
    t1 = animationFrame.createTask({
      measure: function() {
        result += 'me1';
      },
      mutate: function() {
        result += 'mu1';
      },
    });
    assertEquals('', result);
  },

  tearDown() {
    mockClock.dispose();
  },

  testCreateTask_one() {
    t0();
    assertEquals('', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0mu0', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0mu0', result);
    t0();
    t0();  // Should do nothing.
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0mu0me0mu0', result);
  },

  testCreateTask_onlyMutate() {
    t0 = animationFrame.createTask({
      mutate: function() {
        result += 'mu0';
      }
    });
    t0();
    assertEquals('', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('mu0', result);
  },

  testCreateTask_onlyMeasure() {
    t0 = animationFrame.createTask({
      mutate: function() {
        result += 'me0';
      }
    });
    t0();
    assertEquals('', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0', result);
  },

  testCreateTask_two() {
    t0();
    t1();
    assertEquals('', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0me1mu0mu1', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0me1mu0mu1', result);
    t0();
    t1();
    t0();
    t1();
    mockClock.tick(NEXT_FRAME);
    assertEquals('me0me1mu0mu1me0me1mu0mu1', result);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCreateTask_recurse() {
    let stop = false;
    const recurse = animationFrame.createTask({
      measure: function() {
        if (!stop) {
          recurse();
        }
        result += 're0';
      },
      mutate: function() {
        result += 'ru0';
      },
    });
    recurse();
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0re0ru0', result);
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0re0ru0re0ru0', result);
    t0();
    stop = true;
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0re0ru0re0ru0re0me0ru0mu0', result);

    // Recursion should have stopped now.
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0re0ru0re0ru0re0me0ru0mu0', result);
    assertFalse(animationFrame.requestedFrame_);
    mockClock.tick(NEXT_FRAME);
    assertEquals('re0ru0re0ru0re0ru0re0me0ru0mu0', result);
    assertFalse(animationFrame.requestedFrame_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCreateTask_recurseTwoMethodsWithState() {
    let stop = false;
    const recurse1 = animationFrame.createTask({
      measure: function(state) {
        if (!stop) {
          recurse2();
        }
        result += 'r1e0';
        state.text = 'T0';
      },
      mutate: function(state) {
        result += 'r1u0' + state.text;
      },
    });
    const recurse2 = animationFrame.createTask({
      measure: function(state) {
        if (!stop) {
          recurse1();
        }
        result += 'r2e0';
        state.text = 'T1';
      },
      mutate: function(state) {
        result += 'r2u0' + state.text;
      },
    });

    /** @suppress {visibility} suppression added to enable type checking */
    const taskLength = animationFrame.tasks_[0].length;

    recurse1();
    mockClock.tick(NEXT_FRAME);
    // Only recurse1 executed.
    assertEquals('r1e0r1u0T0', result);

    mockClock.tick(NEXT_FRAME);
    // Recurse2 executed and queueup recurse1.
    assertEquals('r1e0r1u0T0r2e0r2u0T1', result);

    mockClock.tick(NEXT_FRAME);
    // Recurse1 executed and queueup recurse2.
    assertEquals('r1e0r1u0T0r2e0r2u0T1r1e0r1u0T0', result);

    stop = true;
    mockClock.tick(NEXT_FRAME);
    // Recurse2 executed and should have stopped.
    assertEquals('r1e0r1u0T0r2e0r2u0T1r1e0r1u0T0r2e0r2u0T1', result);
    assertFalse(animationFrame.requestedFrame_);

    mockClock.tick(NEXT_FRAME);
    assertEquals('r1e0r1u0T0r2e0r2u0T1r1e0r1u0T0r2e0r2u0T1', result);
    assertFalse(animationFrame.requestedFrame_);

    mockClock.tick(NEXT_FRAME);
    assertEquals('r1e0r1u0T0r2e0r2u0T1r1e0r1u0T0r2e0r2u0T1', result);
    assertFalse(animationFrame.requestedFrame_);
  },

  testCreateTask_args() {
    const context = {context: true};
    const s = animationFrame.createTask(
        {
          measure: function(state) {
            assertEquals(context, this);
            assertUndefined(state.foo);
            state.foo = 'foo';
          },
          mutate: function(state) {
            assertEquals(context, this);
            result += state.foo;
          },
        },
        context);
    s();
    mockClock.tick(NEXT_FRAME);
    assertEquals('foo', result);

    const moreArgs = animationFrame.createTask({
      measure: function(event, state) {
        assertEquals('event', event);
        state.baz = 'baz';
      },
      mutate: function(event, state) {
        assertEquals('event', event);
        result += state.baz;
      },
    });
    moreArgs('event');
    mockClock.tick(NEXT_FRAME);
    assertEquals('foobaz', result);
  },

  testIsRunning() {
    let result = '';
    const task = animationFrame.createTask({
      measure: function() {
        result += 'me';
        assertTrue(animationFrame.isRunning());
      },
      mutate: function() {
        result += 'mu';
        assertTrue(animationFrame.isRunning());
      },
    });
    task();
    assertFalse(animationFrame.isRunning());
    mockClock.tick(NEXT_FRAME);
    assertFalse(animationFrame.isRunning());
    assertEquals('memu', result);
  },
});
