/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.DisposableTest');
goog.setTestOnly();

const Disposable = goog.require('goog.Disposable');
const dispose = goog.require('goog.dispose');
const disposeAll = goog.require('goog.disposeAll');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let d1;
let d2;

// Sample subclass of goog.Disposable.
class DisposableTest extends Disposable {
  constructor() {
    super();
    this.element = document.getElementById('someElement');
  }

  disposeInternal() {
    super.disposeInternal();
    delete this.element;
  }
}

// Class that doesn't inherit from goog.Disposable, but implements the
// disposable interface via duck typing.
class DisposableDuck {
  constructor() {
    this.element = document.getElementById('someElement');
  }

  dispose() {
    delete this.element;
  }
}

// Class which calls dispose recursively.
class RecursiveDisposable extends Disposable {
  constructor() {
    super();
    this.disposedCount = 0;
  }

  disposeInternal() {
    ++this.disposedCount;
    assertEquals('Disposed too many times', 1, this.disposedCount);
    this.dispose();
  }
}

// Test methods.

testSuite({
  setUp() {
    d1 = new Disposable();
    d2 = new DisposableTest();
  },

  tearDown() {
    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.OFF;

    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['INCLUDE_STACK_ON_CREATION'] = true;

    /** @suppress {visibility} suppression added to enable type checking */
    Disposable.instances_ = {};
    d1.dispose();
    d2.dispose();
  },

  testConstructor() {
    assertFalse(d1.isDisposed());
    assertFalse(d2.isDisposed());
    assertEquals(document.getElementById('someElement'), d2.element);
  },

  testDispose() {
    assertFalse(d1.isDisposed());
    d1.dispose();
    assertTrue(
        'goog.Disposable instance should have been disposed of',
        d1.isDisposed());

    assertFalse(d2.isDisposed());
    d2.dispose();
    assertTrue(
        'goog.DisposableTest instance should have been disposed of',
        d2.isDisposed());
  },

  testDisposeInternal() {
    assertNotUndefined(d2.element);
    d2.dispose();
    assertUndefined(
        'goog.DisposableTest.prototype.disposeInternal should ' +
            'have deleted the element reference',
        d2.element);
  },

  testDisposeAgain() {
    d2.dispose();
    assertUndefined(
        'goog.DisposableTest.prototype.disposeInternal should ' +
            'have deleted the element reference',
        d2.element);
    // Manually reset the element to a non-null value, and call dispose().
    // Because the object is already marked disposed, disposeInternal won't
    // be called again.
    d2.element = document.getElementById('someElement');
    d2.dispose();
    assertNotUndefined(
        'disposeInternal should not be called again if the ' +
            'object has already been marked disposed',
        d2.element);
  },

  testDisposeWorksRecursively() {
    new RecursiveDisposable().dispose();
  },

  testStaticDispose() {
    assertFalse(d1.isDisposed());
    dispose(d1);
    assertTrue(
        'goog.Disposable instance should have been disposed of',
        d1.isDisposed());

    assertFalse(d2.isDisposed());
    dispose(d2);
    assertTrue(
        'goog.DisposableTest instance should have been disposed of',
        d2.isDisposed());

    const duck = new DisposableDuck();
    assertNotUndefined(duck.element);
    dispose(duck);
    assertUndefined(
        'goog.dispose should have disposed of object that ' +
            'implements the disposable interface',
        duck.element);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testStaticDisposeOnNonDisposableType() {
    // Call goog.dispose() with various types and make sure no errors are
    // thrown.
    dispose(true);
    dispose(false);
    dispose(null);
    dispose(undefined);
    dispose('');
    dispose([]);
    dispose({});

    function A() {}
    dispose(new A());
  },

  testMonitoringFailure() {
    function BadDisposable() {}
    goog.inherits(BadDisposable, Disposable);

    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.PERMANENT;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const badDisposable = new BadDisposable;
    assertArrayEquals(
        'no disposable objects registered', [],
        Disposable.getUndisposedObjects());
    assertThrows(
        'the base ctor should have been called',
        goog.bind(badDisposable.dispose, badDisposable));
  },

  testGetUndisposedObjects() {
    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.PERMANENT;

    const d1 = new DisposableTest();
    const d2 = new DisposableTest();
    assertSameElements(
        'the undisposed instances', [d1, d2],
        Disposable.getUndisposedObjects());

    d1.dispose();
    assertSameElements(
        '1 undisposed instance left', [d2], Disposable.getUndisposedObjects());

    d1.dispose();
    assertSameElements(
        'second disposal of the same object is no-op', [d2],
        Disposable.getUndisposedObjects());

    d2.dispose();
    assertSameElements(
        'all objects have been disposed of', [],
        Disposable.getUndisposedObjects());
  },

  testClearUndisposedObjects() {
    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.PERMANENT;

    const d1 = new DisposableTest();
    const d2 = new DisposableTest();
    d2.dispose();
    Disposable.clearUndisposedObjects();
    assertSameElements(
        'no undisposed object in the registry', [],
        Disposable.getUndisposedObjects());

    assertThrows('disposal after clearUndisposedObjects()', () => {
      d1.dispose();
    });

    // d2 is already disposed of, the redisposal shouldn't throw error.
    d2.dispose();
  },

  testRegisterDisposable() {
    const d1 = new DisposableTest();
    const d2 = new DisposableTest();

    d1.registerDisposable(d2);
    d1.dispose();

    assertTrue('d2 should be disposed when d1 is disposed', d2.isDisposed());
  },

  testDisposeAll() {
    const d1 = new DisposableTest();
    const d2 = new DisposableTest();

    disposeAll(d1, d2);

    assertTrue('d1 should be disposed', d1.isDisposed());
    assertTrue('d2 should be disposed', d2.isDisposed());
  },

  testDisposeAllRecursive() {
    const d1 = new DisposableTest();
    const d2 = new DisposableTest();
    const d3 = new DisposableTest();
    const d4 = new DisposableTest();

    disposeAll(d1, [[d2], d3, d4]);

    assertTrue('d1 should be disposed', d1.isDisposed());
    assertTrue('d2 should be disposed', d2.isDisposed());
    assertTrue('d3 should be disposed', d3.isDisposed());
    assertTrue('d4 should be disposed', d4.isDisposed());
  },

  testCreationStack() {
    if (!new Error().stack) return;
    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.PERMANENT;
    const disposableStack = new DisposableTest().creationStack;
    // Check that the name of this test function occurs in the stack trace.
    assertNotEquals(-1, disposableStack.indexOf('testCreationStack'));
  },

  testMonitoredWithoutCreationStack() {
    if (!new Error().stack) return;

    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.PERMANENT;

    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['INCLUDE_STACK_ON_CREATION'] = false;

    const d1 = new DisposableTest();

    // Check that it is tracked, but not with a creation stack.
    assertUndefined(d1.creationStack);
    assertSameElements(
        'the undisposed instance', [d1], Disposable.getUndisposedObjects());
  },

  testOnDisposeCallback() {
    const callback = recordFunction();
    d1.addOnDisposeCallback(callback);
    assertEquals('callback called too early', 0, callback.getCallCount());
    d1.dispose();
    assertEquals(
        'callback should be called once on dispose', 1,
        callback.getCallCount());
  },

  testOnDisposeCallbackOrder() {
    const invocations = [];
    const callback = (str) => {
      invocations.push(str);
    };
    d1.addOnDisposeCallback(goog.partial(callback, 'a'));
    d1.addOnDisposeCallback(goog.partial(callback, 'b'));
    dispose(d1);
    assertArrayEquals(
        'callbacks should be called in chronological order', ['a', 'b'],
        invocations);
  },

  testAddOnDisposeCallbackAfterDispose() {
    const callback = recordFunction();
    const scope = {};
    dispose(d1);
    d1.addOnDisposeCallback(callback, scope);
    assertEquals(
        'Callback should be immediately called if already disposed', 1,
        callback.getCallCount());
    assertEquals(
        'Callback scope should be respected', scope,
        callback.getLastCall().getThis());
  },

  testInteractiveMonitoring() {
    const d1 = new DisposableTest();

    /** Use computed properties to avoid compiler checks of defines. */
    Disposable['MONITORING_MODE'] = Disposable.MonitoringMode.INTERACTIVE;

    const d2 = new DisposableTest();

    assertSameElements(
        'only 1 undisposed instance tracked', [d2],
        Disposable.getUndisposedObjects());

    // No errors should be thrown.
    d1.dispose();

    assertSameElements(
        '1 undisposed instance left', [d2], Disposable.getUndisposedObjects());

    d2.dispose();
    assertSameElements('all disposed', [], Disposable.getUndisposedObjects());
  },
});
