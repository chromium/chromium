/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.pubsub.BroadcastPubSubTest');
goog.setTestOnly();

const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const BroadcastPubSub = goog.require('goog.labs.pubsub.BroadcastPubSub');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const Level = goog.require('goog.log.Level');
const MockClock = goog.require('goog.testing.MockClock');
const MockControl = goog.require('goog.testing.MockControl');
const MockInterface = goog.requireType('goog.testing.MockInterface');
const StorageStorage = goog.require('goog.storage.Storage');
const StructsMap = goog.require('goog.structs.Map');
const events = goog.require('goog.testing.events');
const googArray = goog.require('goog.array');
const googJson = goog.require('goog.json');
const log = goog.require('goog.log');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/** @type {BroadcastPubSub} */
let broadcastPubSub;

/** @type {MockControl} */
let mockControl;

/** @type {MockClock} */
let mockClock;

/** @type {MockInterface} */
let mockStorage;

/** @type {MockInterface} */
let mockStorageCtor;

/** @type {StructsMap} */
let mockHtml5LocalStorage;

/** @type {MockInterface} */
let mockHTML5LocalStorageCtor;

/** @const {boolean} */
const isIe8 = userAgent.IE && userAgent.DOCUMENT_MODE == 8;

/**
 * Sends a remote storage event with special handling for IE8. With IE8 an
 * event is pushed to the event queue stored in local storage as a result of
 * behaviour by the mockHtml5LocalStorage instanciated when using IE8 an event
 * is automatically generated in the local browser context. For other browsers
 * this simply creates a new browser event.
 * @param {{'args': !Array<string>, 'timestamp': number}} data Value stored in
 *     localStorage which generated the remote event.
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function remoteStorageEvent(data) {
  if (!isIe8) {
    const event = new GoogTestingEvent('storage', window);
    /**
     * @suppress {strictMissingProperties,visibility} suppression added to
     * enable type checking
     */
    event.key = BroadcastPubSub.STORAGE_KEY_;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.newValue = googJson.serialize(data);
    events.fireBrowserEvent(event);
  } else {
    /** @suppress {visibility} suppression added to enable type checking */
    const uniqueKey = BroadcastPubSub.IE8_EVENTS_KEY_PREFIX_ + '1234567890';
    let ie8Events = mockHtml5LocalStorage.get(uniqueKey);
    if (ie8Events != null) {
      ie8Events = JSON.parse(ie8Events);
      // Events should never overlap in IE8 mode.
      if (ie8Events.length > 0 &&
          ie8Events[ie8Events.length - 1]['timestamp'] >= data['timestamp']) {
        /**
         * @suppress {strictMissingProperties,visibility} suppression added to
         * enable type checking
         */
        data['timestamp'] = ie8Events[ie8Events.length - 1]['timestamp'] +
            BroadcastPubSub.IE8_TIMESTAMP_UNIQUE_OFFSET_MS_;
      }
    } else {
      ie8Events = [];
    }
    ie8Events.push(data);
    // This will cause an event.
    mockHtml5LocalStorage.set(uniqueKey, googJson.serialize(ie8Events));
  }
}

