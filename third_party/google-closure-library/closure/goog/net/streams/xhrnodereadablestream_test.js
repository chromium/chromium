/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.XhrNodeReadableStreamTest');
goog.setTestOnly();

const NodeReadableStream = goog.require('goog.net.streams.NodeReadableStream');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const testSuite = goog.require('goog.testing.testSuite');
const {XhrNodeReadableStream} = goog.require('goog.net.streams.xhrNodeReadableStream');
const {XhrStreamReaderStatus} = goog.require('goog.net.streams.xhrStreamReader');

let xhrReader;
let xhrStream;

const EventType = NodeReadableStream.EventType;

let propertyReplacer;

/**
 * Constructs a duck-type XhrStreamReader to simulate xhr events.
 * @final
 */
class MockXhrStreamReader {
  constructor() {
    // mocked API

    this.setStatusHandler = function(handler) {
      /**
       * @suppress {checkTypes,missingProperties} suppression added to enable
       * type checking
       */
      this.statusHandler_ = handler;
    };

    this.setDataHandler = function(handler) {
      /**
       * @suppress {checkTypes,missingProperties} suppression added to enable
       * type checking
       */
      this.dataHandler_ = handler;
    };

    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    this.getStatus = function() {
      return this.status_;
    };

    // simulated events

    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    this.onData = function(messages) {
      this.dataHandler_(messages);
    };

    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    this.onStatus = function(status) {
      /**
       * @suppress {checkTypes,missingProperties} suppression added to enable
       * type checking
       */
      this.status_ = status;
      this.statusHandler_();
    };
  }
}

