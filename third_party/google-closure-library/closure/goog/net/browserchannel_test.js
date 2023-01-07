/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.BrowserChannelTest');
goog.setTestOnly();

const BrowserChannel = goog.require('goog.net.BrowserChannel');
const ChannelDebug = goog.require('goog.net.ChannelDebug');
const ChannelRequest = goog.require('goog.net.ChannelRequest');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const StructsMap = goog.require('goog.structs.Map');
const Timer = goog.require('goog.Timer');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const googJson = goog.require('goog.json');
const recordFunction = goog.require('goog.testing.recordFunction');
const stats = goog.require('goog.net.browserchannelinternal.stats');
const testSuite = goog.require('goog.testing.testSuite');
const tmpnetwork = goog.require('goog.net.tmpnetwork');

/** Delay between a network failure and the next network request. */
const RETRY_TIME = 1000;

/** A really long time - used to make sure no more timeouts will fire. */
const ALL_DAY_MS = 1000 * 60 * 60 * 24;

const stubs = new PropertyReplacer();

let browserChannel;
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
 * Stubs tmpnetwork to always time out. It maintains the
 * contract given by tmpnetwork.testGoogleCom, but always
 * times out (calling callback(false)).
 * stubTmpnetwork should be called in tests that require it before
 * a call to testGoogleCom happens. It is reset at tearDown.
 */
function stubTmpnetwork() {
  stubs.set(tmpnetwork, 'testLoadImage', (url, timeout, callback) => {
    Timer.callOnce(goog.partial(callback, false), timeout);
  });
}

/** Mock ChannelRequest. */
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

    this.postData_ = null;
    this.requestStartTime_ = null;
    this.requestUri_ = null;
  }

  setExtraHeaders(extraHeaders) {}

  setTimeout(timeout) {}

  setReadyStateChangeThrottle(throttle) {}

  xmlHttpPost(uri, postData, decodeChunks) {
    this.channelDebug_.debug(`---> POST: ${uri}, ${postData}, ${decodeChunks}`);
    this.postData_ = postData;
    this.requestStartTime_ = Date.now();
  }

  xmlHttpGet(uri, decodeChunks, noClose = undefined) {
    this.channelDebug_.debug(`<--- GET: ${uri}, ${decodeChunks}, ${noClose}`);
    this.requestUri_ = uri;
    this.requestStartTime_ = Date.now();
  }

  tridentGet(uri, usingSecondaryDomain) {
    this.channelDebug_.debug('<---GET (T): ' + uri);
    this.requestUri_ = uri;
    this.requestStartTime_ = Date.now();
  }

  sendUsingImgTag(uri) {
    this.requestStartTime_ = Date.now();
  }

  cancel() {
    this.successful_ = false;
  }

  getSuccess() {
    return this.successful_;
  }

  getLastError() {
    return this.lastError_;
  }

  getLastStatusCode() {
    return this.lastStatusCode_;
  }

  getSessionId() {
    return this.sessionId_;
  }

  getRequestId() {
    return this.requestId_;
  }

  getPostData() {
    return this.postData_;
  }

  getRequestStartTime() {
    return this.requestStartTime_;
  }
}

/**
 * Helper function to return a formatted string representing an array of maps.
 */
function formatArrayOfMaps(arrayOfMaps) {
  const result = [];
  for (let i = 0; i < arrayOfMaps.length; i++) {
    const map = arrayOfMaps[i];
    const keys = map.map.getKeys();
    for (let j = 0; j < keys.length; j++) {
      const tmp = keys[j] + ':' + map.map.get(keys[j]) +
          (map.context ? ':' + map.context : '');
      result.push(tmp);
    }
  }
  return result.join(', ');
}

function connectForwardChannel(
    serverVersion = undefined, hostPrefix = undefined, opt_uriPrefix) {
  const uriPrefix = opt_uriPrefix || '';
  browserChannel.connect(`${uriPrefix}/test`, `${uriPrefix}/bind`, null);
  mockClock.tick(0);
  completeTestConnection();
  completeForwardChannel(serverVersion, hostPrefix);
}

