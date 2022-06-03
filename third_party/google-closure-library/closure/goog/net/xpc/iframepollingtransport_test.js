// Copyright 2012 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.net.xpc.IframePollingTransportTest');
goog.setTestOnly();

const CfgFields = goog.require('goog.net.xpc.CfgFields');
const CrossPageChannel = goog.require('goog.net.xpc.CrossPageChannel');
const CrossPageChannelRole = goog.require('goog.net.xpc.CrossPageChannelRole');
const IframePollingTransport = goog.require('goog.net.xpc.IframePollingTransport');
const MockClock = goog.require('goog.testing.MockClock');
const TagName = goog.require('goog.dom.TagName');
const Timer = goog.require('goog.Timer');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const googObject = goog.require('goog.object');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/** @type {?MockClock} */
let mockClock = null;

/** @type {?CrossPageChannel} */
let outerChannel = null;

/** @type {?CrossPageChannel} */
let innerChannel = null;

/**
 * Creates a channel with the specified configuration, using frame polling.
 * @param {!CrossPageChannelRole} role The channel role.
 * @param {string} channelName The channel name.
 * @param {string} fromHostName The host name of the window hosting the channel.
 * @param {!Object} fromWindow The window hosting the channel.
 * @param {string} toHostName The host name of the peer window.
 * @param {!Object} toWindow The peer window.
 * @return {!CrossPageChannel}
 */
function createChannel(
    role, channelName, fromHostName, fromWindow, toHostName, toWindow) {
  // Build a channel config using frame polling.
  const channelConfig = googObject.create(
      CfgFields.ROLE, role, CfgFields.PEER_HOSTNAME, toHostName,
      CfgFields.CHANNEL_NAME, channelName, CfgFields.LOCAL_POLL_URI,
      `${fromHostName}/robots.txt`, CfgFields.PEER_POLL_URI,
      `${toHostName}/robots.txt`, CfgFields.TRANSPORT, IframePollingTransport);

  // Build the channel.
  const channel = new CrossPageChannel(channelConfig);

  channel.setPeerWindowObject(toWindow);

  // Update the transport's getWindow, to return the correct host window.
  channel.createTransport_();
  channel.transport_.getWindow = functions.constant(fromWindow);
  return channel;
}

/**
 * Creates a mock window to use as a peer. The peer window will host the frame
 * elements.
 * @param {string} url The peer window's initial URL.
 */
function createMockPeerWindow(url) {
  const mockPeer = createMockWindow(url);

  // Update the appendChild method to use a mock frame window.
  mockPeer.document.body.appendChild = (el) => {
    assertEquals(String(TagName.IFRAME), el.tagName);
    mockPeer.frames[el.name] = createMockWindow(el.src);
    mockPeer.document.body.element.appendChild(el);
  };

  return mockPeer;
}

/**
 * Creates a mock window.
 * @param {string} url The window's initial URL.
 */
function createMockWindow(url) {
  // Create the mock window, document and body.
  const mockWindow = {};
  const mockDocument = {};
  const mockBody = {};
  const mockLocation = {};

  // Configure the mock window's document body.
  mockBody.element = dom.createDom(TagName.BODY);

  // Configure the mock window's document.
  mockDocument.body = mockBody;

  // Configure the mock window's location.
  mockLocation.href = url;
  mockLocation.replace = (value) => {
    mockLocation.href = value;
  };

  // Configure the mock window.
  mockWindow.document = mockDocument;
  mockWindow.frames = {};
  mockWindow.location = mockLocation;
  mockWindow.setTimeout = Timer.callOnce;

  return mockWindow;
}

/**
 * Emulates closing the specified window by clearing frames, document and
 * location.
 */
