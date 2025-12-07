/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for WebChannelBase.@suppress {accessControls}
 * Private methods are accessed for test purposes.
 */

goog.module('goog.labs.net.webChannel.webChannelBaseTransportTest');
goog.setTestOnly();

const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const ChannelRequest = goog.require('goog.labs.net.webChannel.ChannelRequest');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Timer = goog.require('goog.Timer');
const WebChannel = goog.require('goog.net.WebChannel');
const WebChannelBase = goog.require('goog.labs.net.webChannel.WebChannelBase');
const WebChannelBaseTransport = goog.require('goog.labs.net.webChannel.WebChannelBaseTransport');
const Wire = goog.require('goog.labs.net.webChannel.Wire');
const XhrIo = goog.require('goog.net.XhrIo');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googJson = goog.require('goog.json');
const testSuite = goog.require('goog.testing.testSuite');
const {anything} = goog.require('goog.labs.testing.AnythingMatcher');
const {atMost, times} = goog.require('goog.labs.mock.verification');
const {mock, mockFunction, verify} = goog.require('goog.labs.mock');

let webChannel;
const channelUrl = 'http://127.0.0.1:8080/channel';

const stubs = new PropertyReplacer();

/** Stubs ChannelRequest. */
function stubChannelRequest() {
  stubs.set(ChannelRequest, 'supportsXhrStreaming', functions.FALSE);
}

/**
 * Simulates the WebChannelBase firing the open event for the given channel.
 * @param {!WebChannelBase} channel The WebChannelBase.
 */
function simulateOpenEvent(channel) {
  assertNotNull(channel.getHandler());
  channel.getHandler().channelOpened(channel);
}

/**
 * Simulates the WebChannelBase firing the close event for the given channel.
 * @param {!WebChannelBase} channel The WebChannelBase.
 */
function simulateCloseEvent(channel) {
  assertNotNull(channel.getHandler());
  channel.getHandler().channelClosed(channel);
}

/**
 * Simulates the WebChannelBase firing the error event for the given channel.
 * @param {!WebChannelBase} channel The WebChannelBase.
 * @param {!WebChannelBase.Error} error
 */
function simulateErrorEvent(channel, error) {
  assertNotNull(channel.getHandler());
  channel.getHandler().channelError(channel, error);
}

/**
 * Simulates the WebChannelBase firing the message event for the given channel.
 * @param {!WebChannelBase} channel The WebChannelBase.
 * @param {!Object} data The message data array.
 */
function simulateMessageEvent(channel, data) {
  assertNotNull(channel.getHandler());
  channel.getHandler().channelHandleArray(channel, data);
}

