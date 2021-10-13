/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for WebChannelBase.@suppress {accessControls}
 * Private methods are accessed for test purposes.
 */

goog.module('goog.labs.net.webChannel.webChannelBaseTest');
goog.setTestOnly();

const ChannelRequest = goog.require('goog.labs.net.webChannel.ChannelRequest');
const ForwardChannelRequestPool = goog.require('goog.labs.net.webChannel.ForwardChannelRequestPool');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Stat = goog.require('goog.labs.net.webChannel.requestStats.Stat');
const StructsMap = goog.require('goog.structs.Map');
const Timer = goog.require('goog.Timer');
const Uri = goog.requireType('goog.Uri');
const WebChannelBase = goog.require('goog.labs.net.webChannel.WebChannelBase');
const WebChannelBaseTransport = goog.require('goog.labs.net.webChannel.WebChannelBaseTransport');
const WebChannelDebug = goog.require('goog.labs.net.webChannel.WebChannelDebug');
const Wire = goog.require('goog.labs.net.webChannel.Wire');
const XhrIo = goog.requireType('goog.net.XhrIo');
const asserts = goog.require('goog.testing.asserts');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const googJson = goog.require('goog.json');
const netUtils = goog.require('goog.labs.net.webChannel.netUtils');
const requestStats = goog.require('goog.labs.net.webChannel.requestStats');
const testSuite = goog.require('goog.testing.testSuite');

/** Delay between a network failure and the next network request. */
const RETRY_TIME = 1000;

/** A really long time - used to make sure no more timeouts will fire. */
const ALL_DAY_MS = 1000 * 60 * 60 * 24;

const stubs = new PropertyReplacer();

let channel;
let deliveredMaps;
let handler;
let mockClock;
let gotError;
let numStatEvents;
let lastStatEvent;
let numTimingEvents;
let lastPostSize;
let lastPostRtt;
let lastPostRetryCount;

// Set to true to see the channel debug output in the browser window.
const debug = false;
// Debug message to print out when debug is true.
let debugMessage = '';

function debugToWindow(message) {
  if (debug) {
    debugMessage += `${message}<br>`;
    dom.getElement('debug').innerHTML = debugMessage;
  }
}

/**
 * Stubs netUtils to always time out. It maintains the
 * contract given by netUtils.testNetwork, but always
 * times out (calling callback(false)).
 * stubNetUtils should be called in tests that require it before
 * a call to testNetwork happens. It is reset at tearDown.
 */
function stubNetUtils() {
  stubs.set(netUtils, 'testLoadImage', (url, timeout, callback) => {
    Timer.callOnce(goog.partial(callback, false), timeout);
  });
}

/**
 * Stubs
 * ForwardChannelRequestPool.isSpdyOrHttp2Enabled_ to
 * manage the max pool size for the forward channel.
 * @param {boolean} spdyEnabled Whether SPDY is enabled for the test.
 */
function stubSpdyCheck(spdyEnabled) {
  stubs.set(
      ForwardChannelRequestPool, 'isSpdyOrHttp2Enabled_', () => spdyEnabled);
}

/**
 * Mock ChannelRequest.
 * @final
 */
class MockChannelRequest {
  constructor(
      channel, channelDebug, sessionId = undefined, requestId = undefined,
      retryId = undefined) {
    this.channel_ = channel;
    this.channelDebug_ = channelDebug;
    this.sessionId_ = sessionId;
    this.requestId_ = requestId;
    this.successful_ = true;
    this.lastError_ = null;
    this.lastStatusCode_ = 200;

    // For debugging, keep track of whether this is a back or forward channel.
    this.isBack = !!(requestId == 'rpc');
    this.isForward = !this.isBack;

    this.pendingMessages_ = [];

    this.postData_ = null;
    this.requestStartTime_ = null;
  }

  /** @param {?Object} extraHeaders The HTTP headers. */
  setExtraHeaders(extraHeaders) {}

  /** @param {number} timeout The timeout in MS for when we fail the request. */
  setTimeout(timeout) {}

  /**
   * @param {number} throttle The throttle in ms. A value of zero indicates no
   *     throttle.
   */
  setReadyStateChangeThrottle(throttle) {}

  /**
   * @param {?Uri} uri The uri of the request.
   * @param {?string} postData The data for the post body.
   * @param {boolean} decodeChunks Whether to the result is expected to be
   *     encoded for chunking and thus requires decoding.
   */
  xmlHttpPost(uri, postData, decodeChunks) {
    this.channelDebug_.debug(`---> POST: ${uri}, ${postData}, ${decodeChunks}`);
    this.postData_ = postData;
    this.requestStartTime_ = Date.now();
  }

  /**
   * @param {?Uri} uri The uri of the request.
   * @param {boolean} decodeChunks Whether to the result is expected to be
   *     encoded for chunking and thus requires decoding.
   * @param {?string} hostPrefix The host prefix, if we might be using a
   *     secondary domain. Note that it should also be in the URL, adding this
   *     won't cause it to be added to the URL.
   */
  xmlHttpGet(uri, decodeChunks, hostPrefix) {
    this.channelDebug_.debug(
        `<--- GET: ${uri}, ${decodeChunks}, ${hostPrefix}`);
    this.requestStartTime_ = Date.now();
  }

