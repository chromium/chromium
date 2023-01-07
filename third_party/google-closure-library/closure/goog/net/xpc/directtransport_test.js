/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests the direct transport. */

goog.module('goog.net.xpc.DirectTransportTest');
goog.setTestOnly();

const CfgFields = goog.require('goog.net.xpc.CfgFields');
const CrossPageChannel = goog.require('goog.net.xpc.CrossPageChannel');
const CrossPageChannelRole = goog.require('goog.net.xpc.CrossPageChannelRole');
const GoogPromise = goog.require('goog.Promise');
const Level = goog.require('goog.log.Level');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const TransportTypes = goog.require('goog.net.xpc.TransportTypes');
const browser = goog.require('goog.labs.userAgent.browser');
const dom = goog.require('goog.dom');
const log = goog.require('goog.log');
const testSuite = goog.require('goog.testing.testSuite');
const xpc = goog.require('goog.net.xpc');

/**
 * Echo service name.
 * @const {string}
 */
const ECHO_SERVICE_NAME = 'echo';

/**
 * Response service name.
 * @const {string}
 */
const RESPONSE_SERVICE_NAME = 'response';

/**
 * Test Payload.
 * @const {string}
 */
const MESSAGE_PAYLOAD_1 = 'This is message payload 1.';

/**
 * The name id of the peer iframe.
 * @const {string}
 */
const PEER_IFRAME_ID = 'peer-iframe';

// Class aliases.

let outerXpc;
let innerXpc;
let peerIframe;
let channelName;
let messageIsSync = false;
let savedHtml;
let debugDiv;

function createIframe() {
  peerIframe = dom.createElement(TagName.IFRAME);
  peerIframe.id = PEER_IFRAME_ID;
  document.body.insertBefore(peerIframe, document.body.firstChild);
}

function getConfiguration(role, peerFrameId = undefined) {
  const cfg = {};
  cfg[CfgFields.TRANSPORT] = TransportTypes.DIRECT;
  if (peerFrameId != null) {
    cfg[CfgFields.IFRAME_ID] = peerFrameId;
  }
  cfg[CfgFields.CHANNEL_NAME] = channelName;
  cfg[CfgFields.ROLE] = role;
  return cfg;
}