function connect(
    serverVersion = undefined, hostPrefix = undefined, uriPrefix = undefined) {
  connectForwardChannel(serverVersion, hostPrefix, uriPrefix);
  completeBackChannel();
}

function disconnect() {
  browserChannel.disconnect();
  mockClock.tick(0);
}

function completeTestConnection() {
  completeForwardTestConnection();
  completeBackTestConnection();
  assertEquals(BrowserChannel.State.OPENING, browserChannel.getState());
}

/** @suppress {visibility} suppression added to enable type checking */
function completeForwardTestConnection() {
  browserChannel.connectionTest_.onRequestData(
      browserChannel.connectionTest_, '["b"]');
  browserChannel.connectionTest_.onRequestComplete(
      browserChannel.connectionTest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function completeBackTestConnection() {
  browserChannel.connectionTest_.onRequestData(
      browserChannel.connectionTest_, '11111');
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function completeForwardChannel(
    serverVersion = undefined, hostPrefix = undefined) {
  const responseData = '[[0,["c","1234567890ABCDEF",' +
      (hostPrefix ? `"${hostPrefix}"` : 'null') +
      (serverVersion ? `,${serverVersion}` : '') + ']]]';
  browserChannel.onRequestData(
      browserChannel.forwardChannelRequest_, responseData);
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function completeBackChannel() {
  browserChannel.onRequestData(
      browserChannel.backChannelRequest_, '[[1,["foo"]]]');
  browserChannel.onRequestComplete(browserChannel.backChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseVersion7() {
  browserChannel.onRequestData(
      browserChannel.forwardChannelRequest_,
      ChannelDebug.MAGIC_RESPONSE_COOKIE);
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseNoBackchannel(lastArrayIdSentFromServer, outstandingDataSize) {
  const responseData =
      googJson.serialize([0, lastArrayIdSentFromServer, outstandingDataSize]);
  browserChannel.onRequestData(
      browserChannel.forwardChannelRequest_, responseData);
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function response(lastArrayIdSentFromServer, outstandingDataSize) {
  const responseData =
      googJson.serialize([1, lastArrayIdSentFromServer, outstandingDataSize]);
  browserChannel.onRequestData(
      browserChannel.forwardChannelRequest_, responseData);
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function receive(data) {
  browserChannel.onRequestData(
      browserChannel.backChannelRequest_, `[[1,${data}]]`);
  browserChannel.onRequestComplete(browserChannel.backChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseTimeout() {
  Object.assign(
      browserChannel.forwardChannelRequest_,
      {lastError_: ChannelRequest.Error.TIMEOUT, successful_: false});
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseRequestFailed(statusCode = undefined) {
  Object.assign(browserChannel.forwardChannelRequest_, {
    lastError_: ChannelRequest.Error.STATUS,
    lastStatusCode_: statusCode || 503,
    successful_: false,
  });
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseUnknownSessionId() {
  Object.assign(browserChannel.forwardChannelRequest_, {
    lastError_: ChannelRequest.Error.UNKNOWN_SESSION_ID,
    successful_: false,
  });
  browserChannel.onRequestComplete(browserChannel.forwardChannelRequest_);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function responseActiveXBlocked() {
  Object.assign(
      browserChannel.backChannelRequest_,
      {lastError_: ChannelRequest.Error.ACTIVE_X_BLOCKED, successful_: false});
  browserChannel.onRequestComplete(browserChannel.backChannelRequest_);
  mockClock.tick(0);
}

function sendMap(key, value, context = undefined) {
  const map = new StructsMap();
  map.set(key, value);
  browserChannel.sendMap(map, context);
  mockClock.tick(0);
}

/** @suppress {visibility} suppression added to enable type checking */
function hasForwardChannel() {
  return !!browserChannel.forwardChannelRequest_;
}

/** @suppress {visibility} suppression added to enable type checking */
function hasBackChannel() {
  return !!browserChannel.backChannelRequest_;
}

/** @suppress {visibility} suppression added to enable type checking */
function hasDeadBackChannelTimer() {
  return browserChannel.deadBackChannelTimerId_ != null;
}

function assertHasForwardChannel() {
  assertTrue('Forward channel missing.', hasForwardChannel());
}

function assertHasBackChannel() {
  assertTrue('Back channel missing.', hasBackChannel());
}

testSuite({
  setUpPage() {
    // Use our MockChannelRequests instead of the real ones.
    /** @suppress {checkTypes} suppression added to enable type checking */
    ChannelRequest.createChannelRequest =
        (channel, channelDebug, opt_sessionId, opt_requestId, opt_retryId) =>
            new MockChannelRequest(
                channel, channelDebug, opt_sessionId, opt_requestId,
                opt_retryId);

    // Mock out the stat notification code.
    stats.notifyStatEvent = (stat) => {
      numStatEvents++;
      lastStatEvent = stat;
    };

    BrowserChannel.notifyTimingEvent = (size, rtt, retries) => {
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
    browserChannel = new BrowserChannel('1');
    gotError = false;

    handler = new BrowserChannel.Handler();
    handler.channelOpened = () => {};
    handler.channelError = (channel, error) => {
      gotError = true;
    };
    handler.channelSuccess = (channel, maps) => {
      deliveredMaps = googArray.clone(maps);
    };
    handler.channelClosed = function(
        channel, opt_pendingMaps, opt_undeliveredMaps) {
      // Mock out the handler, and let it set a formatted user readable string
      // of the undelivered maps which we can use when verifying our assertions.
      if (opt_pendingMaps) {
        this.pendingMapsString = formatArrayOfMaps(opt_pendingMaps);
      }
      if (opt_undeliveredMaps) {
        this.undeliveredMapsString = formatArrayOfMaps(opt_undeliveredMaps);
      }
    };
    handler.channelHandleMultipleArrays = () => {};
    handler.channelHandleArray = () => {};

    browserChannel.setHandler(handler);

    // Provide a predictable retry time for testing.
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.getRetryTime_ = (retryCount) => RETRY_TIME;

    const channelDebug = new ChannelDebug();
    channelDebug.debug = (message) => {
      debugToWindow(message);
    };
    browserChannel.setChannelDebug(channelDebug);

    numStatEvents = 0;
    lastStatEvent = null;
  },

  tearDown() {
    mockClock.dispose();
    stubs.reset();
    debugToWindow('<hr>');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFormatArrayOfMaps() {
    // This function is used in a non-trivial test, so let's verify that it
    // works.
    const map1 = new StructsMap();
    map1.set('k1', 'v1');
    map1.set('k2', 'v2');
    const map2 = new StructsMap();
    map2.set('k3', 'v3');
    const map3 = new StructsMap();
    map3.set('k4', 'v4');
    map3.set('k5', 'v5');
    map3.set('k6', 'v6');

    // One map.
    const a = [];
    a.push(new BrowserChannel.QueuedMap(0, map1));
    assertEquals('k1:v1, k2:v2', formatArrayOfMaps(a));

    // Many maps.
    const b = [];
    b.push(new BrowserChannel.QueuedMap(0, map1));
    b.push(new BrowserChannel.QueuedMap(0, map2));
    b.push(new BrowserChannel.QueuedMap(0, map3));
    assertEquals(
        'k1:v1, k2:v2, k3:v3, k4:v4, k5:v5, k6:v6', formatArrayOfMaps(b));

    // One map with a context.
    const c = [];
    c.push(new BrowserChannel.QueuedMap(0, map1, 'c1'));
    assertEquals('k1:v1:c1, k2:v2:c1', formatArrayOfMaps(c));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect() {
    connect();
    assertEquals(BrowserChannel.State.OPENED, browserChannel.getState());
    // If the server specifies no version, the client assumes 6
    assertEquals(6, browserChannel.channelVersion_);
    assertFalse(browserChannel.isBuffered());
  },

  testConnect_backChannelEstablished() {
    connect();
    assertHasBackChannel();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withServerHostPrefix() {
    connect(undefined, 'serverHostPrefix');
    assertEquals('serverHostPrefix', browserChannel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withClientHostPrefix() {
    handler.correctHostPrefix = (hostPrefix) => 'clientHostPrefix';
    connect();
    assertEquals('clientHostPrefix', browserChannel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_overrideServerHostPrefix() {
    handler.correctHostPrefix = (hostPrefix) => 'clientHostPrefix';
    connect(undefined, 'serverHostPrefix');
    assertEquals('clientHostPrefix', browserChannel.hostPrefix_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConnect_withServerVersion() {
    connect(8);
    assertEquals(8, browserChannel.channelVersion_);
  },

  testConnect_notOkToMakeRequestForTest() {
    handler.okToMakeRequest = functions.constant(BrowserChannel.Error.NETWORK);
    browserChannel.connect('/test', '/bind', null);
    mockClock.tick(0);
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());
  },

  testConnect_notOkToMakeRequestForBind() {
    browserChannel.connect('/test', '/bind', null);
    mockClock.tick(0);
    completeTestConnection();
    handler.okToMakeRequest = functions.constant(BrowserChannel.Error.NETWORK);
    completeForwardChannel();
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());
  },

  testSendMap() {
    connect();
    assertEquals(1, numTimingEvents);
    sendMap('foo', 'bar');
    responseVersion7();
    assertEquals(2, numTimingEvents);
    assertEquals('foo:bar', formatArrayOfMaps(deliveredMaps));
  },

  testSendMap_twice() {
    connect();
    sendMap('foo1', 'bar1');
    responseVersion7();
    assertEquals('foo1:bar1', formatArrayOfMaps(deliveredMaps));
    sendMap('foo2', 'bar2');
    responseVersion7();
    assertEquals('foo2:bar2', formatArrayOfMaps(deliveredMaps));
  },

  testSendMap_andReceive() {
    connect();
    sendMap('foo', 'bar');
    responseVersion7();
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
    responseVersion7();
    assertHasBackChannel();
  },

  testBackChannelRemainsEstablished_afterSingleSendMap() {
    connect();

    sendMap('foo', 'bar');
    responseVersion7();
    receive('["ack"]');

    assertHasBackChannel();
  },

  testBackChannelRemainsEstablished_afterDoubleSendMap() {
    connect();

    sendMap('foo1', 'bar1');
    sendMap('foo2', 'bar2');
    responseVersion7();
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
    /** @suppress {visibility} suppression added to enable type checking */
    let expSize = browserChannel.forwardChannelRequest_.getPostData().length;
    responseVersion7();

    assertEquals(2, numTimingEvents);
    assertEquals(expSize, lastPostSize);
    assertEquals(20, lastPostRtt);
    assertEquals(0, lastPostRetryCount);

    sendMap('abcdefg', '123456');
    /** @suppress {visibility} suppression added to enable type checking */
    expSize = browserChannel.forwardChannelRequest_.getPostData().length;
    responseTimeout();
    assertEquals(2, numTimingEvents);
    mockClock.tick(RETRY_TIME + 1);
    responseVersion7();
    assertEquals(3, numTimingEvents);
    assertEquals(expSize, lastPostSize);
    assertEquals(1, lastPostRetryCount);
    assertEquals(1, lastPostRtt);
  },

  /**
   * Make sure that dropping the forward channel retry limit below the retry
   * count reports an error, and prevents another request from firing.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSetFailFastWhileWaitingForRetry() {
    stubTmpnetwork();

    connect();
    assertEquals(1, numTimingEvents);

    sendMap('foo', 'bar');
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);

    // Watchdog timeout.
    responseTimeout();
    assertNotNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);

    // Almost finish the between-retry timeout.
    mockClock.tick(RETRY_TIME - 1);
    assertNotNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);

    // Setting max retries to 0 should cancel the timer and raise an error.
    browserChannel.setFailFast(true);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);

    assertTrue(gotError);
    assertEquals(0, deliveredMaps.length);
    // We get the error immediately before starting to ping google.com.
    // Simulate that timing out. We should get a network error in addition to
    // the initial failure.
    gotError = false;
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);
    assertTrue('No error after tmpnetwork ping timed out.', gotError);

    // Make sure no more retry timers are firing.
    mockClock.tick(ALL_DAY_MS);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);
    assertEquals(1, numTimingEvents);
  },

  /**
   * Make sure that dropping the forward channel retry limit below the retry
   * count reports an error, and prevents another request from firing.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSetFailFastWhileRetryXhrIsInFlight() {
    stubTmpnetwork();

    connect();
    assertEquals(1, numTimingEvents);

    sendMap('foo', 'bar');
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);

    // Watchdog timeout.
    responseTimeout();
    assertNotNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);

    // Wait for the between-retry timeout.
    mockClock.tick(RETRY_TIME);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(1, browserChannel.forwardChannelRetryCount_);

    // Simulate a second watchdog timeout.
    responseTimeout();
    assertNotNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(2, browserChannel.forwardChannelRetryCount_);

    // Wait for another between-retry timeout.
    mockClock.tick(RETRY_TIME);
    // Now the third req is in flight.
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(2, browserChannel.forwardChannelRetryCount_);

    // Set fail fast, killing the request
    browserChannel.setFailFast(true);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(2, browserChannel.forwardChannelRetryCount_);

    assertTrue(gotError);
    // We get the error immediately before starting to ping google.com.
    // Simulate that timing out. We should get a network error in addition to
    // the
    gotError = false;
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);
    assertTrue('No error after tmpnetwork ping timed out.', gotError);

    // Make sure no more retry timers are firing.
    mockClock.tick(ALL_DAY_MS);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(2, browserChannel.forwardChannelRetryCount_);
    assertEquals(1, numTimingEvents);
  },

  /**
   * Makes sure that setting fail fast while not retrying doesn't cause a
   *      failure.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSetFailFastAtRetryCount() {
    stubTmpnetwork();

    connect();
    assertEquals(1, numTimingEvents);

    sendMap('foo', 'bar');
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);

    // Set fail fast.
    browserChannel.setFailFast(true);
    // Request should still be alive.
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);

    // Watchdog timeout. Now we should get an error.
    responseTimeout();
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);

    assertTrue(gotError);
    // We get the error immediately before starting to ping google.com.
    // Simulate that timing out. We should get a network error in addition to
    // the initial failure.
    gotError = false;
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);
    assertTrue('No error after tmpnetwork ping timed out.', gotError);

    // Make sure no more retry timers are firing.
    mockClock.tick(ALL_DAY_MS);
    assertNull(browserChannel.forwardChannelTimerId_);
    assertNull(browserChannel.forwardChannelRequest_);
    assertEquals(0, browserChannel.forwardChannelRetryCount_);
    assertEquals(1, numTimingEvents);
  },

  testRequestFailedClosesChannel() {
    stubTmpnetwork();

    connect();
    assertEquals(1, numTimingEvents);

    sendMap('foo', 'bar');
    responseRequestFailed();

    assertEquals(
        'Should be closed immediately after request failed.',
        BrowserChannel.State.CLOSED, browserChannel.getState());

    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);

    assertEquals(
        'Should remain closed after the ping timeout.',
        BrowserChannel.State.CLOSED, browserChannel.getState());
    assertEquals(1, numTimingEvents);
  },

  testStatEventReportedOnlyOnce() {
    stubTmpnetwork();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseUnknownSessionId();

    assertEquals(1, numStatEvents);
    assertEquals(BrowserChannel.Stat.ERROR_OTHER, lastStatEvent);

    numStatEvents = 0;
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);
    assertEquals('No new stat events should be reported.', 0, numStatEvents);
  },

  testActiveXBlockedEventReportedOnlyOnce() {
    stubTmpnetwork();

    connectForwardChannel();
    numStatEvents = 0;
    lastStatEvent = null;
    responseActiveXBlocked();

    assertEquals(1, numStatEvents);
    assertEquals(BrowserChannel.Stat.ERROR_OTHER, lastStatEvent);

    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);
    assertEquals('No new stat events should be reported.', 1, numStatEvents);
  },

  testStatEventReportedOnlyOnce_onNetworkUp() {
    stubTmpnetwork();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseRequestFailed();

    assertEquals(
        'No stat event should be reported before we know the reason.', 0,
        numStatEvents);

    // Let the ping time out.
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);

    // Assert we report the correct stat event.
    assertEquals(1, numStatEvents);
    assertEquals(BrowserChannel.Stat.ERROR_NETWORK, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testStatEventReportedOnlyOnce_onNetworkDown() {
    stubTmpnetwork();

    connect();
    sendMap('foo', 'bar');
    numStatEvents = 0;
    lastStatEvent = null;
    responseRequestFailed();

    assertEquals(
        'No stat event should be reported before we know the reason.', 0,
        numStatEvents);

    // Wait half the ping timeout period, and then fake the network being up.
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT / 2);
    browserChannel.testGoogleComCallback_(true);

    // Assert we report the correct stat event.
    assertEquals(1, numStatEvents);
    assertEquals(BrowserChannel.Stat.ERROR_OTHER, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOutgoingMapsAwaitsResponse() {
    connect();
    assertEquals(0, browserChannel.outgoingMaps_.length);

    sendMap('foo1', 'bar');
    assertEquals(0, browserChannel.outgoingMaps_.length);
    sendMap('foo2', 'bar');
    assertEquals(1, browserChannel.outgoingMaps_.length);
    sendMap('foo3', 'bar');
    assertEquals(2, browserChannel.outgoingMaps_.length);
    sendMap('foo4', 'bar');
    assertEquals(3, browserChannel.outgoingMaps_.length);

    responseVersion7();
    // Now the forward channel request is completed and a new started, so all
    // maps are dequeued from the array of outgoing maps into this new forward
    // request.
    assertEquals(0, browserChannel.outgoingMaps_.length);
  },

  testUndeliveredMaps_doesNotNotifyWhenSuccessful() {
    handler.channelClosed = (channel, opt_pendingMaps, opt_undeliveredMaps) => {
      if (opt_pendingMaps || opt_undeliveredMaps) {
        fail('No pending or undelivered maps should be reported.');
      }
    };

    connect();
    sendMap('foo1', 'bar1');
    responseVersion7();
    sendMap('foo2', 'bar2');
    responseVersion7();
    disconnect();
  },

  testUndeliveredMaps_doesNotNotifyIfNothingWasSent() {
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

    assertEquals(1, browserChannel.pendingMaps_.length);
    assertEquals(2, browserChannel.outgoingMaps_.length);

    disconnect();

    assertEquals(0, browserChannel.pendingMaps_.length);
    assertEquals(0, browserChannel.outgoingMaps_.length);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testUndeliveredMaps_notifiesWithContext() {
    connect();

    // First send two messages that succeed.
    sendMap('foo1', 'bar1', 'context1');
    responseVersion7();
    sendMap('foo2', 'bar2', 'context2');
    responseVersion7();

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

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testUndeliveredMaps_serviceUnavailable() {
    // Send a few maps, and let one fail.
    connect();
    sendMap('foo1', 'bar1');
    responseVersion7();
    sendMap('foo2', 'bar2');
    responseRequestFailed();

    // After a failure, the channel should be closed.
    disconnect();

    assertEquals('foo2:bar2', handler.pendingMapsString);
    assertEquals('', handler.undeliveredMapsString);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testUndeliveredMaps_onPingTimeout() {
    stubTmpnetwork();

    connect();

    // Send a message.
    sendMap('foo1', 'bar1');

    // Fake REQUEST_FAILED, triggering a ping to check the network.
    responseRequestFailed();

    // Let the ping time out, unsuccessfully.
    mockClock.tick(tmpnetwork.GOOGLECOM_TIMEOUT);

    // Assert channel is closed.
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());

    // Assert that the handler is notified about the undelivered messages.
    assertEquals('foo1:bar1', handler.pendingMapsString);
    assertEquals('', handler.undeliveredMapsString);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testResponseNoBackchannelPostNotBeforeBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');

    mockClock.tick(10);
    assertFalse(
        browserChannel.backChannelRequest_.getRequestStartTime() <
        browserChannel.forwardChannelRequest_.getRequestStartTime());
    responseNoBackchannel();
    assertNotEquals(BrowserChannel.Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testResponseNoBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    response(-1, 0);
    mockClock.tick(BrowserChannel.RTT_ESTIMATE + 1);
    sendMap('foo2', 'bar2');
    assertTrue(
        browserChannel.backChannelRequest_.getRequestStartTime() +
            BrowserChannel.RTT_ESTIMATE <
        browserChannel.forwardChannelRequest_.getRequestStartTime());
    responseNoBackchannel();
    assertEquals(BrowserChannel.Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testResponseNoBackchannelWithNoBackchannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertNull(browserChannel.backChannelTimerId_);
    browserChannel.backChannelRequest_.cancel();
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.backChannelRequest_ = null;
    responseNoBackchannel();
    assertEquals(BrowserChannel.Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testResponseNoBackchannelWithStartTimer() {
    connect(8);
    sendMap('foo1', 'bar1');

    browserChannel.backChannelRequest_.cancel();
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.backChannelRequest_ = null;
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.backChannelTimerId_ = 123;
    responseNoBackchannel();
    assertNotEquals(BrowserChannel.Stat.BACKCHANNEL_MISSING, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithNoArraySent() {
    connect(8);
    sendMap('foo1', 'bar1');

    // Send a response as if the server hasn't sent down an array.
    response(-1, 0);

    // POST response with an array ID lower than our last received is OK.
    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithArraysMissing() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    mockClock.tick(BrowserChannel.RTT_ESTIMATE * 2);
    assertEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMultipleResponsesWithArraysMissing() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    sendMap('foo2', 'bar2');
    mockClock.tick(BrowserChannel.RTT_ESTIMATE);
    response(8, 119);
    mockClock.tick(BrowserChannel.RTT_ESTIMATE);
    // The original timer should still fire.
    assertEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOnlyRetryOnceBasedOnResponse() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    assertTrue(hasDeadBackChannelTimer());
    mockClock.tick(BrowserChannel.RTT_ESTIMATE * 2);
    assertEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
    assertEquals(1, browserChannel.backChannelRetryCount_);
    mockClock.tick(BrowserChannel.RTT_ESTIMATE);
    sendMap('foo2', 'bar2');
    assertFalse(hasDeadBackChannelTimer());
    response(8, 119);
    assertFalse(hasDeadBackChannelTimer());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithArraysMissingAndLiveChannel() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays.
    response(7, 111);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    mockClock.tick(BrowserChannel.RTT_ESTIMATE);
    assertTrue(hasDeadBackChannelTimer());
    receive('["ack"]');
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(BrowserChannel.RTT_ESTIMATE);
    assertNotEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithBigOutstandingData() {
    connect(8);
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);

    // Send a response as if the server has sent down seven arrays and 50kbytes.
    response(7, 50000);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(BrowserChannel.RTT_ESTIMATE * 2);
    assertNotEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseInBufferedMode() {
    connect(8);
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.useChunked_ = false;
    sendMap('foo1', 'bar1');
    assertEquals(-1, browserChannel.lastPostResponseArrayId_);
    response(7, 111);

    assertEquals(1, browserChannel.lastArrayId_);
    assertEquals(7, browserChannel.lastPostResponseArrayId_);
    assertFalse(hasDeadBackChannelTimer());
    mockClock.tick(BrowserChannel.RTT_ESTIMATE * 2);
    assertNotEquals(BrowserChannel.Stat.BACKCHANNEL_DEAD, lastStatEvent);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithGarbage() {
    connect(8);
    sendMap('foo1', 'bar1');
    browserChannel.onRequestData(
        browserChannel.forwardChannelRequest_, 'garbage');
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testResponseWithGarbageInArray() {
    connect(8);
    sendMap('foo1', 'bar1');
    browserChannel.onRequestData(
        browserChannel.forwardChannelRequest_, '["garbage"]');
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testResponseWithEvilData() {
    connect(8);
    sendMap('foo1', 'bar1');
    browserChannel.onRequestData(
        browserChannel.forwardChannelRequest_,
        BrowserChannel.LAST_ARRAY_ID_RESPONSE_PREFIX +
            '=<script>evil()<\/script>&' +
            BrowserChannel.OUTSTANDING_DATA_RESPONSE_PREFIX +
            '=<script>moreEvil()<\/script>');
    assertEquals(BrowserChannel.State.CLOSED, browserChannel.getState());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathAbsolute() {
    connect(8, undefined, '/talkgadget');
    assertEquals(
        browserChannel.backChannelUri_.getDomain(), window.location.hostname);
    assertEquals(
        browserChannel.forwardChannelUri_.getDomain(),
        window.location.hostname);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathRelative() {
    connect(8, undefined, 'talkgadget');
    assertEquals(
        browserChannel.backChannelUri_.getDomain(), window.location.hostname);
    assertEquals(
        browserChannel.forwardChannelUri_.getDomain(),
        window.location.hostname);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPathWithHost() {
    connect(8, undefined, 'https://example.com');
    assertEquals(browserChannel.backChannelUri_.getScheme(), 'https');
    assertEquals(browserChannel.backChannelUri_.getDomain(), 'example.com');
    assertEquals(browserChannel.forwardChannelUri_.getScheme(), 'https');
    assertEquals(browserChannel.forwardChannelUri_.getDomain(), 'example.com');
  },

  testCreateXhrIo() {
    let xhr = browserChannel.createXhrIo(null);
    assertFalse(xhr.getWithCredentials());

    assertThrows(
        'Error connection to different host without CORS',
        goog.bind(browserChannel.createXhrIo, browserChannel, 'some_host'));

    browserChannel.setSupportsCrossDomainXhrs(true);

    xhr = browserChannel.createXhrIo(null);
    assertTrue(xhr.getWithCredentials());

    xhr = browserChannel.createXhrIo('some_host');
    assertTrue(xhr.getWithCredentials());
  },

  testSetParser() {
    const recordParse = recordFunction(JSON.parse);
    const parser = {};
    parser.parse = recordParse;
    browserChannel.setParser(parser);

    connect();
    assertEquals(3, recordParse.getCallCount());

    const call3 = recordParse.popLastCall();
    const call2 = recordParse.popLastCall();
    const call1 = recordParse.popLastCall();

    assertEquals(1, call1.getArguments().length);
    assertEquals('["b"]', call1.getArgument(0));

    assertEquals(1, call2.getArguments().length);
    assertEquals('[[0,["c","1234567890ABCDEF",null]]]', call2.getArgument(0));

    assertEquals(1, call3.getArguments().length);
    assertEquals('[[1,["foo"]]]', call3.getArgument(0));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAsyncTest() {
    /** @suppress {visibility} suppression added to enable type checking */
    browserChannel.asyncTest_ = true;
    browserChannel.connect('/test', '/bind');
    mockClock.tick(0);

    // We first establish the channel, assuming it is buffered.
    assertNotNull(browserChannel.forwardChannelRequest_);
    assertNull(browserChannel.connectionTest_);
    assertTrue(browserChannel.isBuffered());

    // Since we're assuming the channel is buferred, the "Close Immediately"
    // flag should be set.
    completeForwardChannel();
    assertEquals(
        '1',
        browserChannel.backChannelRequest_.requestUri_.queryData_.get('CI'));

    mockClock.tick(100);

    // Now, we perform the test which reveals the channel is not buffered.
    assertNotNull(browserChannel.connectionTest_);
    completeForwardTestConnection();
    completeBackTestConnection();
    assertFalse(browserChannel.isBuffered());

    // From now on, the "Close Immediately" flag should not be set.
    completeBackChannel();
    assertEquals(
        '0',
        browserChannel.backChannelRequest_.requestUri_.queryData_.get('CI'));
  },
});