  /** @param {?Uri} uri The uri to send a request to. */
  sendCloseRequest(uri) {
    this.requestStartTime_ = Date.now();
  }

  /** Cancel. */
  cancel() {
    this.successful_ = false;
  }

  /** @return {boolean} */
  getSuccess() {
    return this.successful_;
  }

  /** @return {?ChannelRequest.Error} The last error. */
  getLastError() {
    return this.lastError_;
  }

  /** @return {number} The status code of the last request. */
  getLastStatusCode() {
    return this.lastStatusCode_;
  }

  /** @return {string|undefined} The session ID. */
  getSessionId() {
    return this.sessionId_;
  }

  /** @return {string|number|undefined} The request ID. */
  getRequestId() {
    return this.requestId_;
  }

  /** @return {?string} The POST data provided by the request initiator. */
  getPostData() {
    return this.postData_;
  }

  /**
   * @return {?number} The time the request started, as returned by Date.now().
   */
  getRequestStartTime() {
    return this.requestStartTime_;
  }

  /** @return {?XhrIo} Any XhrIo request created for this object. */
  getXhr() {
    return null;
  }

  /**
   * @param {!Array<?Wire.QueuedMap>} messages The pending messages for this
   *     request.
   */
  setPendingMessages(messages) {
    this.pendingMessages_ = messages;
  }

  /** @return {!Array<?Wire.QueuedMap>} The pending messages for this request. */
  getPendingMessages() {
    return this.pendingMessages_;
  }

  /** @return {boolean} true if X_HTTP_INITIAL_RESPONSE has been handled. */
  isInitialResponseDecoded() {
    return false;
  }

  /** Decodes X_HTTP_INITIAL_RESPONSE if present. */
  setDecodeInitialResponse() {}
}

function getSingleForwardRequest() {
  /** @suppress {visibility} suppression added to enable type checking */
  const pool = channel.forwardChannelRequestPool_;
  if (!pool.hasPendingRequest()) {
    return null;
  }
  return pool.request_ || pool.requestPool_.getValues()[0];
}

/**
 * Helper function to return a formatted string representing an array of maps.
 */
function formatArrayOfMaps(arrayOfMaps) {
  const result = [];
  for (let i = 0; i < arrayOfMaps.length; i++) {
    const map = arrayOfMaps[i];

    if (Object.getPrototypeOf(map.map) === Object.prototype) {  // Object map
      for (const key in map.map) {
        const tmp =
            key + ':' + map.map[key] + (map.context ? ':' + map.context : '');
        result.push(tmp);
      }
    } else if (
        typeof map.map.keys === 'function' &&
        typeof map.map.get === 'function') {  // MapLike
      for (const key of map.map.keys()) {
        const tmp = key + ':' + map.map.get(key) +
            (map.context ? ':' + map.context : '');
        result.push(tmp);
      }
    } else {
      throw new Error('Unknown input type for map: ' + String(map));
    }
  }
  return result.join(', ');
}

/**
 * @param {number=} serverVersion
 * @param {string=} hostPrefix
 * @param {string=} opt_uriPrefix
 * @param {boolean=} spdyEnabled
 */
function connectForwardChannel(
    serverVersion = undefined, hostPrefix = undefined, opt_uriPrefix,
    spdyEnabled = undefined) {
  stubSpdyCheck(!!spdyEnabled);
  const uriPrefix = opt_uriPrefix || '';
  channel.connect(`${uriPrefix}/bind`, null);
  mockClock.tick(0);
  completeForwardChannel(serverVersion, hostPrefix);
}

/**
 * @param {number=} serverVersion
 * @param {string=} hostPrefix
 * @param {string=} uriPrefix
 * @param {boolean=} spdyEnabled
 */
function connect(
    serverVersion = undefined, hostPrefix = undefined, uriPrefix = undefined,
    spdyEnabled = undefined) {
  connectForwardChannel(serverVersion, hostPrefix, uriPrefix, spdyEnabled);
  completeBackChannel();
}

function disconnect() {
  channel.disconnect();
  mockClock.tick(0);
}

/**
 * @param {number=} serverVersion
 * @param {string=} hostPrefix
 */
