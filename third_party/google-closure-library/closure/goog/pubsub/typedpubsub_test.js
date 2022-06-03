/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.pubsub.TypedPubSubTest');
goog.setTestOnly();

const TopicId = goog.require('goog.pubsub.TopicId');
const TypedPubSub = goog.require('goog.pubsub.TypedPubSub');
const testSuite = goog.require('goog.testing.testSuite');

let pubsub;

testSuite({
  setUp() {
    pubsub = new TypedPubSub();
  },

  tearDown() {
    pubsub.dispose();
  },

  testConstructor() {
    assertNotNull('PubSub instance must not be null', pubsub);
    assertTrue(
        'PubSub instance must have the expected type',
        pubsub instanceof TypedPubSub);
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

    /** const */ const FOO = new TopicId('foo');
    /** const */ const BAR = new TopicId('bar');
    /** const */ const BAZ = new TopicId('baz');

    assertEquals(
        'Topic "foo" must not have any subscribers', 0, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must not have any subscribers', 0, pubsub.getCount(BAR));

    pubsub.subscribe(FOO, foo1);
    assertEquals('Topic "foo" must have 1 subscriber', 1, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must not have any subscribers', 0, pubsub.getCount(BAR));

    pubsub.subscribe(BAR, bar1);
    assertEquals('Topic "foo" must have 1 subscriber', 1, pubsub.getCount(FOO));
    assertEquals('Topic "bar" must have 1 subscriber', 1, pubsub.getCount(BAR));

    pubsub.subscribe(FOO, foo2);
    assertEquals(
        'Topic "foo" must have 2 subscribers', 2, pubsub.getCount(FOO));
    assertEquals('Topic "bar" must have 1 subscriber', 1, pubsub.getCount(BAR));

    pubsub.subscribe(BAR, bar2);
    assertEquals(
        'Topic "foo" must have 2 subscribers', 2, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount(BAR));

    assertTrue(pubsub.unsubscribe(FOO, foo1));
    assertEquals('Topic "foo" must have 1 subscriber', 1, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount(BAR));

    assertTrue(pubsub.unsubscribe(FOO, foo2));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must have 2 subscribers', 2, pubsub.getCount(BAR));

    assertTrue(pubsub.unsubscribe(BAR, bar1));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount(FOO));
    assertEquals('Topic "bar" must have 1 subscriber', 1, pubsub.getCount(BAR));

    assertTrue(pubsub.unsubscribe(BAR, bar2));
    assertEquals(
        'Topic "foo" must have no subscribers', 0, pubsub.getCount(FOO));
    assertEquals(
        'Topic "bar" must have no subscribers', 0, pubsub.getCount(BAR));

    assertFalse(
        'Unsubscribing a nonexistent topic must return false',
        pubsub.unsubscribe(BAZ, foo1));

    assertFalse(
        'Unsubscribing a nonexistent function must return false',
        pubsub.unsubscribe(FOO, () => {}));
  },

  testSubscribeUnsubscribeWithContext() {
    function foo() {}
    function bar() {}

    const contextA = {};
    const contextB = {};

    /** const */ const TOPIC_X = new TopicId('X');

    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount(TOPIC_X));

    pubsub.subscribe(TOPIC_X, foo, contextA);
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));

    pubsub.subscribe(TOPIC_X, bar);
    assertEquals(
        'Topic "X" must have 2 subscribers', 2, pubsub.getCount(TOPIC_X));

    pubsub.subscribe(TOPIC_X, bar, contextB);
    assertEquals(
        'Topic "X" must have 3 subscribers', 3, pubsub.getCount(TOPIC_X));

    assertFalse(
        'Unknown function/context combination return false',
        pubsub.unsubscribe(TOPIC_X, foo, contextB));

    assertTrue(pubsub.unsubscribe(TOPIC_X, foo, contextA));
    assertEquals(
        'Topic "X" must have 2 subscribers', 2, pubsub.getCount(TOPIC_X));

    assertTrue(pubsub.unsubscribe(TOPIC_X, bar));
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));

    assertTrue(pubsub.unsubscribe(TOPIC_X, bar, contextB));
    assertEquals(
        'Topic "X" must have no subscribers', 0, pubsub.getCount(TOPIC_X));
  },

  testSubscribeOnce() {
    let called;
    let context;

    const SOME_TOPIC = new TopicId('someTopic');

    called = false;
    pubsub.subscribeOnce(SOME_TOPIC, () => {
      called = true;
    });
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse('Subscriber must not have been called yet', called);

    pubsub.publish(SOME_TOPIC);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount(SOME_TOPIC));
    assertTrue('Subscriber must have been called', called);

    context = {called: false};
    pubsub.subscribeOnce(SOME_TOPIC, function() {
      this.called = true;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse('Subscriber must not have been called yet', context.called);

    pubsub.publish(SOME_TOPIC);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount(SOME_TOPIC));
    assertTrue('Subscriber must have been called', context.called);

    context = {called: false, value: 0};
    pubsub.subscribeOnce(SOME_TOPIC, function(value) {
      this.called = true;
      this.value = value;
    }, context);
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse('Subscriber must not have been called yet', context.called);
    assertEquals('Value must have expected value', 0, context.value);

    pubsub.publish(SOME_TOPIC, 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount(SOME_TOPIC));
    assertTrue('Subscriber must have been called', context.called);
    assertEquals('Value must have been updated', 17, context.value);
  },

  testSubscribeOnce_boundFn() {
    const context = {called: false, value: 0};

    const SOME_TOPIC = new TopicId('someTopic');

    function subscriber(value) {
      this.called = true;
      this.value = value;
    }

    pubsub.subscribeOnce(SOME_TOPIC, goog.bind(subscriber, context));
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse('Subscriber must not have been called yet', context.called);
    assertEquals('Value must have expected value', 0, context.value);

    pubsub.publish(SOME_TOPIC, 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount(SOME_TOPIC));
    assertTrue('Subscriber must have been called', context.called);
    assertEquals('Value must have been updated', 17, context.value);
  },

  testSubscribeOnce_partialFn() {
    let called = false;
    let value = 0;

    const SOME_TOPIC = new TopicId('someTopic');

    function subscriber(hasBeenCalled, newValue) {
      called = hasBeenCalled;
      value = newValue;
    }

    pubsub.subscribeOnce(SOME_TOPIC, goog.partial(subscriber, true));
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse('Subscriber must not have been called yet', called);
    assertEquals('Value must have expected value', 0, value);

    pubsub.publish(SOME_TOPIC, 17);
    assertEquals(
        'Topic must have no subscribers', 0, pubsub.getCount(SOME_TOPIC));
    assertTrue('Subscriber must have been called', called);
    assertEquals('Value must have been updated', 17, value);
  },

  testSelfResubscribe() {
    let value = null;

    const SOME_TOPIC = new TopicId('someTopic');

    function resubscribe(iteration, newValue) {
      pubsub.subscribeOnce(
          SOME_TOPIC, goog.partial(resubscribe, iteration + 1));
      value = `${newValue}:${iteration}`;
    }

    pubsub.subscribeOnce(SOME_TOPIC, goog.partial(resubscribe, 0));
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertNull('Value must be null', value);

    pubsub.publish(SOME_TOPIC, 'foo');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertEquals('Value be as expected', 'foo:0', value);

    pubsub.publish(SOME_TOPIC, 'bar');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertEquals('Value be as expected', 'bar:1', value);

    pubsub.publish(SOME_TOPIC, 'baz');
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertEquals('Value be as expected', 'baz:2', value);
  },

  testUnsubscribeByKey() {
    let key1;
    let key2;
    let key3;

    /** const */ const TOPIC_X = new TopicId('X');
    /** const */ const TOPIC_Y = new TopicId('Y');

    key1 = pubsub.subscribe(TOPIC_X, () => {});
    key2 = pubsub.subscribe(TOPIC_Y, () => {});

    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));
    assertNotEquals('Subscription keys must be distinct', key1, key2);

    pubsub.unsubscribeByKey(key1);
    assertEquals(
        'Topic "X" must have no subscribers', 0, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));

    key3 = pubsub.subscribe(TOPIC_X, () => {});
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));
    assertNotEquals('Subscription keys must be distinct', key1, key3);
    assertNotEquals('Subscription keys must be distinct', key2, key3);

    pubsub.unsubscribeByKey(key1);  // Obsolete key; should be no-op.
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));

    pubsub.unsubscribeByKey(key2);
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have no subscribers', 0, pubsub.getCount(TOPIC_Y));

    pubsub.unsubscribeByKey(key3);
    assertEquals(
        'Topic "X" must have no subscribers', 0, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have no subscribers', 0, pubsub.getCount(TOPIC_Y));
  },

  testSubscribeUnsubscribeMultiple() {
    function foo() {}
    function bar() {}

    const context = {};

    /** const */ const TOPIC_X = new TopicId('X');
    /** const */ const TOPIC_Y = new TopicId('Y');
    /** const */ const TOPIC_Z = new TopicId('Z');

    assertEquals(
        'Pubsub channel must not have any subscribers', 0, pubsub.getCount());

    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must not have any subscribers', 0, pubsub.getCount(TOPIC_Y));
    assertEquals(
        'Topic "Z" must not have any subscribers', 0, pubsub.getCount(TOPIC_Z));

    [TOPIC_X, TOPIC_Y, TOPIC_Z].forEach(topic => {
      pubsub.subscribe(topic, foo);
    });
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));
    assertEquals(
        'Topic "Z" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Z));

    [TOPIC_X, TOPIC_Y, TOPIC_Z].forEach(topic => {
      pubsub.subscribe(topic, bar, context);
    });
    assertEquals(
        'Topic "X" must have 2 subscribers', 2, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 2 subscribers', 2, pubsub.getCount(TOPIC_Y));
    assertEquals(
        'Topic "Z" must have 2 subscribers', 2, pubsub.getCount(TOPIC_Z));

    assertEquals(
        'Pubsub channel must have a total of 6 subscribers', 6,
        pubsub.getCount());

    [TOPIC_X, TOPIC_Y, TOPIC_Z].forEach(topic => {
      pubsub.unsubscribe(topic, foo);
    });
    assertEquals(
        'Topic "X" must have 1 subscriber', 1, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Y));
    assertEquals(
        'Topic "Z" must have 1 subscriber', 1, pubsub.getCount(TOPIC_Z));

    [TOPIC_X, TOPIC_Y, TOPIC_Z].forEach(topic => {
      pubsub.unsubscribe(topic, bar, context);
    });
    assertEquals(
        'Topic "X" must not have any subscribers', 0, pubsub.getCount(TOPIC_X));
    assertEquals(
        'Topic "Y" must not have any subscribers', 0, pubsub.getCount(TOPIC_Y));
    assertEquals(
        'Topic "Z" must not have any subscribers', 0, pubsub.getCount(TOPIC_Z));

    assertEquals(
        'Pubsub channel must not have any subscribers', 0, pubsub.getCount());
  },

  testPublish() {
    const context = {};
    let fooCalled = false;
    let barCalled = false;

    const SOME_TOPIC = new TopicId('someTopic');

    function foo(record) {
      fooCalled = true;
      assertEquals('x must have expected value', 'x', record.x);
      assertEquals('y must have expected value', 'y', record.y);
    }

    function bar(record) {
      barCalled = true;
      assertEquals('Context must have expected value', context, this);
      assertEquals('x must have expected value', 'x', record.x);
      assertEquals('y must have expected value', 'y', record.y);
    }

    pubsub.subscribe(SOME_TOPIC, foo);
    pubsub.subscribe(SOME_TOPIC, bar, context);

    assertTrue(pubsub.publish(SOME_TOPIC, {x: 'x', y: 'y'}));
    assertTrue('foo() must have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);

    fooCalled = false;
    barCalled = false;
    assertTrue(pubsub.unsubscribe(SOME_TOPIC, foo));

    assertTrue(pubsub.publish(SOME_TOPIC, {x: 'x', y: 'y'}));
    assertFalse('foo() must not have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);

    fooCalled = false;
    barCalled = false;
    pubsub.subscribe('differentTopic', foo);

    assertTrue(pubsub.publish(SOME_TOPIC, {x: 'x', y: 'y'}));
    assertFalse('foo() must not have been called', fooCalled);
    assertTrue('bar() must have been called', barCalled);
  },

  testPublishEmptyTopic() {
    let fooCalled = false;
    function foo() {
      fooCalled = true;
    }

    const SOME_TOPIC = new TopicId('someTopic');

    assertFalse(
        'Publishing to nonexistent topic must return false',
        pubsub.publish(SOME_TOPIC));

    pubsub.subscribe(SOME_TOPIC, foo);
    assertTrue(
        'Publishing to topic with subscriber must return true',
        pubsub.publish(SOME_TOPIC));
    assertTrue('Foo must have been called', fooCalled);

    pubsub.unsubscribe(SOME_TOPIC, foo);
    fooCalled = false;
    assertFalse(
        'Publishing to topic without subscribers must return false',
        pubsub.publish(SOME_TOPIC));
    assertFalse('Foo must nothave been called', fooCalled);
  },

  testSubscribeWhilePublishing() {
    // It's OK for a subscriber to add a new subscriber to its own topic,
    // but the newly added subscriber shouldn't be called until the next
    // publish cycle.

    let firstCalled = false;
    let secondCalled = false;

    const SOME_TOPIC = new TopicId('someTopic');

    pubsub.subscribe(SOME_TOPIC, () => {
      pubsub.subscribe(SOME_TOPIC, () => {
        secondCalled = true;
      });
      firstCalled = true;
    });
    assertEquals(
        'Topic must have one subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse(
        'No subscriber must have been called yet', firstCalled || secondCalled);

    pubsub.publish(SOME_TOPIC);
    assertEquals(
        'Topic must have two subscribers', 2, pubsub.getCount(SOME_TOPIC));
    assertTrue('The first subscriber must have been called', firstCalled);
    assertFalse(
        'The second subscriber must not have been called yet', secondCalled);

    pubsub.publish(SOME_TOPIC);
    assertEquals(
        'Topic must have three subscribers', 3, pubsub.getCount(SOME_TOPIC));
    assertTrue('The first subscriber must have been called', firstCalled);
    assertTrue(
        'The second subscriber must also have been called', secondCalled);
  },

  testUnsubscribeWhilePublishing() {
    // It's OK for a subscriber to unsubscribe another subscriber from its
    // own topic, but the subscriber in question won't actually be removed
    // until after publishing is complete.

    let firstCalled = false;
    let secondCalled = false;
    let thirdCalled = false;

    /** const */ const TOPIC_X = new TopicId('X');

    function first() {
      assertTrue(
          'unsubscribe() must return true when removing a topic',
          pubsub.unsubscribe(TOPIC_X, second));
      assertEquals(
          'Topic "X" must still have 3 subscribers', 3,
          pubsub.getCount(TOPIC_X));
      firstCalled = true;
    }
    pubsub.subscribe(TOPIC_X, first);

    function second() {
      secondCalled = true;
    }
    pubsub.subscribe(TOPIC_X, second);

    function third() {
      assertTrue(
          'unsubscribe() must return true when removing a topic',
          pubsub.unsubscribe(TOPIC_X, first));
      assertEquals(
          'Topic "X" must still have 3 subscribers', 3,
          pubsub.getCount(TOPIC_X));
      thirdCalled = true;
    }
    pubsub.subscribe(TOPIC_X, third);

    assertEquals(
        'Topic "X" must have 3 subscribers', 3, pubsub.getCount(TOPIC_X));
    assertFalse(
        'No subscribers must have been called yet',
        firstCalled || secondCalled || thirdCalled);

    assertTrue(pubsub.publish(TOPIC_X));
    assertTrue('First function must have been called', firstCalled);
    assertFalse('Second function must have been called', secondCalled);
    assertTrue('Third function must have been called', thirdCalled);
    assertEquals(
        'Topic "X" must have 1 subscriber after publishing', 1,
        pubsub.getCount(TOPIC_X));
  },

  testUnsubscribeSelfWhilePublishing() {
    // It's OK for a subscriber to unsubscribe itself, but it won't actually
    // be removed until after publishing is complete.

    let selfDestructCalled = false;

    const SOME_TOPIC = new TopicId('someTopic');

    let selfDestruct = function() {
      assertTrue(
          'unsubscribe() must return true when removing a topic',
          pubsub.unsubscribe(SOME_TOPIC, selfDestruct));
      assertEquals(
          'Topic must still have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
      selfDestructCalled = true;
    };

    pubsub.subscribe(SOME_TOPIC, selfDestruct);
    assertEquals(
        'Topic must have 1 subscriber', 1, pubsub.getCount(SOME_TOPIC));
    assertFalse(
        'selfDestruct() must not have been called yet', selfDestructCalled);

    pubsub.publish(SOME_TOPIC);
    assertTrue('selfDestruct() must have been called', selfDestructCalled);
    assertEquals(
        'Topic must have no subscribers after publishing', 0,
        pubsub.getCount(SOME_TOPIC));
  },

  testPublishReturnValue() {
    const SOME_TOPIC = new TopicId('someTopic');
    const fn = function() {
      pubsub.unsubscribe(SOME_TOPIC, fn);
    };
    pubsub.subscribe(SOME_TOPIC, fn);
    assertTrue(
        'publish() must return true even if the only subscriber ' +
            'removes itself during publishing',
        pubsub.publish(SOME_TOPIC));
  },

  testNestedPublish() {
    let x1 = false;
    let x2 = false;
    let y1 = false;
    let y2 = false;

    /** @const */ const TOPIC_X = new TopicId('X');
    /** @const */ const TOPIC_Y = new TopicId('Y');

    const callback1 = function() {
      pubsub.publish(TOPIC_Y);
      pubsub.unsubscribe(TOPIC_X, callback1);
      x1 = true;
    };

    pubsub.subscribe(TOPIC_X, callback1);

    pubsub.subscribe(TOPIC_X, () => {
      x2 = true;
    });

    const callback2 = function() {
      pubsub.unsubscribe(TOPIC_Y, callback2);
      y1 = true;
    };
    pubsub.subscribe(TOPIC_Y, callback2);

    pubsub.subscribe(TOPIC_Y, () => {
      y2 = true;
    });

    pubsub.publish(TOPIC_X);

    assertTrue('x1 must be true', x1);
    assertTrue('x2 must be true', x2);
    assertTrue('y1 must be true', y1);
    assertTrue('y2 must be true', y2);
  },

  testClear() {
    function fn() {}

    const topics = [
      new TopicId('W'),
      new TopicId('X'),
      new TopicId('Y'),
      new TopicId('Z'),
    ];

    topics.forEach(topic => {
      pubsub.subscribe(topic, fn);
    });
    assertEquals(
        'Pubsub channel must have 4 subscribers', 4, pubsub.getCount());

    pubsub.clear(topics[0]);
    assertEquals(
        'Pubsub channel must have 3 subscribers', 3, pubsub.getCount());

    pubsub.clear(topics[1]);
    pubsub.clear(topics[2]);
    assertEquals('Pubsub channel must have 1 subscriber', 1, pubsub.getCount());

    pubsub.clear();
    assertEquals(
        'Pubsub channel must have no subscribers', 0, pubsub.getCount());
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