testSuite({
  shouldRunTests() {
    return ChannelRequest.supportsXhrStreaming();
  },

  setUp() {},

  tearDown() {
    dispose(webChannel);
    stubs.reset();
  },

  testUnsupportedTransports() {
    stubChannelRequest();

    const err = assertThrows(() => {
      new WebChannelBaseTransport();
    });
    assertContains('error', err.message);
  },

  testOpenWithUrl() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    let eventFired = false;
    events.listen(webChannel, WebChannel.EventType.OPEN, (e) => {
      eventFired = true;
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateOpenEvent(channel);
    assertTrue(eventFired);
  },

  testOpenWithCustomHeaders() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'messageHeaders': {'foo-key': 'foo-value'}};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const extraHeaders_ = webChannel.channel_.extraHeaders_;
    assertNotNullNorUndefined(extraHeaders_);
    assertEquals('foo-value', extraHeaders_['foo-key']);
    assertEquals(undefined, extraHeaders_['X-Client-Protocol']);
  },

  testOpenWithInitHeaders() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'initMessageHeaders': {'foo-key': 'foo-value'}};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const initHeaders_ = webChannel.channel_.initHeaders_;
    assertNotNullNorUndefined(initHeaders_);
    assertEquals('foo-value', initHeaders_['foo-key']);
  },

  testOpenWithMessageContentType() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'messageContentType': 'application/protobuf+json'};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const initHeaders_ = webChannel.channel_.initHeaders_;
    assertNotNullNorUndefined(initHeaders_);
    assertEquals(
        'application/protobuf+json', initHeaders_['X-WebChannel-Content-Type']);
  },

  testOpenWithMessageContentTypeAndInitHeaders() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {
      'messageContentType': 'application/protobuf+json',
      'initMessageHeaders': {'foo-key': 'foo-value'},
    };
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const initHeaders_ = webChannel.channel_.initHeaders_;
    assertNotNullNorUndefined(initHeaders_);
    assertEquals(
        'application/protobuf+json', initHeaders_['X-WebChannel-Content-Type']);
    assertEquals('foo-value', initHeaders_['foo-key']);
  },

  testClientProtocolHeaderRequired() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'clientProtocolHeaderRequired': true};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const extraHeaders_ = webChannel.channel_.extraHeaders_;
    assertNotNullNorUndefined(extraHeaders_);
    assertEquals('webchannel', extraHeaders_['X-Client-Protocol']);
  },

  testClientProtocolHeaderNotRequiredByDefault() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const extraHeaders_ = webChannel.channel_.extraHeaders_;
    assertNull(extraHeaders_);
  },

  testClientProtocolHeaderRequiredWithCustomHeader() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {
      'clientProtocolHeaderRequired': true,
      'messageHeaders': {'foo-key': 'foo-value'},
    };
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const extraHeaders_ = webChannel.channel_.extraHeaders_;
    assertNotNullNorUndefined(extraHeaders_);
    assertEquals('foo-value', extraHeaders_['foo-key']);
    assertEquals('webchannel', extraHeaders_['X-Client-Protocol']);
  },

  async testOpenWithCustomParams() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'messageUrlParams': {'foo-key': 'foo-value'}};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    const mockXhrIo = mock(XhrIo);
    stubs.set(channel, 'createXhrIo', () => {
      return mockXhrIo;
    });

    webChannel.open();
    await Timer.promise(0);

    verify(mockXhrIo, times(1))
        .send(
            new ArgumentMatcher((uri) => {
              return uri.getParameterValue('foo-key') == 'foo-value';
            }),
            anything(), anything(), anything());
  },

  testOpenWithHttpSessionIdParam() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'httpSessionIdParam': 'xsessionid'};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const httpSessionIdParam = webChannel.channel_.getHttpSessionIdParam();
    assertEquals('xsessionid', httpSessionIdParam);
  },

  testOpenWithDuplicatedHttpSessionIdParam() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {
      'messageUrlParams': {'xsessionid': 'abcd1234'},
      'httpSessionIdParam': 'xsessionid',
    };
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    const httpSessionIdParam = webChannel.channel_.getHttpSessionIdParam();
    assertEquals('xsessionid', httpSessionIdParam);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const extraParams = webChannel.channel_.extraParams_;
    assertUndefined(extraParams['xsessionid']);
  },

  /** @suppress {strictMissingProperties} Accessing private property. */
  testOpenWithCorsEnabled() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'supportsCrossDomainXhr': true};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    assertTrue(webChannel.channel_.supportsCrossDomainXhrs_);
  },

  testSendRawJsonDefaultValue() {
    let channelMsg;
    stubs.set(WebChannelBase.prototype, 'sendMap', (message) => {
      channelMsg = message;
    });

    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);
    webChannel.open();

    webChannel.send({foo: 'bar'});
    assertEquals('bar', channelMsg.foo);
  },

  testSendRawJsonUndefinedValue() {
    let channelMsg;
    stubs.set(WebChannelBase.prototype, 'sendMap', (message) => {
      channelMsg = message;
    });

    const webChannelTransport = new WebChannelBaseTransport();
    const options = {};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    webChannel.send({foo: 'bar'});
    assertEquals('bar', channelMsg.foo);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSendRawJsonExplicitTrueValue() {
    let channelMsg;
    stubs.set(WebChannelBase.prototype, 'sendMap', (message) => {
      channelMsg = message;
    });
    stubs.set(WebChannelBase.prototype, 'getServerVersion', () => 12);

    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'sendRawJson': true};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    webChannel.send({foo: 'bar'});

    const receivedMsg = googJson.parse(channelMsg[Wire.RAW_DATA_KEY]);
    assertEquals('bar', receivedMsg.foo);
  },

  testSendRawJsonExplicitFalseValue() {
    let channelMsg;
    stubs.set(WebChannelBase.prototype, 'sendMap', (message) => {
      channelMsg = message;
    });
    stubs.set(WebChannelBase.prototype, 'getServerVersion', () => 12);

    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'sendRawJson': false};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    webChannel.send({foo: 'bar'});
    assertEquals('bar', channelMsg.foo);
  },

  testOpenThenCloseChannel() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    let eventFired = false;
    events.listen(webChannel, WebChannel.EventType.CLOSE, (e) => {
      eventFired = true;
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateCloseEvent(channel);
    assertTrue(eventFired);
  },

  async testOpenThenCloseChannelWithUpdatedCustomParams() {
    const webChannelTransport = new WebChannelBaseTransport();
    let messageUrlParams = {'foo-key': 'foo-value'};
    const options = {'messageUrlParams': messageUrlParams};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    const mockXhrIo = mock(XhrIo);
    stubs.set(channel, 'createXhrIo', () => {
      return mockXhrIo;
    });

    webChannel.open();
    await Timer.promise(0);

    verify(mockXhrIo, atMost(1))
        .send(anything(), anything(), anything(), anything());

    // Update internal webchannel state to OPENED so that the close request can
    // be sent.
    channel.state_ = WebChannelBase.State.OPENED;

    // Set a new custom url param to be sent with the close request.
    messageUrlParams['close-key'] = 'close-value';

    const sendBeaconMock = mockFunction();
    if (goog.global.navigator.sendBeacon) {
      stubs.replace(goog.global.navigator, 'sendBeacon', sendBeaconMock);
    } else {
      // IE doesn't support sendBeacon() so we'll set it directly.
      goog.global.navigator.sendBeacon = sendBeaconMock;
    }

    webChannel.close();
    await Timer.promise(0);

    verify(sendBeaconMock, times(1))(
        new ArgumentMatcher((uriStr) => {
          return uriStr.includes('close-key=close-value');
        }),
        anything());
  },

  testChannelError() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    const error = WebChannelBase.Error.NETWORK;
    let eventFired = false;
    events.listen(webChannel, WebChannel.EventType.ERROR, (e) => {
      eventFired = true;
      assertEquals(WebChannel.ErrorStatus.NETWORK_ERROR, e.status);
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateErrorEvent(channel, error);
    assertTrue(eventFired);
  },

  testChannelMessage() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    let eventFired = false;
    const data = {message: 'foo'};
    events.listen(webChannel, WebChannel.EventType.MESSAGE, (e) => {
      eventFired = true;
      assertEquals(data, e.data);
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateMessageEvent(channel, data);
    assertTrue(eventFired);
  },

  /**
   * @suppress {checkTypes} Allow sending a string as data, although not
   * supported by the method API, since it is done by clients.
   */
  testChannelMessage_stringDataSupported() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    let eventFired = false;
    const data = 'foo';
    events.listen(webChannel, WebChannel.EventType.MESSAGE, (e) => {
      eventFired = true;
      assertEquals(data, e.data);
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateMessageEvent(channel, data);
    assertTrue(eventFired);
  },

  testChannelMessage_WithMetadata() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);

    let eventFired = false;
    const headers = {'header': 'value'};
    const statusCode = 200;
    const data = {'__headers__': {'header': 'value'}, '__status__': statusCode};
    events.listen(webChannel, WebChannel.EventType.MESSAGE, (e) => {
      eventFired = true;
      assertObjectEquals({}, e.data);
      assertObjectEquals(headers, e.headers);
      assertEquals(statusCode, e.statusCode);
    });

    webChannel.open();
    assertFalse(eventFired);

    /** @suppress {strictMissingProperties} Accessing private property. */
    const channel = webChannel.channel_;
    assertNotNull(channel);

    simulateMessageEvent(channel, data);
    assertTrue(eventFired);
  },

  // ALLOW_ORIGIN_TRIAL_FEATURES = false
  testEnableOriginTrials() {
    const webChannelTransport = new WebChannelBaseTransport();
    let options = {
      'enableOriginTrials': true,
    };
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    let enabled = webChannel.channel_.enableOriginTrials_;
    assertFalse(enabled);

    options = {
      'enableOriginTrials': false,
    };
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    enabled = webChannel.channel_.enableOriginTrials_;
    assertFalse(enabled);

    options = {};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    enabled = webChannel.channel_.enableOriginTrials_;
    assertFalse(enabled);

    options = undefined;
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    webChannel.open();

    /** @suppress {strictMissingProperties} Accessing private property. */
    enabled = webChannel.channel_.enableOriginTrials_;
    assertFalse(enabled);
  },

  testGetNonAckedMessages_withJsObjectReturnsExactMessage() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);
    const messageToSend = {foo: 'bar'};
    const messageToSend2 = {foo2: 'bar2'};

    webChannel.open();
    webChannel.send(messageToSend);
    webChannel.send(messageToSend2);

    assertElementsEquals(
        [messageToSend, messageToSend2],
        webChannel.getRuntimeProperties().getNonAckedMessages());
  },

  testGetNonAckedMessages_withStringReturnsExactMessage() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);
    const messageToSend = 'foo';
    const messageToSend2 = 'foo2';

    webChannel.open();
    webChannel.send(messageToSend);
    webChannel.send(messageToSend2);

    assertElementsEquals(
        [messageToSend, messageToSend2],
        webChannel.getRuntimeProperties().getNonAckedMessages());
  },

  testGetNonAckedMessages_withRawJsonReturnsEqualObject() {
    const webChannelTransport = new WebChannelBaseTransport();
    const options = {'sendRawJson': true};
    webChannel = webChannelTransport.createWebChannel(channelUrl, options);
    const messageToSend = {foo: 'bar'};

    webChannel.open();
    webChannel.send(messageToSend);

    const nonAckedMessages =
        webChannel.getRuntimeProperties().getNonAckedMessages();
    assertEquals(1, nonAckedMessages.length);
    // JSON objects went through serialization and deserialization so an equal
    // (but not the same) object is returned.
    assertObjectEquals(messageToSend, nonAckedMessages[0]);
  },

  testGetNonAckedMessagesAfterChannelClose() {
    const webChannelTransport = new WebChannelBaseTransport();
    webChannel = webChannelTransport.createWebChannel(channelUrl);
    const messageToSend = {foo: 'bar'};
    const messageToSend2 = {foo2: 'bar2'};

    webChannel.open();
    webChannel.send(messageToSend);
    webChannel.send(messageToSend2);
    webChannel.close();

    assertElementsEquals(
        [messageToSend, messageToSend2],
        webChannel.getRuntimeProperties().getNonAckedMessages());
  },
});
