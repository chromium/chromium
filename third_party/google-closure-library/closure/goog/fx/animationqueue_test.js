/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.AnimationQueueTest');
goog.setTestOnly();

const Animation = goog.require('goog.fx.Animation');
const AnimationParallelQueue = goog.require('goog.fx.AnimationParallelQueue');
const AnimationSerialQueue = goog.require('goog.fx.AnimationSerialQueue');
const MockClock = goog.require('goog.testing.MockClock');
const Transition = goog.require('goog.fx.Transition');
const events = goog.require('goog.events');
const fxAnim = goog.require('goog.fx.anim');
const testSuite = goog.require('goog.testing.testSuite');

let clock;

testSuite({
  setUpPage() {
    clock = new MockClock(true);
    fxAnim.setAnimationWindow(null);
  },

  tearDownPage() {
    clock.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testParallelEvents() {
    const anim = new AnimationParallelQueue();
    anim.add(new Animation([0], [100], 200));
    anim.add(new Animation([0], [100], 400));
    anim.add(new Animation([0], [100], 600));

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());

    let beginEvents = 0;
    let pauseEvents = 0;
    let playEvents = 0;
    let resumeEvents = 0;

    let endEvents = 0;
    let finishEvents = 0;
    let stopEvents = 0;

    events.listen(anim, Transition.EventType.PLAY, () => {
      ++playEvents;
    });
    events.listen(anim, Transition.EventType.BEGIN, () => {
      ++beginEvents;
    });
    events.listen(anim, Transition.EventType.RESUME, () => {
      ++resumeEvents;
    });
    events.listen(anim, Transition.EventType.PAUSE, () => {
      ++pauseEvents;
    });
    events.listen(anim, Transition.EventType.END, () => {
      ++endEvents;
    });
    events.listen(anim, Transition.EventType.STOP, () => {
      ++stopEvents;
    });
    events.listen(anim, Transition.EventType.FINISH, () => {
      ++finishEvents;
    });

    // PLAY, BEGIN
    anim.play();
    // No queue events.
    clock.tick(100);
    // PAUSE
    anim.pause();
    // No queue events
    clock.tick(200);
    // PLAY, RESUME
    anim.play();
    // No queue events.
    clock.tick(400);
    // END, STOP
    anim.stop();
    // PLAY, BEGIN
    anim.play();
    // No queue events.
    clock.tick(400);
    // END, FINISH
    clock.tick(200);

    // Make sure the event counts are right.
    assertEquals(3, playEvents);
    assertEquals(2, beginEvents);
    assertEquals(1, resumeEvents);
    assertEquals(1, pauseEvents);
    assertEquals(2, endEvents);
    assertEquals(1, stopEvents);
    assertEquals(1, finishEvents);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerialEvents() {
    const anim = new AnimationSerialQueue();
    anim.add(new Animation([0], [100], 100));
    anim.add(new Animation([0], [100], 200));
    anim.add(new Animation([0], [100], 300));

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());

    let beginEvents = 0;
    let pauseEvents = 0;
    let playEvents = 0;
    let resumeEvents = 0;

    let endEvents = 0;
    let finishEvents = 0;
    let stopEvents = 0;

    events.listen(anim, Transition.EventType.PLAY, () => {
      ++playEvents;
    });
    events.listen(anim, Transition.EventType.BEGIN, () => {
      ++beginEvents;
    });
    events.listen(anim, Transition.EventType.RESUME, () => {
      ++resumeEvents;
    });
    events.listen(anim, Transition.EventType.PAUSE, () => {
      ++pauseEvents;
    });
    events.listen(anim, Transition.EventType.END, () => {
      ++endEvents;
    });
    events.listen(anim, Transition.EventType.STOP, () => {
      ++stopEvents;
    });
    events.listen(anim, Transition.EventType.FINISH, () => {
      ++finishEvents;
    });

    // PLAY, BEGIN
    anim.play();
    // No queue events.
    clock.tick(100);
    // PAUSE
    anim.pause();
    // No queue events
    clock.tick(200);
    // PLAY, RESUME
    anim.play();
    // No queue events.
    clock.tick(400);
    // END, STOP
    anim.stop();
    // PLAY, BEGIN
    anim.play(true);
    // No queue events.
    clock.tick(400);
    // END, FINISH
    clock.tick(200);

    // Make sure the event counts are right.
    assertEquals(3, playEvents);
    assertEquals(2, beginEvents);
    assertEquals(1, resumeEvents);
    assertEquals(1, pauseEvents);
    assertEquals(2, endEvents);
    assertEquals(1, stopEvents);
    assertEquals(1, finishEvents);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testParallelPause() {
    const anim = new AnimationParallelQueue();
    anim.add(new Animation([0], [100], 100));
    anim.add(new Animation([0], [100], 200));
    anim.add(new Animation([0], [100], 300));

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());

    anim.play();

    assertTrue(anim.queue[0].isPlaying());
    assertTrue(anim.queue[1].isPlaying());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    clock.tick(100);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPlaying());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    anim.pause();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPaused());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    clock.tick(200);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPaused());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    anim.play();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPlaying());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    clock.tick(100);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    anim.pause();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    clock.tick(200);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    anim.play();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    clock.tick(100);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerialPause() {
    const anim = new AnimationSerialQueue();
    anim.add(new Animation([0], [100], 100));
    anim.add(new Animation([0], [100], 200));
    anim.add(new Animation([0], [100], 300));

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());

    anim.play();

    assertTrue(anim.queue[0].isPlaying());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isPlaying());

    clock.tick(100);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPlaying());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isPlaying());

    anim.pause();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPaused());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isPaused());

    clock.tick(400);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPaused());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isPaused());

    anim.play();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isPlaying());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isPlaying());

    clock.tick(200);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPlaying());
    assertTrue(anim.isPlaying());

    anim.pause();

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    clock.tick(300);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isPaused());
    assertTrue(anim.isPaused());

    anim.play();

    clock.tick(300);

    assertTrue(anim.queue[0].isStopped());
    assertTrue(anim.queue[1].isStopped());
    assertTrue(anim.queue[2].isStopped());
    assertTrue(anim.isStopped());
  },
});