testSuite({
  setUp() {
    xhrReader = new MockXhrStreamReader();
    /** @suppress {checkTypes} suppression added to enable type checking */
    xhrStream = new XhrNodeReadableStream(xhrReader);

    propertyReplacer = new PropertyReplacer();
    propertyReplacer.replace(xhrStream, 'handleError_', (message) => {
      // the real XhrNodeReadableStream class ignores any error thrown
      // from inside a callback function, but we want to see those assert
      // errors thrown by the test callback function installed by unit tests
      fail(message);
    });
  },

  tearDown() {
    propertyReplacer.reset();
  },

  testOneDataCallback() {
    let delivered = false;

    const callback = (message) => {
      delivered = true;
      assertEquals('a', message.a);
    };

    xhrStream.on(EventType.DATA, callback);

    xhrReader.onData([{a: 'a'}]);
    assertTrue(delivered);
  },

  testMultipleDataCallbacks() {
    let delivered = 0;

    const callback = (message) => {
      delivered++;
      assertEquals('a', message.a);
    };

    xhrStream.on(EventType.DATA, callback);
    xhrStream.on(EventType.DATA, callback);

    xhrReader.onData([{a: 'a'}]);
    assertEquals(2, delivered);
  },

  testOrderedDataCallbacks() {
    let delivered = 0;

    const callback1 = (message) => {
      assertEquals(0, delivered++);
      assertEquals('a', message.a);
    };

    const callback2 = (message) => {
      assertEquals(1, delivered++);
      assertEquals('a', message.a);
    };

    xhrStream.on(EventType.DATA, callback1);
    xhrStream.on(EventType.DATA, callback2);

    xhrReader.onData([{a: 'a'}]);
    assertEquals(2, delivered);
  },

  testMultipleMessagesCallbacks() {
    let delivered = 0;

    const callback1 = (message) => {
      if (message.a) {
        assertEquals(0, delivered++);
        assertEquals('a', message.a);
      } else if (message.b) {
        assertEquals(2, delivered++);
        assertEquals('b', message.b);
      } else {
        fail('unexpected message');
      }
    };

    const callback2 = (message) => {
      if (message.a) {
        assertEquals(1, delivered++);
        assertEquals('a', message.a);
      } else if (message.b) {
        assertEquals(3, delivered++);
        assertEquals('b', message.b);
      } else {
        fail('unexpected message');
      }
    };

    xhrStream.on(EventType.DATA, callback1);
    xhrStream.on(EventType.DATA, callback2);

    xhrReader.onData([{a: 'a'}, {b: 'b'}]);
    assertEquals(4, delivered);
  },

  testMultipleMessagesWithOnceCallbacks() {
    let delivered = 0;

    const callback1 = (message) => {
      if (message.a) {
        assertEquals(0, delivered++);
        assertEquals('a', message.a);
      } else if (message.b) {
        assertEquals(1, delivered++);
        assertEquals('b', message.b);
      } else if (message.c) {
        assertEquals(4, delivered++);
        assertEquals('c', message.c);
      } else {
        fail('unexpected message');
      }
    };

    const callback2 = (message) => {
      if (message.a) {
        assertEquals(2, delivered++);
        assertEquals('a', message.a);
      } else if (message.b) {
        assertEquals(3, delivered++);
        assertEquals('b', message.b);
      } else {
        fail('unexpected message');
      }
    };

    xhrStream.on(EventType.DATA, callback1);
    xhrStream.once(EventType.DATA, callback2);

    xhrReader.onData([{a: 'a'}, {b: 'b'}]);
    assertEquals(4, delivered);

    xhrReader.onData([{c: 'c'}]);
    assertEquals(5, delivered);
  },

  testMultipleMessagesWithRemovedCallbacks() {
    let delivered = 0;

    const callback1 = (message) => {
      if (message.a) {
        assertEquals(0, delivered++);
        assertEquals('a', message.a);
      } else if (message.c) {
        assertEquals(3, delivered++);
        assertEquals('c', message.c);
      } else {
        fail('unexpected message');
      }
    };

    const callback2 = (message) => {
      if (message.a) {
        assertEquals(1, delivered++);
        assertEquals('a', message.a);
      } else if (message.b) {
        assertEquals(2, delivered++);
        assertEquals('b', message.b);
      } else {
        fail('unexpected message');
      }
    };

    xhrStream.on(EventType.DATA, callback1);
    xhrStream.once(EventType.DATA, callback2);

    xhrReader.onData([{a: 'a'}]);
    assertEquals(2, delivered);

    xhrStream.removeListener(EventType.DATA, callback1);
    xhrStream.once(EventType.DATA, callback2);

    xhrReader.onData([{b: 'b'}]);
    assertEquals(3, delivered);

    xhrStream.on(EventType.DATA, callback1);
    xhrStream.once(EventType.DATA, callback2);
    xhrStream.removeListener(EventType.DATA, callback2);

    xhrReader.onData([{c: 'c'}]);
    assertEquals(4, delivered);

    xhrStream.removeListener(EventType.DATA, callback1);
    xhrReader.onData([{d: 'd'}]);
    assertEquals(4, delivered);
  },

  testOrderedStatusCallbacks() {
    checkStatusMapping(XhrStreamReaderStatus.ACTIVE, EventType.READABLE);

    checkStatusMapping(XhrStreamReaderStatus.BAD_DATA, EventType.ERROR);
    checkStatusMapping(
        XhrStreamReaderStatus.HANDLER_EXCEPTION, EventType.ERROR);
    checkStatusMapping(XhrStreamReaderStatus.NO_DATA, EventType.ERROR);
    checkStatusMapping(XhrStreamReaderStatus.TIMEOUT, EventType.ERROR);
    checkStatusMapping(XhrStreamReaderStatus.XHR_ERROR, EventType.ERROR);

    checkStatusMapping(XhrStreamReaderStatus.CANCELLED, EventType.CLOSE);

    checkStatusMapping(XhrStreamReaderStatus.SUCCESS, EventType.END);

    function checkStatusMapping(status, event) {
      let delivered = 0;

      const callback1 = () => {
        if (delivered == 0) {
          delivered++;
        } else if (delivered == 2) {
          delivered++;
        } else {
          fail('unexpected status change');
        }
        assertEquals(status, xhrReader.getStatus());
      };

      const callback2 = () => {
        assertEquals(1, delivered++);
        assertEquals(status, xhrReader.getStatus());
      };

      xhrStream.on(event, callback1);
      xhrStream.once(event, callback2);

      xhrReader.onStatus(status);
      assertEquals(2, delivered);

      xhrReader.onStatus(status);
      assertEquals(3, delivered);

      xhrStream.removeListener(event, callback1);

      xhrReader.onStatus(status);
      assertEquals(3, delivered);
    }
  },

  testOrderedStatusMultipleCallbacks() {
    checkStatusMapping(XhrStreamReaderStatus.ACTIVE, EventType.READABLE);

    function checkStatusMapping(status, event) {
      let delivered = 0;

      const callback1 = () => {
        if (delivered == 0) {
          delivered++;
        } else if (delivered == 2) {
          delivered++;
        } else if (delivered == 4) {
          delivered++;
        } else {
          fail('unexpected status change');
        }
        assertEquals(status, xhrReader.getStatus());
      };

      const callback2 = () => {
        if (delivered == 1) {
          delivered++;
        } else if (delivered == 3) {
          delivered++;
        } else if (delivered == 5) {
          delivered++;
        } else if (delivered == 6) {
          delivered++;
        } else {
          fail('unexpected status change');
        }
        assertEquals(status, xhrReader.getStatus());
      };

      xhrStream.on(event, callback1);
      xhrStream.on(event, callback2);

      xhrStream.once(event, callback1);
      xhrStream.once(event, callback2);

      xhrReader.onStatus(status);
      assertEquals(4, delivered);

      xhrReader.onStatus(status);
      assertEquals(6, delivered);

      xhrStream.removeListener(event, callback1);

      xhrReader.onStatus(status);
      assertEquals(7, delivered);
    }
  },
});