function closeWindow(targetWindow) {
  // Close any child frame windows.
  for (let frameName in targetWindow.frames) {
    closeWindow(targetWindow.frames[frameName]);
  }

  // Clear the target window, set closed to true.
  targetWindow.closed = true;
  targetWindow.frames = null;
  targetWindow.document = null;
  targetWindow.location = null;
}
testSuite({
  setUp() {
    mockClock = new MockClock(true /* opt_autoInstall */);

    // Create the peer windows.
    const outerPeerHostName = 'https://www.youtube.com';
    const outerPeerWindow = createMockPeerWindow(outerPeerHostName);

    const innerPeerHostName = 'https://www.google.com';
    const innerPeerWindow = createMockPeerWindow(innerPeerHostName);

    // Create the channels.
    outerChannel = createChannel(
        CrossPageChannelRole.OUTER, 'test', outerPeerHostName, outerPeerWindow,
        innerPeerHostName, innerPeerWindow);
    innerChannel = createChannel(
        CrossPageChannelRole.INNER, 'test', innerPeerHostName, innerPeerWindow,
        outerPeerHostName, outerPeerWindow);
  },

  tearDown() {
    outerChannel.dispose();
    innerChannel.dispose();
    mockClock.uninstall();
  },

  /** Tests that connection happens normally and callbacks are invoked. */
  testConnect() {
    const outerConnectCallback = recordFunction();
    const innerConnectCallback = recordFunction();

    // Connect the two channels.
    outerChannel.connect(outerConnectCallback);
    innerChannel.connect(innerConnectCallback);
    mockClock.tick(1000);

    // Check that channels were connected and callbacks invoked.
    assertEquals(1, outerConnectCallback.getCallCount());
    assertEquals(1, innerConnectCallback.getCallCount());
    assertTrue(outerChannel.isConnected());
    assertTrue(innerChannel.isConnected());
  },

  /** Tests that messages are successfully delivered to the inner peer. */
  testSend_outerToInner() {
    const serviceCallback = recordFunction();

    // Register a service handler in the inner channel.
    innerChannel.registerService('svc', (payload) => {
      assertEquals('hello', payload);
      serviceCallback();
    });

    // Connect the two channels.
    outerChannel.connect();
    innerChannel.connect();
    mockClock.tick(1000);

    // Send a message.
    outerChannel.send('svc', 'hello');
    mockClock.tick(1000);

    // Check that the message was handled.
    assertEquals(1, serviceCallback.getCallCount());
  },

  /** Tests that messages are successfully delivered to the outer peer. */
  testSend_innerToOuter() {
    const serviceCallback = recordFunction();

    // Register a service handler in the inner channel.
    outerChannel.registerService('svc', (payload) => {
      assertEquals('hello', payload);
      serviceCallback();
    });

    // Connect the two channels.
    outerChannel.connect();
    innerChannel.connect();
    mockClock.tick(1000);

    // Send a message.
    innerChannel.send('svc', 'hello');
    mockClock.tick(1000);

    // Check that the message was handled.
    assertEquals(1, serviceCallback.getCallCount());
  },

  /** Tests that closing the outer peer does not cause an error. */
  testSend_outerPeerClosed() {
    // Connect the inner channel.
    innerChannel.connect();
    mockClock.tick(1000);

    // Close the outer peer before it has a chance to connect.
    closeWindow(innerChannel.getPeerWindowObject());

    // Allow timers to execute (and fail).
    mockClock.tick(1000);
  },

  /** Tests that closing the inner peer does not cause an error. */
  testSend_innerPeerClosed() {
    // Connect the outer channel.
    outerChannel.connect();
    mockClock.tick(1000);

    // Close the inner peer before it has a chance to connect.
    closeWindow(outerChannel.getPeerWindowObject());

    // Allow timers to execute (and fail).
    mockClock.tick(1000);
  },

  /** Tests that partially closing the outer peer does not cause an error. */
  testSend_outerPeerClosing() {
    // Connect the inner channel.
    innerChannel.connect();
    mockClock.tick(1000);

    // Close the outer peer before it has a chance to connect, but
    // leave closed set to false to simulate a partially closed window.
    closeWindow(innerChannel.getPeerWindowObject());
    innerChannel.getPeerWindowObject().closed = false;

    // Allow timers to execute (and fail).
    mockClock.tick(1000);
  },

  /** Tests that partially closing the inner peer does not cause an error. */
  testSend_innerPeerClosing() {
    // Connect the outer channel.
    outerChannel.connect();
    mockClock.tick(1000);

    // Close the inner peer before it has a chance to connect, but
    // leave closed set to false to simulate a partially closed window.
    closeWindow(outerChannel.getPeerWindowObject());
    outerChannel.getPeerWindowObject().closed = false;

    // Allow timers to execute (and fail).
    mockClock.tick(1000);
  },
});
