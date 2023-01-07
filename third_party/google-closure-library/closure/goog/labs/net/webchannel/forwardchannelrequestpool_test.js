/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for ForwardChannelRequestPool.
 * @suppress {accessControls} Private methods are accessed for test purposes.
 */

goog.module('goog.labs.net.webChannel.ForwardChannelRequestPoolTest');
goog.setTestOnly('goog.labs.net.webChannel.ForwardChannelRequestPoolTest');

const ChannelRequest = goog.require('goog.labs.net.webChannel.ChannelRequest');
const ForwardChannelRequestPool = goog.require('goog.labs.net.webChannel.ForwardChannelRequestPool');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const testSuite = goog.require('goog.testing.testSuite');

const propertyReplacer = new PropertyReplacer();
const req = new ChannelRequest(null, null);

testSuite({
  shouldRunTests: function() {
    return ChannelRequest.supportsXhrStreaming();
  },

  setUp: function() {},

  tearDown: function() {
    propertyReplacer.reset();
  },

  stubSpdyCheck: function(spdyEnabled) {
    propertyReplacer.set(
        ForwardChannelRequestPool, 'isSpdyOrHttp2Enabled_', function() {
          return spdyEnabled;
        });
  },

  testSpdyEnabled: /**
                      @suppress {globalThis} suppression added to enable type
                      checking
                    */
      function() {
        this.stubSpdyCheck(true);

        const pool = new ForwardChannelRequestPool();
        assertFalse(pool.isFull());
        assertEquals(0, pool.getRequestCount());
        pool.addRequest(req);
        assertTrue(pool.hasPendingRequest());
        assertTrue(pool.hasRequest(req));
        pool.removeRequest(req);
        assertFalse(pool.hasPendingRequest());

        for (let i = 0; i < pool.getMaxSize(); i++) {
          pool.addRequest(new ChannelRequest(null, null));
        }
        assertTrue(pool.isFull());

        // do not fail
        pool.addRequest(req);
        assertTrue(pool.isFull());
      },

  testSpdyNotEnabled: /**
                         @suppress {globalThis} suppression added to enable
                         type checking
                       */
      function() {
        this.stubSpdyCheck(false);

        const pool = new ForwardChannelRequestPool();
        assertFalse(pool.isFull());
        assertEquals(0, pool.getRequestCount());
        pool.addRequest(req);
        assertTrue(pool.hasPendingRequest());
        assertTrue(pool.hasRequest(req));
        assertTrue(pool.isFull());
        pool.removeRequest(req);
        assertFalse(pool.hasPendingRequest());

        // do not fail
        pool.addRequest(req);
        assertTrue(pool.isFull());
      },

  testApplyClientProtocol: /**
                              @suppress {globalThis} suppression added to
                              enable type checking
                            */
      function() {
        this.stubSpdyCheck(false);

        let pool = new ForwardChannelRequestPool();
        assertEquals(1, pool.getMaxSize());
        pool.applyClientProtocol('spdy/3');
        assertTrue(pool.getMaxSize() > 1);
        pool.applyClientProtocol('foo-bar');  // no effect
        assertTrue(pool.getMaxSize() > 1);

        pool = new ForwardChannelRequestPool();
        assertEquals(1, pool.getMaxSize());
        pool.applyClientProtocol('quic/x');
        assertTrue(pool.getMaxSize() > 1);

        pool = new ForwardChannelRequestPool();
        assertEquals(1, pool.getMaxSize());
        pool.applyClientProtocol('h2');
        assertTrue(pool.getMaxSize() > 1);

        this.stubSpdyCheck(true);

        pool = new ForwardChannelRequestPool();
        assertTrue(pool.getMaxSize() > 1);
        pool.applyClientProtocol('foo/3');  // no effect
        assertTrue(pool.getMaxSize() > 1);
      },

  testPendingMessagesWithSpdyDisabled: /**
                                          @suppress {globalThis} suppression
                                          added to enable type checking
                                        */
      function() {
        this.stubSpdyCheck(false);

        const pool = new ForwardChannelRequestPool();
        assertEquals(1, pool.getMaxSize());
        assertEquals(0, pool.getPendingMessages().length);

        let req = new ChannelRequest(null, null);
        pool.addRequest(req);

        assertEquals(0, pool.getPendingMessages().length);

        req.setPendingMessages([null, null]);  // null represents the message
        assertEquals(2, pool.getPendingMessages().length);

        req = new ChannelRequest(null, null);
        req.setPendingMessages([null]);
        pool.addRequest(req);
        assertEquals(1, pool.getPendingMessages().length);

        pool.removeRequest(req);
        assertEquals(0, pool.getPendingMessages().length);
      },

  testCanelAndPendingMessagesWithSpdyDisabled: /**
                                                  @suppress {globalThis}
                                                  suppression added to enable
                                                  type checking
                                                */
      function() {
        this.stubSpdyCheck(false);

        const pool = new ForwardChannelRequestPool();

        const req = new ChannelRequest(null, null);
        req.setPendingMessages([null, null]);  // null represents the
                                               // message
        pool.addRequest(req);
        assertEquals(2, pool.getPendingMessages().length);

        const req1 = new ChannelRequest(null, null);
        pool.addRequest(req1);
        req1.setPendingMessages([null]);
        assertEquals(1, pool.getPendingMessages().length);

        pool.cancel();
        assertEquals(0, pool.getRequestCount());

        assertEquals(1, pool.getPendingMessages().length);
      },

  testAddPendingMessagesWithSpdyEnabled: /**
                                            @suppress {globalThis} suppression
                                            added to enable type checking
                                          */
      function() {
        this.stubSpdyCheck(false);

        const pool = new ForwardChannelRequestPool();

        pool.addPendingMessages([null, null]);
        assertEquals(2, pool.getPendingMessages().length);

        const req = new ChannelRequest(null, null);
        req.setPendingMessages([null, null]);  // null represents the
                                               // message
        pool.addRequest(req);

        assertEquals(4, pool.getPendingMessages().length);

        pool.addPendingMessages([null]);
        assertEquals(5, pool.getPendingMessages().length);
      },

  testPendingMessagesWithSpdyEnabled: /**
                                         @suppress {globalThis} suppression
                                         added to enable type checking
                                       */
      function() {
        this.stubSpdyCheck(true);

        const pool = new ForwardChannelRequestPool();
        assertTrue(pool.getMaxSize() > 1);
        assertEquals(0, pool.getPendingMessages().length);

        const req = new ChannelRequest(null, null);
        pool.addRequest(req);

        assertEquals(0, pool.getPendingMessages().length);

        req.setPendingMessages([null, null]);  // null represents the message
        assertEquals(2, pool.getPendingMessages().length);

        const req1 = new ChannelRequest(null, null);
        pool.addRequest(req1);
        assertEquals(2, pool.getPendingMessages().length);
        req1.setPendingMessages([null]);
        assertEquals(3, pool.getPendingMessages().length);

        pool.removeRequest(req1);
        assertEquals(2, pool.getPendingMessages().length);

        pool.removeRequest(req);
        assertEquals(0, pool.getPendingMessages().length);
      },

  testCanelAndPendingMessagesWithSpdyEnabled: /**
                                                 @suppress {globalThis}
                                                 suppression added to enable
                                                 type checking
                                               */
      function() {
        this.stubSpdyCheck(true);

        const pool = new ForwardChannelRequestPool();

        const req = new ChannelRequest(null, null);
        req.setPendingMessages([null, null]);  // null represents the
                                               // message
        pool.addRequest(req);

        const req1 = new ChannelRequest(null, null);
        pool.addRequest(req1);
        req1.setPendingMessages([null]);

        assertEquals(3, pool.getPendingMessages().length);

        pool.cancel();
        assertEquals(0, pool.getRequestCount());

        assertEquals(3, pool.getPendingMessages().length);
      },

  testAddAndSetPendingMessagesWithSpdyEnabled: /**
                                                  @suppress {globalThis}
                                                  suppression added to enable
                                                  type checking
                                                */
      function() {
        this.stubSpdyCheck(true);

        const pool = new ForwardChannelRequestPool();

        pool.addPendingMessages([null, null]);
        assertEquals(2, pool.getPendingMessages().length);

        const req = new ChannelRequest(null, null);
        req.setPendingMessages([null, null]);  // null represents the
                                               // message
        pool.addRequest(req);

        const req1 = new ChannelRequest(null, null);
        pool.addRequest(req1);
        req1.setPendingMessages([null]);

        assertEquals(5, pool.getPendingMessages().length);

        pool.addPendingMessages([null, null]);
        assertEquals(7, pool.getPendingMessages().length);
      },

  testClearPendingMessages: /**
                               @suppress {globalThis} suppression added to
                               enable type checking
                             */
      function() {
        this.stubSpdyCheck(true);

        const pool = new ForwardChannelRequestPool();

        pool.addPendingMessages([null, null]);
        assertEquals(2, pool.getPendingMessages().length);

        pool.clearPendingMessages();
        assertEquals(0, pool.getPendingMessages().length);
      },
});