testSuite({
  setUp() {
    mockControl = new MockControl();

    mockClock = new MockClock(true);
    // Time should never be 0...
    mockClock.tick();
    /** @suppress {missingRequire} */
    mockHTML5LocalStorageCtor = mockControl.createConstructorMock(
        goog.storage.mechanism, 'HTML5LocalStorage');

    mockHtml5LocalStorage = new StructsMap();

    // The builtin localStorage returns null instead of undefined.
    const originalGetFn =
        goog.bind(mockHtml5LocalStorage.get, mockHtml5LocalStorage);
    mockHtml5LocalStorage.get = (key) => {
      const value = originalGetFn(key);
      if (value === undefined) {
        return null;
      }
      return value;
    };
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockHtml5LocalStorage.key = (idx) => mockHtml5LocalStorage.getKeys()[idx];
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockHtml5LocalStorage.isAvailable = () => true;

    // IE has problems. IE9+ still dispatches storage events locally. IE8 also
    // doesn't include the key/value information. So for IE, every time we get a
    // "set" on localStorage we simulate for the appropriate browser.
    if (userAgent.IE) {
      const target = isIe8 ? document : window;
      const originalSetFn =
          goog.bind(mockHtml5LocalStorage.set, mockHtml5LocalStorage);
      mockHtml5LocalStorage.set = (key, value) => {
        originalSetFn(key, value);
        const event = new GoogTestingEvent('storage', target);
        if (!isIe8) {
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          event.key = key;
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          event.newValue = value;
        }
        events.fireBrowserEvent(event);
      };
    }
  },

  tearDown() {
    mockControl.$tearDown();
    mockClock.dispose();
    /** @suppress {checkTypes} suppression added to enable type checking */
    broadcastPubSub = undefined;
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testConstructor() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    assertNotNullNorUndefined(
        'BroadcastChannel instance must not be null', broadcastPubSub);
    assertTrue(
        'BroadcastChannel instance must have the expected type',
        broadcastPubSub instanceof BroadcastPubSub);
    assertArrayEquals(BroadcastPubSub.instances_, [broadcastPubSub]);
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
    assertNotNullNorUndefined(
        'Storage should not be undefined or null in broadcastPubSub.',
        broadcastPubSub.storage_);
    assertArrayEquals(BroadcastPubSub.instances_, []);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testConstructor_noLocalStorage() {
    mockHTML5LocalStorageCtor().$returns({
      isAvailable: function() {
        return false;
      },
    });
    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    assertNotNullNorUndefined(
        'BroadcastChannel instance must not be null', broadcastPubSub);
    assertTrue(
        'BroadcastChannel instance must have the expected type',
        broadcastPubSub instanceof BroadcastPubSub);
    assertArrayEquals(BroadcastPubSub.instances_, [broadcastPubSub]);
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
    assertNull(
        'Storage should be null in broadcastPubSub.', broadcastPubSub.storage_);
    assertArrayEquals(BroadcastPubSub.instances_, []);
  },

  /**
     Verify we cleanup after ourselves.
     @suppress {checkTypes,missingProperties,visibility} suppression added to
     enable type checking
   */
  testDispose() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const mockStorage = mockControl.createLooseMock(StorageStorage);

    const mockStorageCtor =
        mockControl.createConstructorMock(goog.storage, 'Storage');

    mockStorageCtor(mockHtml5LocalStorage).$returns(mockStorage);
    mockStorageCtor(mockHtml5LocalStorage).$returns(mockStorage);

    if (isIe8) {
      mockStorage.remove(BroadcastPubSub.IE8_EVENTS_KEY_);
    }

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    const broadcastPubSubExtra = new BroadcastPubSub();
    assertArrayEquals(
        BroadcastPubSub.instances_, [broadcastPubSub, broadcastPubSubExtra]);

    assertFalse(
        'BroadcastChannel extra instance must not have been disposed of',
        broadcastPubSubExtra.isDisposed());
    broadcastPubSubExtra.dispose();
    assertTrue(
        'BroadcastChannel extra instance must have been disposed of',
        broadcastPubSubExtra.isDisposed());
    assertFalse(
        'BroadcastChannel instance must not have been disposed of',
        broadcastPubSub.isDisposed());

    assertArrayEquals(BroadcastPubSub.instances_, [broadcastPubSub]);
    assertFalse(
        'BroadcastChannel instance must not have been disposed of',
        broadcastPubSub.isDisposed());
    broadcastPubSub.dispose();
    assertTrue(
        'BroadcastChannel instance must have been disposed of',
        broadcastPubSub.isDisposed());
    assertArrayEquals(BroadcastPubSub.instances_, []);
    mockControl.$verifyAll();
  },

  /**
   * Tests related to remote events that an instance of BroadcastChannel
   * should handle.
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testHandleRemoteEvent() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    foo('x', 'y').$times(2);

    const context = {'foo': 'bar'};
    const bar = recordFunction();

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    const eventData = {
      'args': ['someTopic', 'x', 'y'],
      'timestamp': Date.now()
    };

    broadcastPubSub.subscribe('someTopic', foo);
    broadcastPubSub.subscribe('someTopic', bar, context);

    remoteStorageEvent(eventData);
    mockClock.tick();

    assertEquals(1, bar.getCallCount());
    assertEquals(context, bar.getLastCall().getThis());
    assertArrayEquals(['x', 'y'], bar.getLastCall().getArguments());

    broadcastPubSub.unsubscribe('someTopic', foo);
    eventData['timestamp'] = Date.now();
    remoteStorageEvent(eventData);
    mockClock.tick();

    assertEquals(2, bar.getCallCount());
    assertEquals(context, bar.getLastCall().getThis());
    assertArrayEquals(['x', 'y'], bar.getLastCall().getArguments());

    broadcastPubSub.subscribe('someTopic', foo);
    broadcastPubSub.unsubscribe('someTopic', bar, context);
    eventData['timestamp'] = Date.now();
    remoteStorageEvent(eventData);
    mockClock.tick();

    assertEquals(2, bar.getCallCount());
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testHandleRemoteEventSubscribeOnce() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    foo('x', 'y');

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribeOnce('someTopic', foo);
    assertEquals(
        'BroadcastChannel must have one subscriber', 1,
        broadcastPubSub.getCount());

    remoteStorageEvent(
        {'args': ['someTopic', 'x', 'y'], 'timestamp': Date.now()});
    mockClock.tick();

    assertEquals(
        'BroadcastChannel must have no subscribers after receiving the event',
        0, broadcastPubSub.getCount());
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testHandleQueuedRemoteEvents() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    const bar = mockControl.createFunctionMock();

    foo('x', 'y');
    bar('d', 'c');

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('fooTopic', foo);
    broadcastPubSub.subscribe('barTopic', bar);

    let eventData = {'args': ['fooTopic', 'x', 'y'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);

    eventData = {'args': ['barTopic', 'd', 'c'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testHandleRemoteEventsUnsubscribe() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    const bar = mockControl.createFunctionMock();

    foo('x', 'y').$does(/**
                           @suppress {checkTypes} suppression added to enable
                           type checking
                         */
                        () => {
                          broadcastPubSub.unsubscribe('barTopic', bar);
                        });

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('fooTopic', foo);
    broadcastPubSub.subscribe('barTopic', bar);

    let eventData = {'args': ['fooTopic', 'x', 'y'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);
    mockClock.tick();

    eventData = {'args': ['barTopic', 'd', 'c'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testHandleRemoteEventsCalledOnce() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    foo('x', 'y');

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribeOnce('someTopic', foo);

    let eventData = {'args': ['someTopic', 'x', 'y'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);
    mockClock.tick();

    eventData = {'args': ['someTopic', 'x', 'y'], 'timestamp': Date.now()};
    remoteStorageEvent(eventData);
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testHandleRemoteEventNestedPublish() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo1 = mockControl.createFunctionMock();
    foo1().$does(() => {
      remoteStorageEvent({'args': ['bar'], 'timestamp': Date.now()});
    });
    const foo2 = mockControl.createFunctionMock();
    foo2();
    const bar1 = mockControl.createFunctionMock();
    bar1().$does(() => {
      broadcastPubSub.publish('baz');
    });
    const bar2 = mockControl.createFunctionMock();
    bar2();
    const baz1 = mockControl.createFunctionMock();
    baz1();
    const baz2 = mockControl.createFunctionMock();
    baz2();

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('foo', foo1);
    broadcastPubSub.subscribe('foo', foo2);
    broadcastPubSub.subscribe('bar', bar1);
    broadcastPubSub.subscribe('bar', bar2);
    broadcastPubSub.subscribe('baz', baz1);
    broadcastPubSub.subscribe('baz', baz2);

    remoteStorageEvent({'args': ['foo'], 'timestamp': Date.now()});
    mockClock.tick();
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
   * Local publish that originated from another instance of BroadcastChannel
   * in the same JavaScript context.
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testSecondInstancePublish() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage).$times(2);
    const foo = mockControl.createFunctionMock();
    foo('x', 'y');
    const context = {'foo': 'bar'};
    const bar = recordFunction();

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('someTopic', foo);
    broadcastPubSub.subscribe('someTopic', bar, context);

    const broadcastPubSub2 = new BroadcastPubSub();
    broadcastPubSub2.publish('someTopic', 'x', 'y');
    mockClock.tick();

    assertEquals(1, bar.getCallCount());
    assertEquals(context, bar.getLastCall().getThis());
    assertArrayEquals(['x', 'y'], bar.getLastCall().getArguments());

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSecondInstanceNestedPublish() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage).$times(2);
    const foo = mockControl.createFunctionMock();
    foo('m', 'n').$does(() => {
      broadcastPubSub.publish('barTopic', 'd', 'c');
    });
    const bar = mockControl.createFunctionMock();
    bar('d', 'c');

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('fooTopic', foo);

    const broadcastPubSub2 = new BroadcastPubSub();
    broadcastPubSub2.subscribe('barTopic', bar);
    broadcastPubSub2.publish('fooTopic', 'm', 'n');
    mockClock.tick();
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     Validate the localStorage data is being set as we expect.
     @suppress {checkTypes,missingProperties,visibility} suppression added to
     enable type checking
   */
  testLocalStorageData() {
    const topic = 'someTopic';
    const anotherTopic = 'anotherTopic';
    const now = Date.now();

    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const mockStorage = mockControl.createLooseMock(StorageStorage);

    const mockStorageCtor =
        mockControl.createConstructorMock(goog.storage, 'Storage');

    mockStorageCtor(mockHtml5LocalStorage).$returns(mockStorage);
    if (!isIe8) {
      mockStorage.set(
          BroadcastPubSub.STORAGE_KEY_,
          {'args': [topic, '10'], 'timestamp': now});
      mockStorage.remove(BroadcastPubSub.STORAGE_KEY_);
      mockStorage.set(
          BroadcastPubSub.STORAGE_KEY_,
          {'args': [anotherTopic, '13'], 'timestamp': now});
      mockStorage.remove(BroadcastPubSub.STORAGE_KEY_);
    } else {
      const firstEventArray = [{'args': [topic, '10'], 'timestamp': now}];
      /** @suppress {visibility} suppression added to enable type checking */
      const secondEventArray = [
        {'args': [topic, '10'], 'timestamp': now},
        {
          'args': [anotherTopic, '13'],
          'timestamp': now + BroadcastPubSub.IE8_TIMESTAMP_UNIQUE_OFFSET_MS_,
        },
      ];

      mockStorage.get(BroadcastPubSub.IE8_EVENTS_KEY_).$returns(null);
      mockStorage.set(
          BroadcastPubSub.IE8_EVENTS_KEY_,
          new ArgumentMatcher(
              (val) => mockmatchers.flexibleArrayMatcher(firstEventArray, val),
              'First event array'));

      // Make sure to clone or you're going to have a bad time.
      mockStorage.get(BroadcastPubSub.IE8_EVENTS_KEY_)
          .$returns(googArray.clone(firstEventArray));

      mockStorage.set(
          BroadcastPubSub.IE8_EVENTS_KEY_,
          new ArgumentMatcher(
              (val) => mockmatchers.flexibleArrayMatcher(secondEventArray, val),
              'Second event array'));

      mockStorage.remove(BroadcastPubSub.IE8_EVENTS_KEY_);
    }

    const fn = recordFunction();
    fn('10');
    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe(topic, fn);

    broadcastPubSub.publish(topic, '10');
    broadcastPubSub.publish(anotherTopic, '13');

    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testBrokenTimestamp() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fn = mockControl.createFunctionMock();
    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('someTopic', fn);

    remoteStorageEvent({'args': 'WAT?', 'timestamp': 'wat?'});
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
   * Test response to bad localStorage data.
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testBrokenEvent() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fn = mockControl.createFunctionMock();
    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('someTopic', fn);

    if (!isIe8) {
      const event = new GoogTestingEvent('storage', window);
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      event.key = 'FooBarBaz';
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      event.newValue = googJson.serialize({'keyby': 'word'});
      events.fireBrowserEvent(event);
    } else {
      /** @suppress {visibility} suppression added to enable type checking */
      const uniqueKey = BroadcastPubSub.IE8_EVENTS_KEY_PREFIX_ + '1234567890';
      // This will cause an event.
      mockHtml5LocalStorage.set(uniqueKey, 'Toothpaste!');
    }
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
   * The following tests are duplicated from pubsub because they depend
   * on functionality (mostly "publish") that has changed in BroadcastChannel.
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testPublish() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    foo('x', 'y');

    const context = {'foo': 'bar'};
    const bar = recordFunction();

    const baz = mockControl.createFunctionMock();
    baz('d', 'c');

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('someTopic', foo);
    broadcastPubSub.subscribe('someTopic', bar, context);
    broadcastPubSub.subscribe('anotherTopic', baz, context);

    broadcastPubSub.publish('someTopic', 'x', 'y');
    mockClock.tick();

    assertTrue(broadcastPubSub.unsubscribe('someTopic', foo));

    broadcastPubSub.publish('anotherTopic', 'd', 'c');
    broadcastPubSub.publish('someTopic', 'x', 'y');
    mockClock.tick();

    broadcastPubSub.subscribe('differentTopic', foo);

    broadcastPubSub.publish('someTopic', 'x', 'y');
    mockClock.tick();

    assertEquals(3, bar.getCallCount());
    bar.getCalls().forEach(call => {
      assertArrayEquals(['x', 'y'], call.getArguments());
      assertEquals(context, call.getThis());
    });
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testPublishEmptyTopic() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const foo = mockControl.createFunctionMock();
    foo();

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    broadcastPubSub.subscribe('someTopic', foo);
    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    broadcastPubSub.unsubscribe('someTopic', foo);
    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSubscribeWhilePublishing() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    // It's OK for a subscriber to add a new subscriber to its own topic,
    // but the newly added subscriber shouldn't be called until the next
    // publish cycle.

    const fn1 = mockControl.createFunctionMock();
    const fn2 = mockControl.createFunctionMock();
    fn1()
        .$does(/**
                  @suppress {checkTypes} suppression added to enable type
                  checking
                */
               () => {
                 broadcastPubSub.subscribe('someTopic', fn2);
               })
        .$times(2);
    fn2();

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('someTopic', fn1);
    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    assertEquals(
        'Topic must have two subscribers', 2,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    assertEquals(
        'Topic must have three subscribers', 3,
        broadcastPubSub.getCount('someTopic'));
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility,strictMissingProperties} suppression
     added to enable type checking
   */
  testUnsubscribeWhilePublishing() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    // It's OK for a subscriber to unsubscribe another subscriber from its
    // own topic, but the subscriber in question won't actually be removed
    // until after publishing is complete.

    const fn1 = mockControl.createFunctionMock();
    const fn2 = mockControl.createFunctionMock();
    const fn3 = mockControl.createFunctionMock();

    fn1().$does(/**
                   @suppress {checkTypes} suppression added to enable type
                   checking
                 */
                () => {
                  assertTrue(
                      'unsubscribe() must return true when removing a topic',
                      broadcastPubSub.unsubscribe('X', fn2));
                  assertEquals(
                      'Topic "X" must still have 3 subscribers', 3,
                      broadcastPubSub.getCount('X'));
                });
    fn2().$times(0);
    fn3().$does(/**
                   @suppress {checkTypes} suppression added to enable type
                   checking
                 */
                () => {
                  assertTrue(
                      'unsubscribe() must return true when removing a topic',
                      broadcastPubSub.unsubscribe('X', fn1));
                  assertEquals(
                      'Topic "X" must still have 3 subscribers', 3,
                      broadcastPubSub.getCount('X'));
                });

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('X', fn1);
    broadcastPubSub.subscribe('X', fn2);
    broadcastPubSub.subscribe('X', fn3);

    assertEquals(
        'Topic "X" must have 3 subscribers', 3, broadcastPubSub.getCount('X'));

    broadcastPubSub.publish('X');
    mockClock.tick();

    assertEquals(
        'Topic "X" must have 1 subscriber after publishing', 1,
        broadcastPubSub.getCount('X'));
    assertEquals(
        'BroadcastChannel must not have any subscriptions pending removal', 0,
        broadcastPubSub.pubSub_.pendingKeys_.length);
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility,strictMissingProperties} suppression
     added to enable type checking
   */
  testUnsubscribeSelfWhilePublishing() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    // It's OK for a subscriber to unsubscribe itself, but it won't actually
    // be removed until after publishing is complete.

    const fn = mockControl.createFunctionMock();
    fn().$does(/**
                  @suppress {checkTypes} suppression added to enable type
                  checking
                */
               () => {
                 assertTrue(
                     'unsubscribe() must return true when removing a topic',
                     broadcastPubSub.unsubscribe('someTopic', fn));
                 assertEquals(
                     'Topic must still have 1 subscriber', 1,
                     broadcastPubSub.getCount('someTopic'));
               });

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribe('someTopic', fn);
    assertEquals(
        'Topic must have 1 subscriber', 1,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    assertEquals(
        'Topic must have no subscribers after publishing', 0,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(
        'BroadcastChannel must not have any subscriptions pending removal', 0,
        broadcastPubSub.pubSub_.pendingKeys_.length);
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testNestedPublish() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const xFn1 = mockControl.createFunctionMock();
    const callback1 = function() {
      broadcastPubSub.publish('Y');
      broadcastPubSub.unsubscribe('X', callback1);
    };
    xFn1().$does(callback1);
    const xFn2 = mockControl.createFunctionMock();
    xFn2();

    const callback2 = function() {
      broadcastPubSub.unsubscribe('Y', callback2);
    };
    const yFn1 = mockControl.createFunctionMock();
    yFn1().$does(callback2);
    const yFn2 = mockControl.createFunctionMock();
    yFn2();

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribe('X', xFn1);
    broadcastPubSub.subscribe('X', xFn2);
    broadcastPubSub.subscribe('Y', yFn1);
    broadcastPubSub.subscribe('Y', yFn2);

    broadcastPubSub.publish('X');
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSubscribeOnce() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fn = recordFunction();

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);
    broadcastPubSub.subscribeOnce('someTopic', fn);

    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    assertEquals(
        'Topic must have no subscribers', 0,
        broadcastPubSub.getCount('someTopic'));

    let context = {'foo': 'bar'};
    broadcastPubSub.subscribeOnce('someTopic', fn, context);
    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(
        'Subscriber must not have been called yet', 1, fn.getCallCount());

    broadcastPubSub.publish('someTopic');
    mockClock.tick();

    assertEquals(
        'Topic must have no subscribers', 0,
        broadcastPubSub.getCount('someTopic'));
    assertEquals('Subscriber must have been called', 2, fn.getCallCount());
    assertEquals(context, fn.getLastCall().getThis());
    assertArrayEquals([], fn.getLastCall().getArguments());

    context = {'foo': 'bar'};
    broadcastPubSub.subscribeOnce('someTopic', fn, context);
    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertEquals('Subscriber must not have been called', 2, fn.getCallCount());

    broadcastPubSub.publish('someTopic', '17');
    mockClock.tick();

    assertEquals(
        'Topic must have no subscribers', 0,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(context, fn.getLastCall().getThis());
    assertEquals('Subscriber must have been called', 3, fn.getCallCount());
    assertArrayEquals(['17'], fn.getLastCall().getArguments());
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSubscribeOnce_boundFn() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fn = recordFunction();
    const context = {'foo': 'bar'};

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribeOnce('someTopic', goog.bind(fn, context));
    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertNull('Subscriber must not have been called yet', fn.getLastCall());

    broadcastPubSub.publish('someTopic', '17');
    mockClock.tick();
    assertEquals(
        'Topic must have no subscribers', 0,
        broadcastPubSub.getCount('someTopic'));
    assertEquals('Subscriber must have been called', 1, fn.getCallCount());
    assertEquals(
        'Must receive correct argument.', '17',
        fn.getLastCall().getArgument(0));
    assertEquals(
        'Must have appropriate context.', context, fn.getLastCall().getThis());

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSubscribeOnce_partialFn() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fullFn = mockControl.createFunctionMock();
    fullFn(true, '17');

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribeOnce('someTopic', goog.partial(fullFn, true));
    assertEquals(
        'Topic must have one subscriber', 1,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic', '17');
    mockClock.tick();

    assertEquals(
        'Topic must have no subscribers', 0,
        broadcastPubSub.getCount('someTopic'));
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility,strictMissingProperties} suppression
     added to enable type checking
   */
  testSelfResubscribe() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const resubscribeFn = mockControl.createFunctionMock();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const resubscribe = () => {
      broadcastPubSub.subscribeOnce('someTopic', resubscribeFn);
    };
    resubscribeFn('foo').$does(resubscribe);
    resubscribeFn('bar').$does(resubscribe);
    resubscribeFn('baz').$does(resubscribe);

    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    broadcastPubSub.subscribeOnce('someTopic', resubscribeFn);
    assertEquals(
        'Topic must have 1 subscriber', 1,
        broadcastPubSub.getCount('someTopic'));

    broadcastPubSub.publish('someTopic', 'foo');
    mockClock.tick();
    assertEquals(
        'Topic must have 1 subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(
        'BroadcastChannel must not have any pending unsubscribe keys', 0,
        broadcastPubSub.pubSub_.pendingKeys_.length);

    broadcastPubSub.publish('someTopic', 'bar');
    mockClock.tick();
    assertEquals(
        'Topic must have 1 subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(
        'BroadcastChannel must not have any pending unsubscribe keys', 0,
        broadcastPubSub.pubSub_.pendingKeys_.length);

    broadcastPubSub.publish('someTopic', 'baz');
    mockClock.tick();
    assertEquals(
        'Topic must have 1 subscriber', 1,
        broadcastPubSub.getCount('someTopic'));
    assertEquals(
        'BroadcastChannel must not have any pending unsubscribe keys', 0,
        broadcastPubSub.pubSub_.pendingKeys_.length);
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testClear() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);
    const fn = mockControl.createFunctionMock();
    mockControl.$replayAll();
    broadcastPubSub = new BroadcastPubSub();
    log.setLevel(broadcastPubSub.logger_, Level.OFF);

    ['V', 'W', 'X', 'Y', 'Z'].forEach(
        /**
           @suppress {checkTypes} suppression added to enable type checking
         */
        topic => {
          broadcastPubSub.subscribe(topic, fn);
        });
    assertEquals(
        'BroadcastChannel must have 5 subscribers', 5,
        broadcastPubSub.getCount());

    broadcastPubSub.clear('W');
    assertEquals(
        'BroadcastChannel must have 4 subscribers', 4,
        broadcastPubSub.getCount());

    ['X', 'Y'].forEach(topic => {
      broadcastPubSub.clear(topic);
    });
    assertEquals(
        'BroadcastChannel must have 2 subscriber', 2,
        broadcastPubSub.getCount());

    broadcastPubSub.clear();
    assertEquals(
        'BroadcastChannel must have no subscribers', 0,
        broadcastPubSub.getCount());
    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNestedSubscribeOnce() {
    mockHTML5LocalStorageCtor().$returns(mockHtml5LocalStorage);

    const x = mockControl.createFunctionMock();
    const y = mockControl.createFunctionMock();

    x().$times(1);
    y().$does(() => {
      broadcastPubSub.publish('X');
      broadcastPubSub.publish('X');
    });

    mockControl.$replayAll();

    broadcastPubSub = new BroadcastPubSub();
    broadcastPubSub.subscribeOnce('X', x);
    broadcastPubSub.subscribe('Y', y);
    broadcastPubSub.publish('Y');
    mockClock.tick();

    broadcastPubSub.dispose();
    mockControl.$verifyAll();
  },
});