testSuite({
  setUpPage() {
    // Show debug log
    debugDiv = dom.createElement(TagName.DIV);
    const logger = log.getLogger('goog.net.xpc');
    log.setLevel(logger, Level.ALL);
    log.addHandler(logger, (logRecord) => {
      const msgElm = dom.createDom(TagName.DIV);
      msgElm.innerHTML = logRecord.getMessage();
      dom.appendChild(debugDiv, msgElm);
    });
    TestCase.getActiveTestCase().promiseTimeout = 10000;  // 10s
  },

  setUp() {
    savedHtml = document.body.innerHTML;
    document.body.appendChild(debugDiv);
  },

  tearDown() {
    if (peerIframe) {
      document.body.removeChild(peerIframe);
      peerIframe = null;
    }
    if (outerXpc) {
      outerXpc.dispose();
      outerXpc = null;
    }
    if (innerXpc) {
      innerXpc.dispose();
      innerXpc = null;
    }
    /**
     * @suppress {strictMissingProperties,checkTypes} suppression added to
     * enable type checking
     */
    window.iframeLoadHandler = null;
    channelName = null;
    messageIsSync = false;
    document.body.innerHTML = savedHtml;
  },

  /** Tests 2 same domain frames using direct transport. */
  testDirectTransport() {
    // This test has been flaky on IE.
    // For now, disable.
    // Flakiness is tracked in http://b/18595666
    if (browser.isIE()) {
      return;
    }

    createIframe();
    channelName = xpc.getRandomString(10);
    outerXpc = new CrossPageChannel(
        getConfiguration(CrossPageChannelRole.OUTER, PEER_IFRAME_ID));
    // Outgoing service.
    outerXpc.registerService(ECHO_SERVICE_NAME, goog.nullFunction);
    // Incoming service.
    const resolver = GoogPromise.withResolver();
    outerXpc.registerService(RESPONSE_SERVICE_NAME, (message) => {
      assertEquals(
          'Received payload is equal to sent payload.', message,
          MESSAGE_PAYLOAD_1);
      resolver.resolve();
    });

    outerXpc.connect(() => {
      assertTrue(
          'XPC over direct channel is connected', outerXpc.isConnected());
      outerXpc.send(ECHO_SERVICE_NAME, MESSAGE_PAYLOAD_1);
    });
    // inner_peer.html calls this method at end of html.
    /**
     * @suppress {strictMissingProperties,missingProperties} suppression added
     * to enable type checking
     */
    window.iframeLoadHandler = () => {
      peerIframe.contentWindow.instantiateChannel(
          getConfiguration(CrossPageChannelRole.INNER));
    };
    peerIframe.src = 'testdata/inner_peer.html';

    return resolver.promise;
  },

  /** Tests 2 xpc's communicating with each other in the same window. */
  testSameWindowDirectTransport() {
    channelName = xpc.getRandomString(10);

    outerXpc =
        new CrossPageChannel(getConfiguration(CrossPageChannelRole.OUTER));
    outerXpc.setPeerWindowObject(self);

    // Outgoing service.
    outerXpc.registerService(ECHO_SERVICE_NAME, goog.nullFunction);

    const resolver = GoogPromise.withResolver();
    // Incoming service.
    outerXpc.registerService(RESPONSE_SERVICE_NAME, (message) => {
      assertEquals(
          'Received payload is equal to sent payload.', message,
          MESSAGE_PAYLOAD_1);
      resolver.resolve();
    });
    outerXpc.connect(() => {
      assertTrue(
          'XPC over direct channel, same window, is connected',
          outerXpc.isConnected());
      outerXpc.send(ECHO_SERVICE_NAME, MESSAGE_PAYLOAD_1);
    });

    innerXpc =
        new CrossPageChannel(getConfiguration(CrossPageChannelRole.INNER));
    innerXpc.setPeerWindowObject(self);
    // Incoming service.
    innerXpc.registerService(ECHO_SERVICE_NAME, (message) => {
      innerXpc.send(RESPONSE_SERVICE_NAME, message);
    });
    // Outgoing service.
    innerXpc.registerService(RESPONSE_SERVICE_NAME, goog.nullFunction);
    innerXpc.connect();
    return resolver.promise;
  },

  /** Tests 2 same domain frames using direct transport using sync mode. */
  testSyncMode() {
    // This test has been flaky on IE.
    // For now, disable.
    // Flakiness is tracked in http://b/18595666
    if (browser.isIE()) {
      return;
    }

    createIframe();
    channelName = xpc.getRandomString(10);

    const cfg = getConfiguration(CrossPageChannelRole.OUTER, PEER_IFRAME_ID);
    /**
     * @suppress {strictPrimitiveOperators} suppression added to enable type
     * checking
     */
    cfg[CfgFields.DIRECT_TRANSPORT_SYNC_MODE] = true;

    outerXpc = new CrossPageChannel(cfg);
    // Outgoing service.
    outerXpc.registerService(ECHO_SERVICE_NAME, goog.nullFunction);
    const resolver = GoogPromise.withResolver();
    // Incoming service.
    outerXpc.registerService(RESPONSE_SERVICE_NAME, (message) => {
      assertTrue('The message response was syncronous', messageIsSync);
      assertEquals(
          'Received payload is equal to sent payload.', message,
          MESSAGE_PAYLOAD_1);
      resolver.resolve();
    });
    outerXpc.connect(() => {
      assertTrue(
          'XPC over direct channel is connected', outerXpc.isConnected());
      messageIsSync = true;
      outerXpc.send(ECHO_SERVICE_NAME, MESSAGE_PAYLOAD_1);
      messageIsSync = false;
    });
    // inner_peer.html calls this method at end of html.
    /**
     * @suppress {strictMissingProperties,missingProperties} suppression added
     * to enable type checking
     */
    window.iframeLoadHandler = () => {
      const cfg = getConfiguration(CrossPageChannelRole.INNER);
      /**
       * @suppress {strictPrimitiveOperators} suppression added to enable type
       * checking
       */
      cfg[CfgFields.DIRECT_TRANSPORT_SYNC_MODE] = true;
      peerIframe.contentWindow.instantiateChannel(cfg);
    };
    peerIframe.src = 'testdata/inner_peer.html';
    return resolver.promise;
  },
});
