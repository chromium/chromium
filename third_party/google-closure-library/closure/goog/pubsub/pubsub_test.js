/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.pubsub.PubSubTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const PubSub = goog.require('goog.pubsub.PubSub');
const testSuite = goog.require('goog.testing.testSuite');

let pubsub;
let asyncPubsub;
let mockClock;

testSuite({
  setUp() {
    pubsub = new PubSub();
    asyncPubsub = new PubSub(true);
    mockClock = new MockClock(true);
  },

  tearDown() {
    mockClock.uninstall();
    asyncPubsub.dispose();
    pubsub.dispose();
  },

  testConstructor() {
    assertNotNull('PubSub instance must not be null', pubsub);
    assertTrue(
        'PubSub instance must have the expected type',
        pubsub instanceof PubSub);
  },

  testDispose() {
    assertFalse(
        'PubSub instance must not have been disposed of', pubsub.isDisposed());
    pubsub.dispose();
    assertTrue(
        'PubSub instance must have been disposed of', pubsub.isDisposed());
  },

  testSubscribeUnsubscribe() {
    function foo1() {}
    function bar1() {}
    function foo2() {}
    function bar2() {}

    assertEquals(
        'Topic "foo" must not have any subscribers', 0, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must not have any subscribers', 0, pubsub.getCount('bar'));

    pubsub.subscribe('foo', foo1);
    assertEquals(
        'Topic "foo" must have 1 subscriber', 1, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must not have any subscribers', 0, pubsub.getCount('bar'));

    pubsub.subscribe('bar', bar1);
    assertEquals(
        'Topic "foo" must have 1 subscriber', 1, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 1 subscriber', 1, pubsub.getCount('bar'));

    pubsub.subscribe('foo', foo2);
    assertEquals(
        'Topic "foo" must have 2 subscribers', 2, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 1 subscriber', 1, pubsub.getCount('bar'));

    pubsub.subscribe('bar', bar2);
    assertEquals(
        'Topic "foo" must have 2 subscribers', 2, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount('bar'));

    assertTrue(pubsub.unsubscribe('foo', foo1));
    assertEquals(
        'Topic "foo" must have 1 subscriber', 1, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount('bar'));

    assertTrue(pubsub.unsubscribe('foo', foo2));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount('bar'));

    assertTrue(pubsub.unsubscribe('bar', bar1));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have 1 subscriber', 1, pubsub.getCount('bar'));

    assertTrue(pubsub.unsubscribe('bar', bar2));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount('foo'));
    assertEquals(
        'Topic "bar" must have no subscribers', 0, pubsub.getCount('bar'));

    assertFalse(
        'Unsubscribing a nonexistent topic must return false',
        pubsub.unsubscribe('baz', foo1));

    assertFalse(
        'Unsubscribing a nonexistent function must return false',
        pubsub.unsubscribe('foo', () => {}));
  },

  testSubscribeUnsubscribeWithContext() {
    function foo() {}
    function bar() {}

    const contextA = {};
    const contextB = {};

    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount('X'));

    pubsub.subscribe('X', foo, contextA);
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));

    pubsub.subscribe('X', bar);
    assertEquals('Topic "X" must have 2 subscribers', 2, pubsub.getCount('X'));

    pubsub.subscribe('X', bar, contextB);
    assertEquals('Topic "X" must have 3 subscribers', 3, pubsub.getCount('X'));

    assertFalse(
        'Unknown function/context combination return false',
        pubsub.unsubscribe('X', foo, contextB));

    assertTrue(pubsub.unsubscribe('X', foo, contextA));
    assertEquals('Topic "X" must have 2 subscribers', 2, pubsub.getCount('X'));

    assertTrue(pubsub.unsubscribe('X', bar));
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));

    assertTrue(pubsub.unsubscribe('X', bar, contextB));
    assertEquals('Topic "X" must have no subscribers', 0, pubsub.getCount('X'));
  },

  testSubscribeOnce() {
    let called;
    let context;

    called = false;
    pubsub.subscribeOnce('someTopic', () => {
      called = true;
    });
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse('Subscriber must not have been called yet', called);

    pubsub.publish('someTopic');
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount('someTopic'));
    assertTrue('Subscriber must have been called', called);

    context = {called: false};
    pubsub.subscribeOnce('someTopic', function() {
      this.called = true;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse('Subscriber must not have been called yet', context.called);

    pubsub.publish('someTopic');
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount('someTopic'));
    assertTrue('Subscriber must have been called', context.called);

    context = {called: false, value: 0};
    pubsub.subscribeOnce('someTopic', function(value) {
      this.called = true;
      this.value = value;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse('Subscriber must not have been called yet', context.called);
    assertEquals('Value must have expected value', 0, context.value);

    pubsub.publish('someTopic', 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount('someTopic'));
    assertTrue('Subscriber must have been called', context.called);
    assertEquals('Value must have been updated', 17, context.value);
  },

  testAsyncSubscribeOnce() {
    let callCount = 0;
    asyncPubsub.subscribeOnce('someTopic', () => {
      callCount++;
    });
    assertEquals(
        'Topic must have one subscriber', 1, asyncPubsub.getCount('someTopic'));
    mockClock.tick();
    assertEquals('Subscriber must not have been called yet', 0, callCount);

    asyncPubsub.publish('someTopic');
    asyncPubsub.publish('someTopic');
    mockClock.tick();
    assertEquals(
        'Topic must have no subscribers', 0, asyncPubsub.getCount('someTopic'));
    assertEquals('Subscriber must have been called once', 1, callCount);
  },

  testAsyncSubscribeOnceWithContext() {
    const context = {callCount: 0};
    asyncPubsub.subscribeOnce('someTopic', function() {
      this.callCount++;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, asyncPubsub.getCount('someTopic'));
    mockClock.tick();
    assertEquals(
        'Subscriber must not have been called yet', 0, context.callCount);

    asyncPubsub.publish('someTopic');
    asyncPubsub.publish('someTopic');
    mockClock.tick();
    assertEquals(
        'Topic must have no subscribers', 0, asyncPubsub.getCount('someTopic'));
    assertEquals('Subscriber must have been called once', 1, context.callCount);
  },

  testAsyncSubscribeOnceWithContextAndValue() {
    const context = {callCount: 0, value: 0};
    asyncPubsub.subscribeOnce('someTopic', function(value) {
      this.callCount++;
      this.value = value;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, asyncPubsub.getCount('someTopic'));
    mockClock.tick();
    assertEquals(
        'Subscriber must not have been called yet', 0, context.callCount);
    assertEquals('Value must have expected value', 0, context.value);

    asyncPubsub.publish('someTopic', 17);
    asyncPubsub.publish('someTopic', 42);
    mockClock.tick();
    assertEquals(
        'Topic must have no subscribers', 0, asyncPubsub.getCount('someTopic'));
    assertEquals('Subscriber must have been called once', 1, context.callCount);
    assertEquals('Value must have been updated', 17, context.value);
  },

  testSubscribeOnce_boundFn() {
    const context = {called: false, value: 0};

    function subscriber(value) {
      this.called = true;
      this.value = value;
    }

    pubsub.subscribeOnce('someTopic', goog.bind(subscriber, context));
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse('Subscriber must not have been called yet', context.called);
    assertEquals('Value must have expected value', 0, context.value);

    pubsub.publish('someTopic', 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount('someTopic'));
    assertTrue('Subscriber must have been called', context.called);
    assertEquals('Value must have been updated', 17, context.value);
  },

  testSubscribeOnce_partialFn() {
    let called = false;
    let value = 0;

    function subscriber(hasBeenCalled, newValue) {
      called = hasBeenCalled;
      value = newValue;
    }

    pubsub.subscribeOnce('someTopic', goog.partial(subscriber, true));
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse('Subscriber must not have been called yet', called);
    assertEquals('Value must have expected value', 0, value);

    pubsub.publish('someTopic', 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount('someTopic'));
    assertTrue('Subscriber must have been called', called);
    assertEquals('Value must have been updated', 17, value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSelfResubscribe() {
    let value = null;

    function resubscribe(iteration, newValue) {
      pubsub.subscribeOnce(
          'someTopic', goog.partial(resubscribe, iteration + 1));
      value = `${newValue}:${iteration}`;
    }

    pubsub.subscribeOnce('someTopic', goog.partial(resubscribe, 0));
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount('someTopic'));
    assertNull('Value must be null', value);

    pubsub.publish('someTopic', 'foo');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount('someTopic'));
    assertEquals(
        'Pubsub must not have any pending unsubscribe keys', 0,
        pubsub.pendingKeys_.length);
    assertEquals('Value be as expected', 'foo:0', value);

    pubsub.publish('someTopic', 'bar');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount('someTopic'));
    assertEquals(
        'Pubsub must not have any pending unsubscribe keys', 0,
        pubsub.pendingKeys_.length);
    assertEquals('Value be as expected', 'bar:1', value);

    pubsub.publish('someTopic', 'baz');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount('someTopic'));
    assertEquals(
        'Pubsub must not have any pending unsubscribe keys', 0,
        pubsub.pendingKeys_.length);
    assertEquals('Value be as expected', 'baz:2', value);
  },

  testUnsubscribeByKey() {
    let key1;
    let key2;
    let key3;

    key1 = pubsub.subscribe('X', () => {});
    key2 = pubsub.subscribe('Y', () => {});

    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));
    assertNotEquals('Subscription keys must be distinct', key1, key2);

    pubsub.unsubscribeByKey(key1);
    assertEquals('Topic "X" must have no subscribers', 0, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));

    key3 = pubsub.subscribe('X', () => {});
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));
    assertNotEquals('Subscription keys must be distinct', key1, key3);
    assertNotEquals('Subscription keys must be distinct', key2, key3);

    pubsub.unsubscribeByKey(key1);  // Obsolete key; should be no-op.
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));

    pubsub.unsubscribeByKey(key2);
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have no subscribers', 0, pubsub.getCount('Y'));

    pubsub.unsubscribeByKey(key3);
    assertEquals('Topic "X" must have no subscribers', 0, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have no subscribers', 0, pubsub.getCount('Y'));
  },

  testSubscribeUnsubscribeMultiple() {
    function foo() {}
    function bar() {}

    const context = {};

    assertEquals(
        'Pubsub channel must not have any subscribers', 0, pubsub.getCount());

    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount('X'));
    assertEquals(
        'Topic "Y" must not have any subscribers', 0, pubsub.getCount('Y'));
    assertEquals(
        'Topic "Z" must not have any subscribers', 0, pubsub.getCount('Z'));

    ['X', 'Y', 'Z'].forEach(topic => {
      pubsub.subscribe(topic, foo);
    });
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));
    assertEquals('Topic "Z" must have 1 subscriber', 1, pubsub.getCount('Z'));

    ['X', 'Y', 'Z'].forEach(topic => {
      pubsub.subscribe(topic, bar, context);
    });
    assertEquals('Topic "X" must have 2 subscribers', 2, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 2 subscribers', 2, pubsub.getCount('Y'));
    assertEquals('Topic "Z" must have 2 subscribers', 2, pubsub.getCount('Z'));

    assertEquals(
        'Pubsub channel must have a total of 6 subscribers', 6,
        pubsub.getCount());

    ['X', 'Y', 'Z'].forEach(topic => {
      pubsub.unsubscribe(topic, foo);
    });
    assertEquals('Topic "X" must have 1 subscriber', 1, pubsub.getCount('X'));
    assertEquals('Topic "Y" must have 1 subscriber', 1, pubsub.getCount('Y'));
    assertEquals('Topic "Z" must have 1 subscriber', 1, pubsub.getCount('Z'));

    ['X', 'Y', 'Z'].forEach(topic => {
      pubsub.unsubscribe(topic, bar, context);
    });
    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount('X'));
    assertEquals(
        'Topic "Y" must not have any subscribers', 0, pubsub.getCount('Y'));
    assertEquals(
        'Topic "Z" must not have any subscribers', 0, pubsub.getCount('Z'));

    assertEquals(
        'Pubsub channel must not have any subscribers', 0, pubsub.getCount());
  },

  testPublish() {
    const context = {};
    let fooCalled = false;
    let barCalled = false;

    function foo(x, y) {
      fooCalled = true;
      assertEquals('x must have expected value', 'x', x);
      assertEquals('y must have expected value', 'y', y);
    }

    function bar(x, y) {
      barCalled = true;
      assertEquals('Context must have expected value', context, this);
      assertEquals('x must have expected value', 'x', x);
      assertEquals('y must have expected value', 'y', y);
    }

    pubsub.subscribe('someTopic', foo);
    pubsub.subscribe('someTopic', bar, context);

    assertTrue(pubsub.publish('someTopic', 'x', 'y'));
    assertTrue('foo() must have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);

    fooCalled = false;
    barCalled = false;
    assertTrue(pubsub.unsubscribe('someTopic', foo));

    assertTrue(pubsub.publish('someTopic', 'x', 'y'));
    assertFalse('foo() must not have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);

    fooCalled = false;
    barCalled = false;
    pubsub.subscribe('differentTopic', foo);

    assertTrue(pubsub.publish('someTopic', 'x', 'y'));
    assertFalse('foo() must not have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);
  },

  testAsyncPublish() {
    const context = {};
    let fooCallCount = 0;
    let barCallCount = 0;

    function foo(x, y) {
      fooCallCount++;
      assertEquals('x must have expected value', 'x', x);
      assertEquals('y must have expected value', 'y', y);
    }

    function bar(x, y) {
      barCallCount++;
      assertEquals('Context must have expected value', context, this);
      assertEquals('x must have expected value', 'x', x);
      assertEquals('y must have expected value', 'y', y);
    }

    asyncPubsub.subscribe('someTopic', foo);
    asyncPubsub.subscribe('someTopic', bar, context);

    assertTrue(asyncPubsub.publish('someTopic', 'x', 'y'));
    assertEquals('foo() must not have been called', 0, fooCallCount);
    assertEquals('bar() must not have been called', 0, barCallCount);
    mockClock.tick();
    assertEquals('foo() must have been called once', 1, fooCallCount);
    assertEquals('bar() must have been called once', 1, barCallCount);

    fooCallCount = 0;
    barCallCount = 0;
    assertTrue(asyncPubsub.unsubscribe('someTopic', foo));

    assertTrue(asyncPubsub.publish('someTopic', 'x', 'y'));
    assertEquals('foo() must not have been called', 0, fooCallCount);
    assertEquals('bar() must not have been called', 0, barCallCount);
    mockClock.tick();
    assertEquals('foo() must not have been called', 0, fooCallCount);
    assertEquals('bar() must have been called once', 1, barCallCount);

    fooCallCount = 0;
    barCallCount = 0;
    asyncPubsub.subscribe('differentTopic', foo);
    assertTrue(asyncPubsub.publish('someTopic', 'x', 'y'));
    assertEquals('foo() must not have been called', 0, fooCallCount);
    assertEquals('bar() must not have been called', 0, barCallCount);
    mockClock.tick();
    assertEquals('foo() must not have been called', 0, fooCallCount);
    assertEquals('bar() must have been called once', 1, barCallCount);
  },

  testPublishEmptyTopic() {
    let fooCalled = false;
    function foo() {
      fooCalled = true;
    }

    assertFalse(
        'Publishing to nonexistent topic must return false',
        pubsub.publish('someTopic'));

    pubsub.subscribe('someTopic', foo);
    assertTrue(
        'Publishing to topic with subscriber must return true',
        pubsub.publish('someTopic'));
    assertTrue('Foo must have been called', fooCalled);

    pubsub.unsubscribe('someTopic', foo);
    fooCalled = false;
    assertFalse(
        'Publishing to topic without subscribers must return false',
        pubsub.publish('someTopic'));
    assertFalse('Foo must nothave been called', fooCalled);
  },

  testSubscribeWhilePublishing() {
    // It's OK for a subscriber to add a new subscriber to its own topic,
    // but the newly added subscriber shouldn't be called until the next
    // publish cycle.

    let firstCalled = false;
    let secondCalled = false;

    pubsub.subscribe('someTopic', () => {
      pubsub.subscribe('someTopic', () => {
        secondCalled = true;
      });
      firstCalled = true;
    });
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse(
        'No subscriber must have been called yet', firstCalled || secondCalled);

    pubsub.publish('someTopic');
    assertEquals(
        'Topic must have two subscribers', 2, pubsub.getCount('someTopic'));
    assertTrue('The first subscriber must have been called', firstCalled);
    assertFalse(
        'The second subscriber must not have been called yet', secondCalled);

    pubsub.publish('someTopic');
    assertEquals(
        'Topic must have three subscribers', 3, pubsub.getCount('someTopic'));
    assertTrue('The first subscriber must have been called', firstCalled);
    assertTrue(
        'The second subscriber must also have been called', secondCalled);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUnsubscribeWhilePublishing() {
    // It's OK for a subscriber to unsubscribe another subscriber from its
    // own topic, but the subscriber in question won't actually be removed
    // until after publishing is complete.

    let firstCalled = false;
    let secondCalled = false;
    let thirdCalled = false;

    function first() {
      assertTrue(
          'unsubscribe() must return true when unsubscribing',
          pubsub.unsubscribe('X', second));
      assertEquals(
          'Topic "X" must still have 3 subscribers', 3, pubsub.getCount('X'));
      firstCalled = true;
    }
    pubsub.subscribe('X', first);

    function second() {
      secondCalled = true;
    }
    pubsub.subscribe('X', second);

    function third() {
      assertTrue(
          'unsubscribe() must return true when unsubscribing',
          pubsub.unsubscribe('X', first));
      assertEquals(
          'Topic "X" must still have 3 subscribers', 3, pubsub.getCount('X'));
      thirdCalled = true;
    }
    pubsub.subscribe('X', third);

    assertEquals('Topic "X" must have 3 subscribers', 3, pubsub.getCount('X'));
    assertFalse(
        'No subscribers must have been called yet',
        firstCalled || secondCalled || thirdCalled);

    assertTrue(pubsub.publish('X'));
    assertTrue('First function must have been called', firstCalled);
    assertFalse('Second function must not have been called', secondCalled);
    assertTrue('Third function must have been called', thirdCalled);
    assertEquals(
        'Topic "X" must have 1 subscriber after publishing', 1,
        pubsub.getCount('X'));
    assertEquals(
        'PubSub must not have any subscriptions pending removal', 0,
        pubsub.pendingKeys_.length);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUnsubscribeSelfWhilePublishing() {
    // It's OK for a subscriber to unsubscribe itself, but it won't actually
    // be removed until after publishing is complete.

    let selfDestructCalled = false;

    let selfDestruct = function() {
      assertTrue(
          'unsubscribe() must return true when unsubscribing',
          pubsub.unsubscribe('someTopic', selfDestruct));
      assertEquals(
          'Topic must still have 1 subscriber', 1,
          pubsub.getCount('someTopic'));
      selfDestructCalled = true;
    };

    pubsub.subscribe('someTopic', selfDestruct);
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount('someTopic'));
    assertFalse(
        'selfDestruct() must not have been called yet', selfDestructCalled);

    pubsub.publish('someTopic');
    assertTrue('selfDestruct() must have been called', selfDestructCalled);
    assertEquals(
        'Topic must have no subscribers after publishing', 0,
        pubsub.getCount('someTopic'));
    assertEquals(
        'PubSub must not have any subscriptions pending removal', 0,
        pubsub.pendingKeys_.length);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDisposeWhilePublishing() {
    let callDispose = function() {
      pubsub.dispose();
    };
    let afterDisposeCalled = false;
    let afterDispose = function() {
      afterDisposeCalled = true;
    };
    pubsub.subscribe('someTopic', callDispose);
    pubsub.subscribe('someTopic', afterDispose);
    let exceptionThrown = false;
    try {
      pubsub.publish('someTopic');
    } catch (e) {
      exceptionThrown = true;
    }
    assertFalse('publishing did not throw an error', exceptionThrown);
    assertTrue('pubsub is disposed', pubsub.isDisposed());
    assertFalse('afterDispose must not have been called', afterDisposeCalled);
  },

  testPublishReturnValue() {
    const fn = function() {
      pubsub.unsubscribe('X', fn);
    };
    pubsub.subscribe('X', fn);
    assertTrue(
        'publish() must return true even if the only subscriber ' +
            'removes itself during publishing',
        pubsub.publish('X'));
  },

  testNestedPublish() {
    let x1 = false;
    let x2 = false;
    let y1 = false;
    let y2 = false;

    const fn1 = function() {
      pubsub.publish('Y');
      pubsub.unsubscribe('X', fn1);
      x1 = true;
    };
    pubsub.subscribe('X', fn1);

    pubsub.subscribe('X', () => {
      x2 = true;
    });

    let fn2 = function() {
      pubsub.unsubscribe('Y', fn2);
      y1 = true;
    };
    pubsub.subscribe('Y', fn2);

    pubsub.subscribe('Y', () => {
      y2 = true;
    });

    pubsub.publish('X');

    assertTrue('x1 must be true', x1);
    assertTrue('x2 must be true', x2);
    assertTrue('y1 must be true', y1);
    assertTrue('y2 must be true', y2);
  },

  testClear() {
    function fn() {}

    ['W', 'X', 'Y', 'Z'].forEach(topic => {
      pubsub.subscribe(topic, fn);
    });
    assertEquals(
        'Pubsub channel must have 4 subscribers', 4, pubsub.getCount());

    pubsub.clear('W');
    assertEquals(
        'Pubsub channel must have 3 subscribers', 3, pubsub.getCount());

    ['X', 'Y'].forEach(topic => {
      pubsub.clear(topic);
    });
    assertEquals('Pubsub channel must have 1 subscriber', 1, pubsub.getCount());

    pubsub.clear();
    assertEquals(
        'Pubsub channel must have no subscribers', 0, pubsub.getCount());
  },

  testSubscriberExceptionUnlocksSubscriptions() {
    const key1 = pubsub.subscribe('X', () => {});

    pubsub.subscribe('X', () => {
      // Pushes "key1" onto queue to be unsubscribed after subscriptions are
      // processed.
      pubsub.unsubscribeByKey(key1);
    });

    pubsub.subscribe('X', () => {
      throw 'Oh no!';
    });

    const key2 = pubsub.subscribe('X', () => {});

    assertThrows(() => {
      pubsub.publish('X');
    });

    assertTrue(pubsub.unsubscribeByKey(key2));
    // "key1" should've been successfully removed already;
    assertFalse(pubsub.unsubscribeByKey(key1));
  },

  testNestedSubscribeOnce() {
    let calls = 0;

    pubsub.subscribeOnce('X', () => {
      calls++;
    });

    pubsub.subscribe('Y', () => {
      pubsub.publish('X');
      pubsub.publish('X');
    });

    pubsub.publish('Y');

    assertEquals('X must be called once', 1, calls);
  },
});
