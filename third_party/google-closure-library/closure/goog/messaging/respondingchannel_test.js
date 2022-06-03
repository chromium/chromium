/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.RespondingChannelTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const RespondingChannel = goog.require('goog.messaging.RespondingChannel');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const CH1_REQUEST = {
  'request': 'quux1'
};
const CH2_REQUEST = {
  'request': 'quux2'
};
const CH1_RESPONSE = {
  'response': 'baz1'
};
const CH2_RESPONSE = {
  'response': 'baz2'
};
const SERVICE_NAME = 'serviceName';

let mockControl;
let ch1;
let ch2;
let respondingCh1;
let respondingCh2;
let stubs;

testSuite({
  setUp() {
    mockControl = new MockControl();

    ch1 = new MockMessageChannel(mockControl);
    ch2 = new MockMessageChannel(mockControl);

    respondingCh1 = new RespondingChannel(ch1);
    respondingCh2 = new RespondingChannel(ch2);

    stubs = new PropertyReplacer();
  },

  tearDown() {
    respondingCh1.dispose();
    respondingCh2.dispose();
    mockControl.$verifyAll();
    stubs.reset();
  },

  testSendWithSignature() {
    // 1 to 2 and back.
    const message1Ch1Request = {'data': CH1_REQUEST, 'signature': 0};
    const message1Ch2Response = {'data': CH2_RESPONSE, 'signature': 0};
    const message2Ch1Request = {'data': CH1_REQUEST, 'signature': 1};
    const message2Ch2Response = {'data': CH2_RESPONSE, 'signature': 1};
    // 2 to 1 and back.
    const message3Ch2Request = {'data': CH2_REQUEST, 'signature': 0};
    const message3Ch1Response = {'data': CH1_RESPONSE, 'signature': 0};
    const message4Ch2Request = {'data': CH2_REQUEST, 'signature': 1};
    const message4Ch1Response = {'data': CH1_RESPONSE, 'signature': 1};

    // The RespondingChannel calls send() synchronously from its send() method.
    // Request sent from channel 1 to channel 2.
    ch1.send(`public:${SERVICE_NAME}`, message1Ch1Request);
    ch1.send(`public:${SERVICE_NAME}`, message2Ch1Request);

    // Request sent from channel 2 to channel 1.
    ch2.send(`public:${SERVICE_NAME}`, message3Ch2Request);
    ch2.send(`public:${SERVICE_NAME}`, message4Ch2Request);

    // It calls send() asynchronously when it receives a message.
    // Request sent from channel 1 to channel 2.
    ch2.send('private:mics', message1Ch2Response);
    ch2.send('private:mics', message2Ch2Response);

    // Request sent from channel 2 to channel 1.
    ch1.send('private:mics', message3Ch1Response);
    ch1.send('private:mics', message4Ch1Response);

    mockControl.$replayAll();

    let hasInvokedCh1 = false;
    let hasInvokedCh2 = false;
    let hasReturnedFromCh1 = false;
    let hasReturnedFromCh2 = false;

    const serviceCallback1 = (message) => {
      hasInvokedCh1 = true;
      assertObjectEquals(CH2_REQUEST, message);
      return CH1_RESPONSE;
    };

    const serviceCallback2 = (message) => {
      hasInvokedCh2 = true;
      assertObjectEquals(CH1_REQUEST, message);
      return CH2_RESPONSE;
    };

    const invocationCallback1 = (message) => {
      hasReturnedFromCh2 = true;
      assertObjectEquals(CH2_RESPONSE, message);
    };

    const invocationCallback2 = (message) => {
      hasReturnedFromCh1 = true;
      assertObjectEquals(CH1_RESPONSE, message);
    };

    respondingCh1.registerService(SERVICE_NAME, serviceCallback1);
    respondingCh2.registerService(SERVICE_NAME, serviceCallback2);

    respondingCh1.send(SERVICE_NAME, CH1_REQUEST, invocationCallback1);
    ch2.receive(`public:${SERVICE_NAME}`, message1Ch1Request);
    ch1.receive('private:mics', message1Ch2Response);

    respondingCh1.send(SERVICE_NAME, CH1_REQUEST, invocationCallback1);
    ch2.receive(`public:${SERVICE_NAME}`, message2Ch1Request);
    ch1.receive('private:mics', message2Ch2Response);

    respondingCh2.send(SERVICE_NAME, CH2_REQUEST, invocationCallback2);
    ch1.receive(`public:${SERVICE_NAME}`, message3Ch2Request);
    ch2.receive('private:mics', message3Ch1Response);

    respondingCh2.send(SERVICE_NAME, CH2_REQUEST, invocationCallback2);
    ch1.receive(`public:${SERVICE_NAME}`, message4Ch2Request);
    ch2.receive('private:mics', message4Ch1Response);

    // Wait for asynchronous calls to occur.
    return GoogPromise.resolve().then(() => {
      assertTrue(
          hasInvokedCh1 && hasInvokedCh2 && hasReturnedFromCh1 &&
          hasReturnedFromCh2);
    });
  },

  testWaitsForAsyncCallbackBeforeSendingResponse() {
    stubs.set(ch1, 'send', recordFunction());
    ch1.send('private:mics', {'data': CH1_RESPONSE, 'signature': 0});
    mockControl.$replayAll();

    const whenResponseReady = GoogPromise.withResolver();
    const serviceHandler = (message) => whenResponseReady.promise;

    respondingCh1.registerService(SERVICE_NAME, serviceHandler);
    ch1.receive(
        `public:${SERVICE_NAME}`, {'data': CH1_REQUEST, 'signature': 0});
    // The call to send() before $replayAll() counts as one call.
    ch1.send.assertCallCount(1);

    whenResponseReady.resolve(CH1_RESPONSE);
    return whenResponseReady.promise.then(() => {
      ch1.send.assertCallCount(2);
    });
  },
});
