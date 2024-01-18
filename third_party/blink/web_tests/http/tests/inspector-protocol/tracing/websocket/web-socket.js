// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests the data of WebSocket trace events');

  const TracingHelper =
      await testRunner.loadScript('../../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline,devtools.timeline');

  await session.evaluateAsync(`
      new Promise((resolve) => {
        const ws = new WebSocket('ws://localhost:8880/echo');
        ws.onopen = () => {
          ws.send('Hello');
        };
        ws.onmessage = () => {
          ws.close();
        }
        ws.onclose = resolve;
      });`);

  await tracingHelper.stopTracing(/(disabled-by-default-)?devtools\.timeline/);

  const webSocketCreate =
      tracingHelper.findEvent('WebSocketCreate', Phase.INSTANT);
  const webSocketSendHandshakeRequest =
      tracingHelper.findEvent('WebSocketSendHandshakeRequest', Phase.INSTANT);
  const webSocketReceiveHandshakeResponse = tracingHelper.findEvent(
      'WebSocketReceiveHandshakeResponse', Phase.INSTANT);
  const webSocketDestroy =
      tracingHelper.findEvent('WebSocketDestroy', Phase.INSTANT);

  testRunner.log('Got a WebSocketCreate event:');
  tracingHelper.logEventShape(webSocketCreate);
  testRunner.log('Got a WebSocketSendHandshakeRequest event:');
  tracingHelper.logEventShape(webSocketSendHandshakeRequest);
  testRunner.log('Got a WebSocketReceiveHandshakeResponse event:');
  tracingHelper.logEventShape(webSocketReceiveHandshakeResponse);
  testRunner.log('Got a WebSocketDestroy event:');
  tracingHelper.logEventShape(webSocketDestroy);

  testRunner.completeTest();
})
