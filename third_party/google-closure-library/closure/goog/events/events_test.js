/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.eventsTest');
goog.setTestOnly();

const AssertionError = goog.require('goog.asserts.AssertionError');
const CaptureSimulationMode = goog.require('goog.events.CaptureSimulationMode');
const EntryPointMonitor = goog.require('goog.debug.EntryPointMonitor');
const ErrorHandler = goog.require('goog.debug.ErrorHandler');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const Listener = goog.require('goog.events.Listener');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const disposeAll = goog.require('goog.disposeAll');
const dom = goog.require('goog.dom');
const entryPointRegistry = goog.require('goog.debug.entryPointRegistry');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/** @suppress {visibility} suppression added to enable type checking */
const originalHandleBrowserEvent = events.handleBrowserEvent_;
let propertyReplacer;
let et1;
let et2;
let et3;

function dispatchClick(target) {
  if (target.click) {
    target.click();
  } else {
    const e = document.createEvent('MouseEvents');
    e.initMouseEvent(
        'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false,
        0, null);
    target.dispatchEvent(e);
  }
}

function runEventPropagationWithReentrantDispatch(useCapture) {
  const eventType = 'test-event-type';

  const child = et1;
  const parent = et2;
  child.setParentEventTarget(parent);

  const firstTarget = useCapture ? parent : child;
  const secondTarget = useCapture ? child : parent;

  const firstListener = (evt) => {
    if (evt.isFirstEvent) {
      // Fires another event of the same type the first time it is invoked.
      child.dispatchEvent(new GoogEvent(eventType));
    }
  };
  events.listen(firstTarget, eventType, firstListener, useCapture);

  const secondListener = recordFunction();
  events.listen(secondTarget, eventType, secondListener, useCapture);

  // Fire the first event.
  const firstEvent = new GoogEvent(eventType);
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  firstEvent.isFirstEvent = true;
  child.dispatchEvent(firstEvent);

  assertEquals(2, secondListener.getCallCount());
}

function runEventPropagationWhenListenerRemoved(useCapture) {
  const eventType = 'test-event-type';

  const child = et1;
  const parent = et2;
  child.setParentEventTarget(parent);

  const firstTarget = useCapture ? parent : child;
  const secondTarget = useCapture ? child : parent;

  const firstListener = recordFunction();
  const secondListener = recordFunction();
  events.listenOnce(firstTarget, eventType, firstListener, useCapture);
  events.listen(secondTarget, eventType, secondListener, useCapture);

  child.dispatchEvent(new GoogEvent(eventType));

  assertEquals(1, secondListener.getCallCount());
}

function runEventPropagationWhenListenerAdded(useCapture) {
  const eventType = 'test-event-type';

  const child = et1;
  const parent = et2;
  child.setParentEventTarget(parent);

  const firstTarget = useCapture ? parent : child;
  const secondTarget = useCapture ? child : parent;

  const firstListener = () => {
    events.listen(secondTarget, eventType, secondListener, useCapture);
  };
  const secondListener = recordFunction();
  events.listen(firstTarget, eventType, firstListener, useCapture);

  child.dispatchEvent(new GoogEvent(eventType));

  assertEquals(1, secondListener.getCallCount());
}

function runEventPropagationWhenListenerAddedAndRemoved(useCapture) {
  const eventType = 'test-event-type';

  const child = et1;
  const parent = et2;
  child.setParentEventTarget(parent);

  const firstTarget = useCapture ? parent : child;
  const secondTarget = useCapture ? child : parent;

  const firstListener = () => {
    events.listen(secondTarget, eventType, secondListener, useCapture);
  };
  const secondListener = recordFunction();
  events.listenOnce(firstTarget, eventType, firstListener, useCapture);

  child.dispatchEvent(new GoogEvent(eventType));

  assertEquals(1, secondListener.getCallCount());
}

