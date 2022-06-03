/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.PortChannelTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const GoogPromise = goog.require('goog.Promise');
const MessagingMessageChannel = goog.requireType('goog.messaging.MessageChannel');
const MockControl = goog.require('goog.testing.MockControl');
const MockMessageEvent = goog.require('goog.testing.messaging.MockMessageEvent');
const PortChannel = goog.require('goog.messaging.PortChannel');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googJson = goog.require('goog.json');
const testSuite = goog.require('goog.testing.testSuite');

let mockControl;
let mockPort;
let portChannel;

let workerChannel;
let setUpPromise;
let timer;
let frameDiv;

/**
 * Registers a service on a channel that will accept a single test message and
 * then fire a Promise.
 * @param {!MessagingMessageChannel} channel
 * @param {string} name The service name.
 * @param {boolean=} objectPayload Whether incoming payloads should be parsed as
 *     Objects instead of raw strings.
 * @return {!GoogPromise<(string|!Object)>} A promise that resolves with the
 *     value of the first message received on the service.
 */
function registerService(channel, name, objectPayload = undefined) {
  return new GoogPromise((resolve, reject) => {
    channel.registerService(name, resolve, objectPayload);
  });
}

/** @suppress {visibility} suppression added to enable type checking */
function makeMessage(serviceName, payload) {
  let msg = {'serviceName': serviceName, 'payload': payload};
  msg[PortChannel.FLAG] = true;
  if (PortChannel.REQUIRES_SERIALIZATION_) {
    msg = googJson.serialize(msg);
  }
  return msg;
}

function expectNoMessage() {
  portChannel.registerDefaultService(
      mockControl.createFunctionMock('expectNoMessage'));
}

function receiveMessage(
    serviceName, payload, origin = undefined, ports = undefined) {
  mockPort.dispatchEvent(MockMessageEvent.wrap(
      makeMessage(serviceName, payload), origin || 'http://google.com',
      undefined, undefined, ports));
}

/** @suppress {visibility} suppression added to enable type checking */
function receiveNonChannelMessage(data) {
  if (PortChannel.REQUIRES_SERIALIZATION_ && typeof data !== 'string') {
    data = googJson.serialize(data);
  }
  mockPort.dispatchEvent(MockMessageEvent.wrap(data, 'http://google.com'));
}

// Integration tests

/**
 * Assert that two HTML5 MessagePorts are entangled by posting messages from
 * each to the other.
 * @param {!MessagePort} port1
 * @param {!MessagePort} port2
 * @return {!GoogPromise} Promise that settles when the assertion is complete.
 */
function assertPortsEntangled(port1, port2) {
  const port2Promise = new GoogPromise((resolve, reject) => {
                         port2.onmessage = resolve;
                       }).then((e) => {
    assertEquals(
        'First port 1 should send a message to port 2', 'port1 to port2',
        e.data);
    port2.postMessage('port2 to port1');
  });

  const port1Promise = new GoogPromise((resolve, reject) => {
                         port1.onmessage = resolve;
                       }).then((e) => {
    assertEquals(
        'Then port 2 should respond to port 1', 'port2 to port1', e.data);
  });

  port1.postMessage('port1 to port2');
  return GoogPromise.all([port1Promise, port2Promise]);
}

/**
 * @param {string=} url A URL to use for the iframe src (defaults to
 *     "testdata/portchannel_inner.html").
 * @return {!GoogPromise<HTMLIFrameElement>} A promise that resolves with the
 *     loaded iframe.
 */