function completeForwardChannel(
    serverVersion = undefined, hostPrefix = undefined) {
  const responseData = '[[0,["c","1234567890ABCDEF",' +
      (hostPrefix ? `"${hostPrefix}"` : 'null') +
      (serverVersion ? `,${serverVersion}` : '') + ']]]';
  channel.onRequestData(getSingleForwardRequest(), responseData);
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function completeBackChannel() {
  channel.onRequestData(channel.backChannelRequest_, '[[1,["foo"]]]');
  channel.onRequestComplete(channel.backChannelRequest_);
  mockClock.tick(0);
}

function responseDone() {
  channel.onRequestData(getSingleForwardRequest(), '[1,0,0]');  // mock data
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

/**
 * @param {number=} lastArrayIdSentFromServer
 * @param {number=} outstandingDataSize
 */
function responseNoBackchannel(
    lastArrayIdSentFromServer = undefined, outstandingDataSize = undefined) {
  const responseData =
      googJson.serialize([0, lastArrayIdSentFromServer, outstandingDataSize]);
  channel.onRequestData(getSingleForwardRequest(), responseData);
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

function response(lastArrayIdSentFromServer, outstandingDataSize) {
  const responseData =
      googJson.serialize([1, lastArrayIdSentFromServer, outstandingDataSize]);
  channel.onRequestData(getSingleForwardRequest(), responseData);
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function receive(data) {
  channel.onRequestData(channel.backChannelRequest_, `[[1,${data}]]`);
  channel.onRequestComplete(channel.backChannelRequest_);
  mockClock.tick(0);
}

function responseTimeout() {
  getSingleForwardRequest().lastError_ = ChannelRequest.Error.TIMEOUT;
  getSingleForwardRequest().successful_ = false;
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

/** @param {number=} statusCode */
function responseRequestFailed(statusCode = undefined) {
  getSingleForwardRequest().lastError_ = ChannelRequest.Error.STATUS;
  getSingleForwardRequest().lastStatusCode_ = statusCode || 503;
  getSingleForwardRequest().successful_ = false;
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

function responseUnknownSessionId() {
  getSingleForwardRequest().lastError_ =
      ChannelRequest.Error.UNKNOWN_SESSION_ID;
  getSingleForwardRequest().successful_ = false;
  channel.onRequestComplete(getSingleForwardRequest());
  mockClock.tick(0);
}

/**
 * Enum for map types to test.
 * @enum {number}
 */
const MapTypes = {
  OBJECT_MAP: 0,
  STRUCTS_MAP: 1,
  ES6_MAP: 2,
};

/**
 * @param {string} key
 * @param {string} value
 * @param {string=} context
 * @param {!MapTypes=} mapType
 */
function sendMap(
    key, value, context = undefined, mapType = MapTypes.OBJECT_MAP) {
  let map;
  if (mapType == MapTypes.OBJECT_MAP) {
    map = {};
    map[key] = value;
  } else if (mapType == MapTypes.STRUCTS_MAP) {
    map = new StructsMap();
    map.set(key, value);
  } else if (mapType == MapTypes.ES6_MAP) {
    map = new Map();
    map.set(key, value);
  } else {
    throw new Error('Unsupported map type :)');
  }

  channel.sendMap(map, context);
  mockClock.tick(0);
}

function hasForwardChannel() {
  return !!getSingleForwardRequest();
}

/** @suppress {visibility} suppression added to enable type checking */
function hasBackChannel() {
  return !!channel.backChannelRequest_;
}

/** @suppress {visibility} suppression added to enable type checking */
function hasDeadBackChannelTimer() {
  return channel.deadBackChannelTimerId_ != null;
}

function assertHasForwardChannel() {
  assertTrue('Forward channel missing.', hasForwardChannel());
}

function assertHasBackChannel() {
  assertTrue('Back channel missing.', hasBackChannel());
}

/**
 * @param {!MapTypes=} mapType
 */
function sendMapOnce(mapType = MapTypes.OBJECT_MAP) {
  assertEquals(1, numTimingEvents);
  sendMap('foo', 'bar', /* context= */ undefined, mapType);
  responseDone();
  assertEquals(2, numTimingEvents);
  assertEquals('foo:bar', formatArrayOfMaps(deliveredMaps));
}

function sendMapTwice() {
  sendMap('foo1', 'bar1');
  responseDone();
  assertEquals('foo1:bar1', formatArrayOfMaps(deliveredMaps));
  sendMap('foo2', 'bar2');
  responseDone();
  assertEquals('foo2:bar2', formatArrayOfMaps(deliveredMaps));
}

/** @suppress {visibility} suppression added to enable type checking */
function setFailFastWhileWaitingForRetry() {
  assertEquals(1, numTimingEvents);

  sendMap('foo', 'bar');
  assertNull(channel.forwardChannelTimerId_);
  assertNotNull(getSingleForwardRequest());
  assertEquals(0, channel.forwardChannelRetryCount_);

  // Watchdog timeout.
  responseTimeout();
  assertNotNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);

  // Almost finish the between-retry timeout.
  mockClock.tick(RETRY_TIME - 1);
  assertNotNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);

  // Setting max retries to 0 should cancel the timer and raise an error.
  channel.setFailFast(true);
  assertNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);

  // We get the error immediately before starting to ping google.com.
  assertTrue(gotError);
  assertEquals(0, deliveredMaps.length);

  // Simulate that timing out. We should not get another error.
  gotError = false;
  mockClock.tick(netUtils.NETWORK_TIMEOUT);
  assertFalse('Extra error after network ping timed out.', gotError);

  // Make sure no more retry timers are firing.
  mockClock.tick(ALL_DAY_MS);
  assertNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);
  assertEquals(1, numTimingEvents);
}

/** @suppress {visibility} suppression added to enable type checking */
function setFailFastWhileRetryXhrIsInFlight() {
  assertEquals(1, numTimingEvents);

  sendMap('foo', 'bar');
  assertNull(channel.forwardChannelTimerId_);
  assertNotNull(getSingleForwardRequest());
  assertEquals(0, channel.forwardChannelRetryCount_);

  // Watchdog timeout.
  responseTimeout();
  assertNotNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);

  // Wait for the between-retry timeout.
  mockClock.tick(RETRY_TIME);
  assertNull(channel.forwardChannelTimerId_);
  assertNotNull(getSingleForwardRequest());
  assertEquals(1, channel.forwardChannelRetryCount_);

  // Simulate a second watchdog timeout.
  responseTimeout();
  assertNotNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(2, channel.forwardChannelRetryCount_);

  // Wait for another between-retry timeout.
  mockClock.tick(RETRY_TIME);
  // Now the third req is in flight.
  assertNull(channel.forwardChannelTimerId_);
  assertNotNull(getSingleForwardRequest());
  assertEquals(2, channel.forwardChannelRetryCount_);

  // Set fail fast, killing the request
  channel.setFailFast(true);
  assertNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(2, channel.forwardChannelRetryCount_);

  // We get the error immediately before starting to ping google.com.
  assertTrue(gotError);

  // Simulate that timing out. We should not get another error.
  gotError = false;
  mockClock.tick(netUtils.NETWORK_TIMEOUT);
  assertFalse('Extra error after network ping timed out.', gotError);

  // Make sure no more retry timers are firing.
  mockClock.tick(ALL_DAY_MS);
  assertNull(channel.forwardChannelTimerId_);
  assertNull(getSingleForwardRequest());
  assertEquals(2, channel.forwardChannelRetryCount_);
  assertEquals(1, numTimingEvents);
}

function requestFailedClosesChannel() {
  assertEquals(1, numTimingEvents);

  sendMap('foo', 'bar');
  responseRequestFailed();

  assertEquals(
      'Should be closed immediately after request failed.',
      WebChannelBase.State.CLOSED, channel.getState());

  mockClock.tick(netUtils.NETWORK_TIMEOUT);

  assertEquals(
      'Should remain closed after the ping timeout.',
      WebChannelBase.State.CLOSED, channel.getState());
  assertEquals(1, numTimingEvents);
}

/** @suppress {visibility} suppression added to enable type checking */
function outgoingMapsAwaitsResponse() {
  assertEquals(0, channel.outgoingMaps_.length);

  sendMap('foo1', 'bar');
  assertEquals(0, channel.outgoingMaps_.length);
  sendMap('foo2', 'bar');
  assertEquals(1, channel.outgoingMaps_.length);
  sendMap('foo3', 'bar');
  assertEquals(2, channel.outgoingMaps_.length);
  sendMap('foo4', 'bar');
  assertEquals(3, channel.outgoingMaps_.length);

  responseDone();
  // Now the forward channel request is completed and a new started, so all maps
  // are dequeued from the array of outgoing maps into this new forward request.
  assertEquals(0, channel.outgoingMaps_.length);
}

testSuite({
  shouldRunTests() {
    return ChannelRequest.supportsXhrStreaming();
  },

  /**
   * @suppress {invalidCasts} The cast from MockChannelRequest to
   * ChannelRequest is invalid and will not compile.
   */
  setUpPage() {
    // Use our MockChannelRequests instead of the real ones.
    ChannelRequest.createChannelRequest =
        (channel, channelDebug, opt_sessionId, opt_requestId, opt_retryId) => {
          return /** @type {!ChannelRequest} */ (new MockChannelRequest(
              channel, channelDebug, opt_sessionId, opt_requestId,
              opt_retryId));
        };

    // Mock out the stat notification code.
    requestStats.notifyStatEvent = (stat) => {
      numStatEvents++;
      lastStatEvent = stat;
    };

    requestStats.notifyTimingEvent = (size, rtt, retries) => {
      numTimingEvents++;
      lastPostSize = size;
      lastPostRtt = rtt;
      lastPostRetryCount = retries;
    };
  },

  setUp() {
    numTimingEvents = 0;
    lastPostSize = null;
    lastPostRtt = null;
    lastPostRetryCount = null;

    mockClock = new MockClock(true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    channel = new WebChannelBase('1');

    gotError = false;

    handler = new WebChannelBase.Handler();
    handler.channelOpened = () => {};
    handler.channelError = (channel, error) => {
      gotError = true;
    };
    handler.channelSuccess = (channel, request) => {
      deliveredMaps = googArray.clone(request.getPendingMessages());
    };

    /**
     * @suppress {checkTypes} The callback function type declaration is
     * skipped.
     */
    handler.channelClosed = (channel, opt_pendingMaps, opt_undeliveredMaps) => {
      // Mock out the handler, and let it set a formatted user readable string
      // of the undelivered maps which we can use when verifying our assertions.
      if (opt_pendingMaps) {
        handler.pendingMapsString = formatArrayOfMaps(opt_pendingMaps);
      }
      if (opt_undeliveredMaps) {
        handler.undeliveredMapsString = formatArrayOfMaps(opt_undeliveredMaps);
      }
    };
    handler.channelHandleMultipleArrays = () => {};
    handler.channelHandleArray = () => {};

    channel.setHandler(handler);

    // Provide a predictable retry time for testing.
    /** @suppress {visibility} suppression added to enable type checking */
    channel.getRetryTime_ = (retryCount) => RETRY_TIME;

    const channelDebug = new WebChannelDebug();
    channelDebug.debug = (message) => {
      debugToWindow(message);
    };
    channel.setChannelDebug(channelDebug);

    numStatEvents = 0;
    lastStatEvent = null;
  },

  tearDown() {
    mockClock.dispose();
    stubs.reset();
    debugToWindow('<hr>');
  },

  testFormatArrayOfMaps() {
    // This function is used in a non-trivial test, so let's verify that it
    // works.
    const map1 = new Map();
    map1.set('k1', 'v1');
    map1.set('k2', 'v2');
    const map2 = new Map();
    map2.set('k3', 'v3');
    const map3 = new Map();
    map3.set('k4', 'v4');
    map3.set('k5', 'v5');
    map3.set('k6', 'v6');

    // One map.
    const a = [];
    a.push(new Wire.QueuedMap(0, map1));
    assertEquals('k1:v1, k2:v2', formatArrayOfMaps(a));

    // Many maps.
    const b = [];
    b.push(new Wire.QueuedMap(0, map1));
    b.push(new Wire.QueuedMap(0, map2));
    b.push(new Wire.QueuedMap(0, map3));
    assertEquals(
        'k1:v1, k2:v2, k3:v3, k4:v4, k5:v5, k6:v6', formatArrayOfMaps(b));

    // One map with a context.
    const c = [];
    c.push(new Wire.QueuedMap(0, map1, new String('c1')));
    assertEquals('k1:v1:c1, k2:v2:c1', formatArrayOfMaps(c));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect() {
    connect();
    assertEquals(WebChannelBase.State.OPENED, channel.getState());
    // If the server specifies no version, the client assumes the latest version
    assertEquals(Wire.LATEST_CHANNEL_VERSION, channel.channelVersion_);
    assertFalse(channel.isBuffered());
  },

  testConnect_backChannelEstablished() {
    connect();
    assertHasBackChannel();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withServerHostPrefix() {
    connect(undefined, 'serverHostPrefix');
    assertEquals('serverHostPrefix', channel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withClientHostPrefix() {
    handler.correctHostPrefix = (hostPrefix) => 'clientHostPrefix';
    connect();
    assertEquals('clientHostPrefix', channel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_overrideServerHostPrefix() {
    handler.correctHostPrefix = (hostPrefix) => 'clientHostPrefix';
    connect(undefined, 'serverHostPrefix');
    assertEquals('clientHostPrefix', channel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withServerVersion() {
    connect(8);
    assertEquals(8, channel.channelVersion_);
  },

  testConnect_notOkToMakeRequestForTest() {
    handler.okToMakeRequest = functions.constant(WebChannelBase.Error.NETWORK);
    channel.connect('/bind', null);
    mockClock.tick(0);
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());
  },

  testConnect_notOkToMakeRequestForBind() {
    channel.connect('/bind', null);
    mockClock.tick(0);
    handler.okToMakeRequest = functions.constant(WebChannelBase.Error.NETWORK);
    completeForwardChannel();
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());
  },

  testSendMap_withObjectMap() {
    connect();
    sendMapOnce(MapTypes.OBJECT_MAP);
  },

  testSendMap_withStructsMap() {
    connect();
    sendMapOnce(MapTypes.STRUCTS_MAP);
  },

  testSendMap_withEs6Map() {
    connect();
    sendMapOnce(MapTypes.ES6_MAP);
  },

  testSendMapWithSpdyEnabled() {
    connect(undefined, undefined, undefined, true);
    sendMapOnce();
  },

  testSendMap_twice() {
    connect();
    sendMapTwice();
  },

  testSendMap_twiceWithSpdyEnabled() {
    connect(undefined, undefined, undefined, true);
    sendMapTwice();
  },

  testSendMap_andReceive() {
    connect();
    sendMap('foo', 'bar');
    responseDone();
    receive('["the server reply"]');
  },

  testReceive() {
    connect();
    receive('["message from server"]');
    assertHasBackChannel();
  },

  testReceive_twice() {
    connect();
    receive('["message one from server"]');
    receive('["message two from server"]');
    assertHasBackChannel();
  },

  testReceive_andSendMap() {
    connect();
    receive('["the server reply"]');
    sendMap('foo', 'bar');
    responseDone();
    assertHasBackChannel();
  },

  testBackChannelRemainsEstablished_afterSingleSendMap() {
    connect();

    sendMap('foo', 'bar');
    responseDone();
    receive('["ack"]');

    assertHasBackChannel();
  },

  testBackChannelRemainsEstablished_afterDoubleSendMap() {
    connect();

    sendMap('foo1', 'bar1');
    sendMap('foo2', 'bar2');
    responseDone();
    receive('["ack"]');

    // This assertion would fail prior to CL 13302660.
    assertHasBackChannel();
  },

  testTimingEvent() {
    connect();
    assertEquals(1, numTimingEvents);
    sendMap('', '');
    assertEquals(1, numTimingEvents);
    mockClock.tick(20);
    let expSize = getSingleForwardRequest().getPostData().length;
    responseDone();

    assertEquals(2, numTimingEvents);
    assertEquals(expSize, lastPostSize);
    assertEquals(20, lastPostRtt);
    assertEquals(0, lastPostRetryCount);

    sendMap('abcdefg', '123456');
    expSize = getSingleForwardRequest().getPostData().length;
    responseTimeout();
    assertEquals(2, numTimingEvents);
    mockClock.tick(RETRY_TIME + 1);
    responseDone();
    assertEquals(3, numTimingEvents);
    assertEquals(expSize, lastPostSize);
    assertEquals(1, lastPostRetryCount);
    assertEquals(1, lastPostRtt);
  },

  /**
   * Make sure that dropping the forward channel retry limit below the retry
   * count reports an error, and prevents another request from firing.
   */
  testSetFailFastWhileWaitingForRetry() {
    stubNetUtils();

    connect();
    setFailFastWhileWaitingForRetry();
  },

  testSetFailFastWhileWaitingForRetryWithSpdyEnabled() {
    stubNetUtils();

    connect(undefined, undefined, undefined, true);
    setFailFastWhileWaitingForRetry();
  },

  /**
   * Make sure that dropping the forward channel retry limit below the retry
   * count reports an error, and prevents another request from firing.
   */
  testSetFailFastWhileRetryXhrIsInFlight() {
    stubNetUtils();

    connect();
    setFailFastWhileRetryXhrIsInFlight();
  },

  testSetFailFastWhileRetryXhrIsInFlightWithSpdyEnabled() {
    stubNetUtils();

    connect(undefined, undefined, undefined, true);
    setFailFastWhileRetryXhrIsInFlight();
  },

  /**
   * Makes sure that setting fail fast while not retrying doesn't cause a
   *      failure.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSetFailFastAtRetryCount() {
    stubNetUtils();

    connect();
    assertEquals(1, numTimingEvents);

    sendMap('foo', 'bar');
    assertNull(channel.forwardChannelTimerId_);
    assertNotNull(getSingleForwardRequest());
    assertEquals(0, channel.forwardChannelRetryCount_);

    // Set fail fast.
    channel.setFailFast(true);
    // Request should still be alive.
    assertNull(channel.forwardChannelTimerId_);
    assertNotNull(getSingleForwardRequest());
    assertEquals(0, channel.forwardChannelRetryCount_);

    // Watchdog timeout. Now we should get an error.
    responseTimeout();
    assertNull(channel.forwardChannelTimerId_);
    assertNull(getSingleForwardRequest());
    assertEquals(0, channel.forwardChannelRetryCount_);

    // We get the error immediately before starting to ping google.com.
    assertTrue(gotError);
    // We get the error immediately before starting to ping google.com.
    // Simulate that timing out. We should not get another error in addition
    // to the initial failure.
    gotError = false;
    mockClock.tick(netUtils.NETWORK_TIMEOUT);
    assertFalse('Extra error after network ping timed out.', gotError);

    // Make sure no more retry timers are firing.
    mockClock.tick(ALL_DAY_MS);
    assertNull(channel.forwardChannelTimerId_);
    assertNull(getSingleForwardRequest());
    assertEquals(0, channel.forwardChannelRetryCount_);
    assertEquals(1, numTimingEvents);
  },

  testRequestFailedClosesChannel() {
    stubNetUtils();

    connect();
    requestFailedClosesChannel();
  },

  testRequestFailedClosesChannelWithSpdyEnabled() {
    stubNetUtils();

    connect(undefined, undefined, undefined, true);
    requestFailedClosesChannel();
  },

  testStatEventReportedOnlyOnce() {
    stubNetUtils();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseUnknownSessionId();

    assertEquals(1, numStatEvents);
    assertEquals(Stat.ERROR_OTHER, lastStatEvent);

    numStatEvents = 0;
    mockClock.tick(netUtils.NETWORK_TIMEOUT);
    assertEquals('No new stat events should be reported.', 0, numStatEvents);
  },

  testStatEventReportedOnlyOnce_onNetworkUp() {
    stubNetUtils();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseRequestFailed();

    assertEquals(
        'No stat event should be reported before we know the reason.', 0,
        numStatEvents);

    // Let the ping time out.
    mockClock.tick(netUtils.NETWORK_TIMEOUT);

    // Assert we report the correct stat event.
    assertEquals(1, numStatEvents);
    assertEquals(Stat.ERROR_NETWORK, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testStatEventReportedOnlyOnce_onNetworkDown() {
    stubNetUtils();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseRequestFailed();

    assertEquals(
        'No stat event should be reported before we know the reason.', 0,
        numStatEvents);

    // Wait half the ping timeout period, and then fake the network being up.
    mockClock.tick(netUtils.NETWORK_TIMEOUT / 2);
    channel.testNetworkCallback_(true);

    // Assert we report the correct stat event.
    assertEquals(1, numStatEvents);
    assertEquals(Stat.ERROR_OTHER, lastStatEvent);
  },

  testOutgoingMapsAwaitsResponse() {
    connect();
    outgoingMapsAwaitsResponse();
  },

  testOutgoingMapsAwaitsResponseWithSpdyEnabled() {
    connect(undefined, undefined, undefined, true);
    outgoingMapsAwaitsResponse();
  },

  testUndeliveredMaps_doesNotNotifyWhenSuccessful() {
    /**
     * @suppress {checkTypes} The callback function type declaration is
     * skipped.
     */
    handler.channelClosed = (channel, opt_pendingMaps, opt_undeliveredMaps) => {
      if (opt_pendingMaps || opt_undeliveredMaps) {
        fail('No pending or undelivered maps should be reported.');
      }
    };

    connect();
    sendMap('foo1', 'bar1');
    responseDone();
    sendMap('foo2', 'bar2');
    responseDone();
    disconnect();
  },

  testUndeliveredMaps_doesNotNotifyIfNothingWasSent() {
    /**
     * @suppress {checkTypes} The callback function type declaration is
     * skipped.
     */
    handler.channelClosed = (channel, opt_pendingMaps, opt_undeliveredMaps) => {
      if (opt_pendingMaps || opt_undeliveredMaps) {
        fail('No pending or undelivered maps should be reported.');
      }
    };

    connect();
    mockClock.tick(ALL_DAY_MS);
    disconnect();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUndeliveredMaps_clearsPendingMapsAfterNotifying() {
    connect();
    sendMap('foo1', 'bar1');
    sendMap('foo2', 'bar2');
    sendMap('foo3', 'bar3');

    assertEquals(
        1, channel.forwardChannelRequestPool_.getPendingMessages().length);
    assertEquals(2, channel.outgoingMaps_.length);

    disconnect();

    assertEquals(
        0, channel.forwardChannelRequestPool_.getPendingMessages().length);
    assertEquals(0, channel.outgoingMaps_.length);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testUndeliveredMaps_notifiesWithContext() {
    connect();

    // First send two messages that succeed.
    sendMap('foo1', 'bar1', 'context1');
    responseDone();
    sendMap('foo2', 'bar2', 'context2');
    responseDone();

    // Pretend the server hangs and no longer responds.
    sendMap('foo3', 'bar3', 'context3');
    sendMap('foo4', 'bar4', 'context4');
    sendMap('foo5', 'bar5', 'context5');

    // Give up.
    disconnect();

    // Assert that we are informed of any undelivered messages; both about
    // #3 that was sent but which we don't know if the server received, and
    // #4 and #5 which remain in the outgoing maps and have not yet been sent.
    assertEquals('foo3:bar3:context3', handler.pendingMapsString);
    assertEquals(
        'foo4:bar4:context4, foo5:bar5:context5',
        handler.undeliveredMapsString);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testUndeliveredMaps_serviceUnavailable() {
    // Send a few maps, and let one fail.
    connect();
    sendMap('foo1', 'bar1');
    responseDone();
    sendMap('foo2', 'bar2');
    responseRequestFailed();

    // After a failure, the channel should be closed.
    disconnect();

    assertEquals('foo2:bar2', handler.pendingMapsString);
    assertEquals('', handler.undeliveredMapsString);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testUndeliveredMaps_onPingTimeout() {
    stubNetUtils();

    connect();

    // Send a message.
    sendMap('foo1', 'bar1');

    // Fake REQUEST_FAILED, triggering a ping to check the network.
    responseRequestFailed();

    // Let the ping time out, unsuccessfully.
    mockClock.tick(netUtils.NETWORK_TIMEOUT);

    // Assert channel is closed.
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());

    // Assert that the handler is notified about the undelivered messages.
    assertEquals('foo1:bar1', handler.pendingMapsString);
    assertEquals('', handler.undeliveredMapsString);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseNoBackchannelPostNotBeforeBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');

    mockClock.tick(10);
    assertFalse(
        channel.backChannelRequest_.getRequestStartTime() <
        getSingleForwardRequest().getRequestStartTime());
    responseNoBackchannel();
    assertNotEquals(Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseNoBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    response(-1, 0);
    mockClock.tick(WebChannelBase.RTT_ESTIMATE + 1);
    sendMap('foo2', 'bar2');
    assertTrue(
        channel.backChannelRequest_.getRequestStartTime() +
            WebChannelBase.RTT_ESTIMATE <
        getSingleForwardRequest().getRequestStartTime());
    responseNoBackchannel();
    assertEquals(Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseNoBackchannelWithNoBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertNull(channel.backChannelTimerId_);
    channel.backChannelRequest_.cancel();
    /** @suppress {visibility} suppression added to enable type checking */
    channel.backChannelRequest_ = null;
    responseNoBackchannel();
    assertEquals(Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseNoBackchannelWithStartTimer() {
    connect(8);
    sendMap('foo1', 'bar1');

    channel.backChannelRequest_.cancel();
    /** @suppress {visibility} suppression added to enable type checking */
    channel.backChannelRequest_ = null;
    /** @suppress {visibility} suppression added to enable type checking */
    channel.backChannelTimerId_ = 123;
    responseNoBackchannel();
    assertNotEquals(Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithNoArraySent() {
    connect(8);
    sendMap('foo1', 'bar1');

    // Send a response as if the server hasn't sent down an array.
    response(-1, 0);

    // POST response with an array ID lower than our last received is OK.
    assertEquals(1, channel.lastArrayId_);
    assertEquals(-1, channel.lastPostResponseArrayId_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithArraysMissing() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    mockClock.tick(WebChannelBase.RTT_ESTIMATE * 2);
    assertEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMultipleResponsesWithArraysMissing() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    sendMap('foo2', 'bar2');
    mockClock.tick(WebChannelBase.RTT_ESTIMATE);
    response(8, 119);
    mockClock.tick(WebChannelBase.RTT_ESTIMATE);
    // The original timer should still fire.
    assertEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOnlyRetryOnceBasedOnResponse() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    assertTrue(hasDeadBackChannelTimer());
    mockClock.tick(WebChannelBase.RTT_ESTIMATE * 2);
    assertEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
    assertEquals(1, channel.backChannelRetryCount_);
    mockClock.tick(WebChannelBase.RTT_ESTIMATE);
    sendMap('foo2', 'bar2');
    assertFalse(hasDeadBackChannelTimer());
    response(8, 119);
    assertFalse(hasDeadBackChannelTimer());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithArraysMissingAndLiveChannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    mockClock.tick(WebChannelBase.RTT_ESTIMATE);
    assertTrue(hasDeadBackChannelTimer());
    receive('["ack"]');
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(WebChannelBase.RTT_ESTIMATE);
    assertNotEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithBigOutstandingData() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays and 50kbytes.
    response(7, 50000);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(WebChannelBase.RTT_ESTIMATE * 2);
    assertNotEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseInBufferedMode() {
    connect(8);
    /** @suppress {visibility} suppression added to enable type checking */
    channel.enableStreaming_ = false;
    sendMap('foo1', 'bar1');
    assertEquals(-1, channel.lastPostResponseArrayId_);
    response(7, 111);

    assertEquals(1, channel.lastArrayId_);
    assertEquals(7, channel.lastPostResponseArrayId_);
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(WebChannelBase.RTT_ESTIMATE * 2);
    assertNotEquals(Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  testResponseWithGarbage() {
    connect(8);
    sendMap('foo1', 'bar1');
    channel.onRequestData(getSingleForwardRequest(), 'garbage');
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());
  },

  testResponseWithGarbageInArray() {
    connect(8);
    sendMap('foo1', 'bar1');
    channel.onRequestData(getSingleForwardRequest(), '["garbage"]');
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());
  },

  testResponseWithEvilData() {
    connect(8);
    sendMap('foo1', 'bar1');
    channel.onRequestData(
        getSingleForwardRequest(),
        'foo=<script>evil()<\/script>&' +
            'bar=<script>moreEvil()<\/script>');
    assertEquals(WebChannelBase.State.CLOSED, channel.getState());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathAbsolute() {
    connect(8, undefined, '/talkgadget');
    assertEquals(channel.backChannelUri_.getDomain(), window.location.hostname);
    assertEquals(
        channel.forwardChannelUri_.getDomain(), window.location.hostname);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathRelative() {
    connect(8, undefined, 'talkgadget');
    assertEquals(channel.backChannelUri_.getDomain(), window.location.hostname);
    assertEquals(
        channel.forwardChannelUri_.getDomain(), window.location.hostname);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathWithHost() {
    connect(8, undefined, 'https://example.com');
    assertEquals(channel.backChannelUri_.getScheme(), 'https');
    assertEquals(channel.backChannelUri_.getDomain(), 'example.com');
    assertEquals(channel.forwardChannelUri_.getScheme(), 'https');
    assertEquals(channel.forwardChannelUri_.getDomain(), 'example.com');
  },

  testCreateXhrIo() {
    let xhr = channel.createXhrIo(null);
    assertFalse(xhr.getWithCredentials());

    assertThrows(
        'Error connection to different host without CORS',
        goog.bind(channel.createXhrIo, channel, 'some_host'));

    channel.setSupportsCrossDomainXhrs(true);

    xhr = channel.createXhrIo(null);
    assertTrue(xhr.getWithCredentials());

    xhr = channel.createXhrIo('some_host');
    assertTrue(xhr.getWithCredentials());
  },

  testSpdyLimitOption() {
    const webChannelTransport = new WebChannelBaseTransport();
    stubSpdyCheck(true);
    const webChannelDefault = webChannelTransport.createWebChannel('/foo');
    assertEquals(
        10,
        webChannelDefault.getRuntimeProperties().getConcurrentRequestLimit());
    assertTrue(webChannelDefault.getRuntimeProperties().isSpdyEnabled());

    const options = {'concurrentRequestLimit': 100};

    stubSpdyCheck(false);
    const webChannelDisabled =
        webChannelTransport.createWebChannel('/foo', options);
    assertEquals(
        1,
        webChannelDisabled.getRuntimeProperties().getConcurrentRequestLimit());
    assertFalse(webChannelDisabled.getRuntimeProperties().isSpdyEnabled());

    stubSpdyCheck(true);
    const webChannelEnabled =
        webChannelTransport.createWebChannel('/foo', options);
    assertEquals(
        100,
        webChannelEnabled.getRuntimeProperties().getConcurrentRequestLimit());
    assertTrue(webChannelEnabled.getRuntimeProperties().isSpdyEnabled());
  },
});