testSuite({
  setUp() {
    et1 = new GoogEventTarget();
    et2 = new GoogEventTarget();
    et3 = new GoogEventTarget();
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    /** Use computed properties to avoid compiler checks of defines */
    events['CAPTURE_SIMULATION_MODE'] = CaptureSimulationMode.ON;
    /** @suppress {visibility} suppression added to enable type checking */
    events.handleBrowserEvent_ = originalHandleBrowserEvent;
    disposeAll(et1, et2, et3);
    events.removeAll(document.body);
    propertyReplacer.reset();
  },

  testProtectBrowserEventEntryPoint() {
    const errorHandlerFn = recordFunction();
    const errorHandler = new ErrorHandler(errorHandlerFn);

    events.protectBrowserEventEntryPoint(errorHandler);

    /** @suppress {visibility} suppression added to enable type checking */
    const browserEventHandler = recordFunction(events.handleBrowserEvent_);
    /** @suppress {visibility} suppression added to enable type checking */
    events.handleBrowserEvent_ = function() {
      try {
        browserEventHandler.apply(this, arguments);
      } catch (e) {
        // Ignored.
      }
    };

    const err = Error('test');
    const body = document.body;
    events.listen(body, EventType.CLICK, () => {
      throw err;
    });

    dispatchClick(body);

    assertEquals(
        'Error handler callback should be called.', 1,
        errorHandlerFn.getCallCount());
    assertEquals(err, errorHandlerFn.getLastCall().getArgument(0));

    assertEquals(1, browserEventHandler.getCallCount());
    const err2 = browserEventHandler.getLastCall().getError();
    assertNotNull(err2);
    assertTrue(err2 instanceof ErrorHandler.ProtectedFunctionError);
  },

  testSelfRemove() {
    const callback = () => {
      // This listener removes itself during event dispatching, so it
      // is marked as 'removed' but not actually removed until after event
      // dispatching ends.
      events.removeAll(et1, 'click');

      // Test that goog.events.getListener ignores events marked as 'removed'.
      assertNull(events.getListener(et1, 'click', callback));
    };
    events.listen(et1, 'click', callback);
    events.dispatchEvent(et1, 'click');
  },

  testMediaQueryList() {
    if (!window.matchMedia) return;

    const mql = window.matchMedia('(max-width: 640px)');
    const key = events.listen(mql, 'change', goog.nullFunction);

    // I don't know of any way to make it raise an event in a test.

    events.unlistenByKey(key);
  },

  testHasListener() {
    const div = dom.createElement(TagName.DIV);
    assertFalse(events.hasListener(div));

    const key = events.listen(div, 'click', () => {});
    assertTrue(events.hasListener(div));
    assertTrue(events.hasListener(div, 'click'));
    assertTrue(events.hasListener(div, 'click', false));
    assertTrue(events.hasListener(div, undefined, false));

    assertFalse(events.hasListener(div, 'click', true));
    assertFalse(events.hasListener(div, undefined, true));
    assertFalse(events.hasListener(div, 'mouseup'));

    // Test that hasListener returns false when all listeners are removed.
    events.unlistenByKey(key);
    assertFalse(events.hasListener(div));
  },

  testHasListenerWithEventTarget() {
    assertFalse(events.hasListener(et1));

    function callback() {}
    events.listen(et1, 'test', callback, true);
    assertTrue(events.hasListener(et1));
    assertTrue(events.hasListener(et1, 'test'));
    assertTrue(events.hasListener(et1, 'test', true));
    assertTrue(events.hasListener(et1, undefined, true));

    assertFalse(events.hasListener(et1, 'click'));
    assertFalse(events.hasListener(et1, 'test', false));

    events.unlisten(et1, 'test', callback, true);
    assertFalse(events.hasListener(et1));
  },

  testHasListenerWithMultipleTargets() {
    function callback() {}

    events.listen(et1, 'test1', callback, true);
    events.listen(et2, 'test2', callback, true);

    assertTrue(events.hasListener(et1));
    assertTrue(events.hasListener(et2));
    assertTrue(events.hasListener(et1, 'test1'));
    assertTrue(events.hasListener(et2, 'test2'));

    assertFalse(events.hasListener(et1, 'et2'));
    assertFalse(events.hasListener(et2, 'et1'));

    events.removeAll(et1);
    events.removeAll(et2);
  },

  testBubbleSingle() {
    et1.setParentEventTarget(et2);
    et2.setParentEventTarget(et3);

    let count = 0;
    function callback() {
      count++;
    }

    events.listen(et3, 'test', callback, false);

    et1.dispatchEvent('test');

    assertEquals(1, count);

    events.removeAll(et1);
    events.removeAll(et2);
    events.removeAll(et3);
  },

  testCaptureSingle() {
    et1.setParentEventTarget(et2);
    et2.setParentEventTarget(et3);

    let count = 0;
    function callback() {
      count++;
    }

    events.listen(et3, 'test', callback, true);

    et1.dispatchEvent('test');

    assertEquals(1, count);

    events.removeAll(et1);
    events.removeAll(et2);
    events.removeAll(et3);
  },

  testCaptureAndBubble() {
    et1.setParentEventTarget(et2);
    et2.setParentEventTarget(et3);

    let count = 0;
    function callbackCapture1() {
      count++;
      assertEquals(3, count);
    }
    function callbackBubble1() {
      count++;
      assertEquals(4, count);
    }

    function callbackCapture2() {
      count++;
      assertEquals(2, count);
    }
    function callbackBubble2() {
      count++;
      assertEquals(5, count);
    }

    function callbackCapture3() {
      count++;
      assertEquals(1, count);
    }
    function callbackBubble3() {
      count++;
      assertEquals(6, count);
    }

    events.listen(et1, 'test', callbackCapture1, true);
    events.listen(et1, 'test', callbackBubble1, false);
    events.listen(et2, 'test', callbackCapture2, true);
    events.listen(et2, 'test', callbackBubble2, false);
    events.listen(et3, 'test', callbackCapture3, true);
    events.listen(et3, 'test', callbackBubble3, false);

    et1.dispatchEvent('test');

    assertEquals(6, count);

    events.removeAll(et1);
    events.removeAll(et2);
    events.removeAll(et3);

    // Try again with the new API:
    count = 0;

    events.listen(et1, 'test', callbackCapture1, {capture: true});
    events.listen(et1, 'test', callbackBubble1, {capture: false});
    events.listen(et2, 'test', callbackCapture2, {capture: true});
    events.listen(et2, 'test', callbackBubble2, {capture: false});
    events.listen(et3, 'test', callbackCapture3, {capture: true});
    events.listen(et3, 'test', callbackBubble3, {capture: false});

    et1.dispatchEvent('test');

    assertEquals(6, count);

    events.removeAll(et1);
    events.removeAll(et2);
    events.removeAll(et3);

    /** Use computed properties to avoid compiler checks of defines */
    events['CAPTURE_SIMULATION_MODE'] = CaptureSimulationMode.OFF_AND_FAIL;
    count = 0;

    events.listen(et1, 'test', callbackCapture1, {capture: true});
    events.listen(et1, 'test', callbackBubble1, {capture: false});
    events.listen(et2, 'test', callbackCapture2, {capture: true});
    events.listen(et2, 'test', callbackBubble2, {capture: false});
    events.listen(et3, 'test', callbackCapture3, {capture: true});
    events.listen(et3, 'test', callbackBubble3, {capture: false});

    et1.dispatchEvent('test');

    assertEquals(6, count);

    events.removeAll(et1);
    events.removeAll(et2);
    events.removeAll(et3);
  },

  testCapturingRemovesBubblingListener() {
    let bubbleCount = 0;
    function callbackBubble() {
      bubbleCount++;
    }

    let captureCount = 0;
    function callbackCapture() {
      captureCount++;
      events.removeAll(et1);
    }

    events.listen(et1, 'test', callbackCapture, true);
    events.listen(et1, 'test', callbackBubble, false);

    et1.dispatchEvent('test');
    assertEquals(1, captureCount);
    assertEquals(0, bubbleCount);
  },

  testHandleBrowserEventBubblingListener() {
    let count = 0;
    const body = document.body;
    events.listen(body, 'click', () => {
      count++;
    });
    dispatchClick(body);
    assertEquals(1, count);
  },

  testHandleBrowserEventCapturingListener() {
    let count = 0;
    const body = document.body;
    events.listen(body, 'click', () => {
      count++;
    }, true);
    dispatchClick(body);
    assertEquals(1, count);
  },

  testHandleBrowserEventCapturingAndBubblingListener() {
    let count = 1;
    const body = document.body;
    events.listen(body, 'click', () => {
      count += 3;
    }, true);
    events.listen(body, 'click', () => {
      count *= 5;
    }, false);
    dispatchClick(body);
    assertEquals(20, count);
  },

  testHandleBrowserEventCapturingRemovesBubblingListener() {
    const body = document.body;

    let bubbleCount = 0;
    function callbackBubble() {
      bubbleCount++;
    }

    let captureCount = 0;
    function callbackCapture() {
      captureCount++;
      events.removeAll(body);
    }

    events.listen(body, 'click', callbackCapture, true);
    events.listen(body, 'click', callbackBubble, false);

    dispatchClick(body);
    assertEquals(1, captureCount);
    assertEquals(0, bubbleCount);
  },

  testHandleEventPropagationOnParentElement() {
    let count = 1;
    events.listen(document.documentElement, 'click', () => {
      count += 3;
    }, true);
    events.listen(document.documentElement, 'click', () => {
      count *= 5;
    }, false);
    dispatchClick(document.body);
    assertEquals(20, count);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testEntryPointRegistry() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const monitor = new EntryPointMonitor();
    const replacement = () => {};
    monitor.wrap = recordFunction(functions.constant(replacement));

    entryPointRegistry.monitorAll(monitor);
    assertTrue(monitor.wrap.getCallCount() >= 1);
    assertEquals(replacement, events.handleBrowserEvent_);
  },

  // Fixes bug http://b/6434926
  testListenOnceHandlerDispatchCausingInfiniteLoop() {
    const handleFoo = recordFunction(() => {
      et1.dispatchEvent('foo');
    });

    events.listenOnce(et1, 'foo', handleFoo);

    et1.dispatchEvent('foo');

    assertEquals(
        'Handler should be called only once.', 1, handleFoo.getCallCount());
  },

  testCreationStack() {
    if (!new Error().stack) return;
    propertyReplacer.replace(Listener, 'ENABLE_MONITORING', true);

    const div = dom.createElement(TagName.DIV);
    const key = events.listen(div, EventType.CLICK, goog.nullFunction);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const listenerStack = key.creationStack;

    // Check that the name of this test function occurs in the stack trace.
    assertContains('testCreationStack', listenerStack);
    events.unlistenByKey(key);
  },

  testListenOnceAfterListenDoesNotChangeExistingListener() {
    const listener = recordFunction();
    events.listen(document.body, 'click', listener);
    events.listenOnce(document.body, 'click', listener);

    dispatchClick(document.body);
    dispatchClick(document.body);
    dispatchClick(document.body);

    assertEquals(3, listener.getCallCount());
  },

  testListenOnceAfterListenOnceDoesNotChangeExistingListener() {
    const listener = recordFunction();
    events.listenOnce(document.body, 'click', listener);
    events.listenOnce(document.body, 'click', listener);

    dispatchClick(document.body);
    dispatchClick(document.body);
    dispatchClick(document.body);

    assertEquals(1, listener.getCallCount());
  },

  testListenAfterListenOnceRemoveOnceness() {
    const listener = recordFunction();
    events.listenOnce(document.body, 'click', listener);
    events.listen(document.body, 'click', listener);

    dispatchClick(document.body);
    dispatchClick(document.body);
    dispatchClick(document.body);

    assertEquals(3, listener.getCallCount());
  },

  testUnlistenAfterListenOnce() {
    const listener = recordFunction();

    events.listenOnce(document.body, 'click', listener);
    events.unlisten(document.body, 'click', listener);
    dispatchClick(document.body);

    events.listenOnce(document.body, 'click', listener);
    events.listen(document.body, 'click', listener);
    events.unlisten(document.body, 'click', listener);
    dispatchClick(document.body);

    events.listen(document.body, 'click', listener);
    events.listenOnce(document.body, 'click', listener);
    events.unlisten(document.body, 'click', listener);
    dispatchClick(document.body);

    events.listenOnce(document.body, 'click', listener);
    events.listenOnce(document.body, 'click', listener);
    events.unlisten(document.body, 'click', listener);
    dispatchClick(document.body);

    assertEquals(0, listener.getCallCount());
  },

  testEventBubblingWithReentrantDispatch_bubbling() {
    runEventPropagationWithReentrantDispatch(false);
  },

  testEventBubblingWithReentrantDispatch_capture() {
    runEventPropagationWithReentrantDispatch(true);
  },

  testEventPropagationWhenListenerRemoved_bubbling() {
    runEventPropagationWhenListenerRemoved(false);
  },

  testEventPropagationWhenListenerRemoved_capture() {
    runEventPropagationWhenListenerRemoved(true);
  },

  testEventPropagationWhenListenerAdded_bubbling() {
    runEventPropagationWhenListenerAdded(false);
  },

  testEventPropagationWhenListenerAdded_capture() {
    runEventPropagationWhenListenerAdded(true);
  },

  testEventPropagationWhenListenerAddedAndRemoved_bubbling() {
    runEventPropagationWhenListenerAddedAndRemoved(false);
  },

  testEventPropagationWhenListenerAddedAndRemoved_capture() {
    runEventPropagationWhenListenerAddedAndRemoved(true);
  },

  testAssertWhenUsedWithUninitializedCustomEventTarget() {
    const SubClass = function() { /* does not call superclass ctor */ };
    goog.inherits(SubClass, GoogEventTarget);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance = new SubClass();

    let e;
    e = assertThrows(() => {
      events.listen(instance, 'test1', () => {});
    });
    assertTrue(e instanceof AssertionError);
    e = assertThrows(() => {
      events.dispatchEvent(instance, 'test1');
    });
    assertTrue(e instanceof AssertionError);
    e = assertThrows(() => {
      instance.dispatchEvent('test1');
    });
    assertTrue(e instanceof AssertionError);
  },

  testAssertWhenDispatchEventIsUsedWithNonCustomEventTarget() {
    const obj = {};
    let e = assertThrows(/**
                            @suppress {checkTypes} suppression added to enable
                            type checking
                          */
                         () => {
                           events.dispatchEvent(obj, 'test1');
                         });
    assertTrue(e instanceof AssertionError);
  },

  testPropagationStoppedDuringCapture() {
    const captureHandler = recordFunction((e) => {
      e.stopPropagation();
    });
    const bubbleHandler = recordFunction();

    const body = document.body;
    const div = dom.createElement(TagName.DIV);
    body.appendChild(div);
    try {
      events.listen(body, 'click', captureHandler, true);
      events.listen(div, 'click', bubbleHandler, false);
      events.listen(body, 'click', bubbleHandler, false);

      dispatchClick(div);
      assertEquals(1, captureHandler.getCallCount());
      assertEquals(0, bubbleHandler.getCallCount());

      events.unlisten(body, 'click', captureHandler, true);

      dispatchClick(div);
      assertEquals(2, bubbleHandler.getCallCount());
    } finally {
      dom.removeNode(div);
      events.removeAll(body);
      events.removeAll(div);
    }
  },

  testPropagationStoppedDuringBubble() {
    const captureHandler = recordFunction();
    const bubbleHandler1 = recordFunction((e) => {
      e.stopPropagation();
    });
    const bubbleHandler2 = recordFunction();

    const body = document.body;
    const div = dom.createElement(TagName.DIV);
    body.appendChild(div);
    try {
      events.listen(body, 'click', captureHandler, true);
      events.listen(div, 'click', bubbleHandler1, false);
      events.listen(body, 'click', bubbleHandler2, false);

      dispatchClick(div);
      assertEquals(1, captureHandler.getCallCount());
      assertEquals(1, bubbleHandler1.getCallCount());
      assertEquals(0, bubbleHandler2.getCallCount());
    } finally {
      dom.removeNode(div);
      events.removeAll(body);
      events.removeAll(div);
    }
  },

  testAddingCaptureListenerDuringBubbleShouldNotFireTheListener() {
    const body = document.body;
    const div = dom.createElement(TagName.DIV);
    body.appendChild(div);

    const captureHandler1 = recordFunction();
    const captureHandler2 = recordFunction();
    const bubbleHandler = recordFunction((e) => {
      events.listen(body, 'click', captureHandler1, true);
      events.listen(div, 'click', captureHandler2, true);
    });

    try {
      events.listen(div, 'click', bubbleHandler, false);

      dispatchClick(div);

      // These verify that the capture handlers registered in the bubble
      // handler is not invoked in the same event propagation phase.
      assertEquals(0, captureHandler1.getCallCount());
      assertEquals(0, captureHandler2.getCallCount());
      assertEquals(1, bubbleHandler.getCallCount());
    } finally {
      dom.removeNode(div);
      events.removeAll(body);
      events.removeAll(div);
    }
  },

  testRemovingCaptureListenerDuringBubbleWouldNotFireListenerTwice() {
    const body = document.body;
    const div = dom.createElement(TagName.DIV);
    body.appendChild(div);

    const captureHandler = recordFunction();
    const bubbleHandler1 = recordFunction((e) => {
      events.unlisten(body, 'click', captureHandler, true);
    });
    const bubbleHandler2 = recordFunction();

    try {
      events.listen(body, 'click', captureHandler, true);
      events.listen(div, 'click', bubbleHandler1, false);
      events.listen(body, 'click', bubbleHandler2, false);

      dispatchClick(div);
      assertEquals(1, captureHandler.getCallCount());

      // Verify that neither of these handlers are called more than once.
      assertEquals(1, bubbleHandler1.getCallCount());
      assertEquals(1, bubbleHandler2.getCallCount());
    } finally {
      dom.removeNode(div);
      events.removeAll(body);
      events.removeAll(div);
    }
  },

  testCaptureSimulationModeOffAndFail() {
    /** Use computed properties to avoid compiler checks of defines */
    events['CAPTURE_SIMULATION_MODE'] = CaptureSimulationMode.OFF_AND_FAIL;
    const captureHandler = recordFunction();

    events.listen(document.body, 'click', captureHandler, true);
    dispatchClick(document.body);
    assertEquals(1, captureHandler.getCallCount());
  },

  testCaptureSimulationModeOffAndSilent() {
    /** Use computed properties to avoid compiler checks of defines */
    events['CAPTURE_SIMULATION_MODE'] = CaptureSimulationMode.OFF_AND_SILENT;
    const captureHandler = recordFunction();

    events.listen(document.body, 'click', captureHandler, true);
    dispatchClick(document.body);
    assertEquals(1, captureHandler.getCallCount());
  },
});
