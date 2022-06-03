/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.css3.TransitionTest');
goog.setTestOnly();

const Css3Transition = goog.require('goog.fx.css3.Transition');
const MockClock = goog.require('goog.testing.MockClock');
const TagName = goog.require('goog.dom.TagName');
const Transition = goog.require('goog.fx.Transition');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const styleTransition = goog.require('goog.style.transition');
const testSuite = goog.require('goog.testing.testSuite');

let transition;
let element;
let mockClock;

function createTransition(element, duration) {
  return new Css3Transition(
      element, duration, {'opacity': 0}, {'opacity': 1},
      {property: 'opacity', duration: duration, timing: 'ease-in', delay: 0});
}

testSuite({
  setUp() {
    mockClock = new MockClock(true);
    element = dom.createElement(TagName.DIV);
    document.body.appendChild(element);
  },

  tearDown() {
    dispose(transition);
    dispose(mockClock);
    dom.removeNode(element);
  },

  testPlayEventFiredOnPlay() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);
    let handlerCalled = false;
    events.listen(transition, Transition.EventType.PLAY, () => {
      handlerCalled = true;
    });

    transition.play();
    assertTrue(handlerCalled);
  },

  testBeginEventFiredOnPlay() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);
    let handlerCalled = false;
    events.listen(transition, Transition.EventType.BEGIN, () => {
      handlerCalled = true;
    });

    transition.play();
    assertTrue(handlerCalled);
  },

  testFinishEventsFiredAfterFinish() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);
    let finishHandlerCalled = false;
    let endHandlerCalled = false;
    events.listen(transition, Transition.EventType.FINISH, () => {
      finishHandlerCalled = true;
    });
    events.listen(transition, Transition.EventType.END, () => {
      endHandlerCalled = true;
    });

    transition.play();

    mockClock.tick(10000);

    assertTrue(finishHandlerCalled);
    assertTrue(endHandlerCalled);
  },

  testEventsWhenTransitionIsUnsupported() {
    if (styleTransition.isSupported()) return;

    transition = createTransition(element, 10);

    let stopHandlerCalled = false;
    let endHandlerCalled = false;
    let finishHandlerCalled = false;

    let beginHandlerCalled = false;
    let playHandlerCalled = false;

    events.listen(transition, Transition.EventType.BEGIN, () => {
      beginHandlerCalled = true;
    });
    events.listen(transition, Transition.EventType.PLAY, () => {
      playHandlerCalled = true;
    });
    events.listen(transition, Transition.EventType.FINISH, () => {
      finishHandlerCalled = true;
    });
    events.listen(transition, Transition.EventType.END, () => {
      endHandlerCalled = true;
    });
    events.listen(transition, Transition.EventType.STOP, () => {
      stopHandlerCalled = true;
    });

    assertFalse(transition.play());

    assertTrue(beginHandlerCalled);
    assertTrue(playHandlerCalled);
    assertTrue(endHandlerCalled);
    assertTrue(finishHandlerCalled);

    transition.stop();

    assertFalse(stopHandlerCalled);
  },

  testCallingStopDuringAnimationWorks() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);

    const stopHandler = recordFunction();
    const endHandler = recordFunction();
    const finishHandler = recordFunction();
    events.listen(transition, Transition.EventType.STOP, stopHandler);
    events.listen(transition, Transition.EventType.END, endHandler);
    events.listen(transition, Transition.EventType.FINISH, finishHandler);

    transition.play();
    mockClock.tick(1);
    transition.stop();
    assertEquals(1, stopHandler.getCallCount());
    assertEquals(1, endHandler.getCallCount());
    mockClock.tick(10000);
    assertEquals(0, finishHandler.getCallCount());
  },

  testCallingStopImmediatelyWorks() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);

    const stopHandler = recordFunction();
    const endHandler = recordFunction();
    const finishHandler = recordFunction();
    events.listen(transition, Transition.EventType.STOP, stopHandler);
    events.listen(transition, Transition.EventType.END, endHandler);
    events.listen(transition, Transition.EventType.FINISH, finishHandler);

    transition.play();
    transition.stop();
    assertEquals(1, stopHandler.getCallCount());
    assertEquals(1, endHandler.getCallCount());
    mockClock.tick(10000);
    assertEquals(0, finishHandler.getCallCount());
  },

  testCallingStopAfterAnimationDoesNothing() {
    if (!styleTransition.isSupported()) return;

    transition = createTransition(element, 10);

    const stopHandler = recordFunction();
    const endHandler = recordFunction();
    const finishHandler = recordFunction();
    events.listen(transition, Transition.EventType.STOP, stopHandler);
    events.listen(transition, Transition.EventType.END, endHandler);
    events.listen(transition, Transition.EventType.FINISH, finishHandler);

    transition.play();
    mockClock.tick(10000);
    transition.stop();
    assertEquals(0, stopHandler.getCallCount());
    assertEquals(1, endHandler.getCallCount());
    assertEquals(1, finishHandler.getCallCount());
  },
});