function createIframe(url = undefined) {
  const iframe = dom.createDom(TagName.IFRAME, {
    style: 'display: none',
    src: url || 'testdata/portchannel_inner.html',
  });

  return new GoogPromise((resolve, reject) => {
           events.listenOnce(iframe, EventType.LOAD, resolve);
           dom.appendChild(frameDiv, iframe);
         })
      .then((e) => iframe.contentWindow);
}
testSuite({
  setUpPage() {
    frameDiv = dom.getElement('frame');

    // Use a relatively long timeout because the iframe created by createIframe
    // can take a couple seconds to load its JS. It seems to take a particularly
    // long time in Edge and Safari.
    TestCase.getActiveTestCase().promiseTimeout = 60 * 1000;

    if (!('Worker' in globalThis)) {
      return;
    }

    const worker = new Worker('testdata/portchannel_worker.js');
    workerChannel = new PortChannel(worker);

    setUpPromise = new GoogPromise((resolve, reject) => {
      worker.onmessage = (e) => {
        if (e.data == 'loaded') {
          resolve();
        }
      };
    });
  },

  tearDownPage() {
    dispose(workerChannel);
  },

  setUp() {
    timer = new Timer(50);
    mockControl = new MockControl();
    mockPort = new GoogEventTarget();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockPort.postMessage = mockControl.createFunctionMock('postMessage');
    /** @suppress {checkTypes} suppression added to enable type checking */
    portChannel = new PortChannel(mockPort);

    if ('Worker' in globalThis) {
      // Ensure the worker channel has started before running each test.
      return setUpPromise;
    }
  },

  tearDown() {
    dispose(timer);
    portChannel.dispose();
    dom.removeChildren(frameDiv);
    mockControl.$verifyAll();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPostMessage() {
    mockPort.postMessage(makeMessage('foobar', 'This is a value'), []);
    mockControl.$replayAll();
    portChannel.send('foobar', 'This is a value');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPostMessageWithPorts() {
    if (!('MessageChannel' in globalThis)) {
      return;
    }
    const channel = new MessageChannel();
    const port1 = channel.port1;
    const port2 = channel.port2;
    mockPort.postMessage(
        makeMessage('foobar', {
          'val': [
            {'_port': {'type': 'real', 'index': 0}},
            {'_port': {'type': 'real', 'index': 1}},
          ],
        }),
        [port1, port2]);
    mockControl.$replayAll();
    portChannel.send('foobar', {'val': [port1, port2]});
  },

  testReceiveMessage() {
    const promise = registerService(portChannel, 'foobar');
    receiveMessage('foobar', 'This is a string');
    return promise.then((msg) => {
      assertEquals(msg, 'This is a string');
    });
  },

  testReceiveMessageWithPorts() {
    if (!('MessageChannel' in globalThis)) {
      return;
    }
    const channel = new MessageChannel();
    const port1 = channel.port1;
    const port2 = channel.port2;
    const promise = registerService(portChannel, 'foobar', true);

    receiveMessage(
        'foobar', {
          'val': [
            {'_port': {'type': 'real', 'index': 0}},
            {'_port': {'type': 'real', 'index': 1}},
          ],
        },
        null, [port1, port2]);

    return promise.then((msg) => {
      assertObjectEquals(msg, {'val': [port1, port2]});
    });
  },

  testReceiveNonChannelMessageWithStringBody() {
    expectNoMessage();
    mockControl.$replayAll();
    receiveNonChannelMessage('Foo bar');
  },

  testReceiveNonChannelMessageWithArrayBody() {
    expectNoMessage();
    mockControl.$replayAll();
    receiveNonChannelMessage([5, 'Foo bar']);
  },

  testReceiveNonChannelMessageWithNoFlag() {
    expectNoMessage();
    mockControl.$replayAll();
    receiveNonChannelMessage(
        {serviceName: 'foobar', payload: 'this is a payload'});
  },

  testReceiveNonChannelMessageWithFalseFlag() {
    expectNoMessage();
    mockControl.$replayAll();
    const body = {serviceName: 'foobar', payload: 'this is a payload'};
    body[PortChannel.FLAG] = false;
    receiveNonChannelMessage(body);
  },

  testWorker() {
    if (!('Worker' in globalThis)) {
      return;
    }
    const promise = registerService(workerChannel, 'pong', true);
    workerChannel.send('ping', {'val': 'fizzbang'});

    return promise.then((msg) => {
      assertObjectEquals({'val': 'fizzbang'}, msg);
    });
  },

  testWorkerWithPorts() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }
    const messageChannel = new MessageChannel();
    const promise = registerService(workerChannel, 'pong', true);
    workerChannel.send('ping', {'port': messageChannel.port1});

    return promise.then(
        (msg) => assertPortsEntangled(msg['port'], messageChannel.port2));
  },

  testPort() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }
    const messageChannel = new MessageChannel();
    workerChannel.send('addPort', messageChannel.port1);
    messageChannel.port2.start();
    const realPortChannel = new PortChannel(messageChannel.port2);
    const promise = registerService(realPortChannel, 'pong', true);
    realPortChannel.send('ping', {'val': 'fizzbang'});

    return promise.then((msg) => {
      assertObjectEquals({'val': 'fizzbang'}, msg);

      messageChannel.port2.close();
      realPortChannel.dispose();
    });
  },

  testPortIgnoresOrigin() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }
    const messageChannel = new MessageChannel();
    workerChannel.send('addPort', messageChannel.port1);
    messageChannel.port2.start();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const realPortChannel =
        new PortChannel(messageChannel.port2, 'http://somewhere-else.com');
    const promise = registerService(realPortChannel, 'pong', true);
    realPortChannel.send('ping', {'val': 'fizzbang'});

    return promise.then((msg) => {
      assertObjectEquals({'val': 'fizzbang'}, msg);

      messageChannel.port2.close();
      realPortChannel.dispose();
    });
  },

  testWindow() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }

    return createIframe().then((iframe) => {
      const peerOrigin = window.location.protocol + '//' + window.location.host;
      /** @suppress {checkTypes} suppression added to enable type checking */
      const iframeChannel =
          PortChannel.forEmbeddedWindow(iframe, peerOrigin, timer);

      const promise = registerService(iframeChannel, 'pong');
      iframeChannel.send('ping', 'fizzbang');

      return promise.then((msg) => {
        assertEquals('fizzbang', msg);

        dispose(iframeChannel);
      });
    });
  },

  testWindowCanceled() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }

    return createIframe().then((iframe) => {
      const peerOrigin = window.location.protocol + '//' + window.location.host;
      /** @suppress {checkTypes} suppression added to enable type checking */
      const iframeChannel =
          PortChannel.forEmbeddedWindow(iframe, peerOrigin, timer);
      iframeChannel.cancel();

      const promise = registerService(iframeChannel, 'pong').then((msg) => {
        fail('no messages should be received due to cancellation');
      });
      iframeChannel.send('ping', 'fizzbang');

      // Leave plenty of time for the connection to be made if the test fails,
      // but stop the test before the timeout is hit.
      return GoogPromise.race([promise, Timer.promise(500)]).thenAlways(() => {
        iframeChannel.dispose();
      });
    });
  },

  testWindowWontSendToWrongOrigin() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }

    return createIframe().then((iframe) => {
      /** @suppress {checkTypes} suppression added to enable type checking */
      const iframeChannel = PortChannel.forEmbeddedWindow(
          iframe, 'http://somewhere-else.com', timer);

      const promise = registerService(iframeChannel, 'pong').then((msg) => {
        fail('Should not receive pong from unexpected origin');
      });
      iframeChannel.send('ping', 'fizzbang');

      return GoogPromise.race([promise, Timer.promise(500)]).thenAlways(() => {
        iframeChannel.dispose();
      });
    });
  },

  testWindowWontReceiveFromWrongOrigin() {
    if (!('Worker' in globalThis) || !('MessageChannel' in globalThis)) {
      return;
    }

    return createIframe('testdata/portchannel_wrong_origin_inner.html')
        .then((iframe) => {
          const peerOrigin =
              window.location.protocol + '//' + window.location.host;
          /**
           * @suppress {checkTypes} suppression added to enable type checking
           */
          const iframeChannel =
              PortChannel.forEmbeddedWindow(iframe, peerOrigin, timer);

          const promise = registerService(iframeChannel, 'pong').then((msg) => {
            fail('Should not receive pong from unexpected origin');
          });
          iframeChannel.send('ping', 'fizzbang');

          return GoogPromise.race([promise, Timer.promise(500)])
              .thenAlways(() => {
                iframeChannel.dispose();
              });
        });
  },
});
