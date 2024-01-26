(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Verifies that WebSocket binary messages are encoded as base64 over the protocol');
  await dp.Network.enable();

  dp.Network.onWebSocketFrameReceived(event => {
    testRunner.log('base64 payload: ' + event.params.response.payloadData);
    testRunner.completeTest();
  });
  session.evaluate(`new WebSocket('ws://localhost:8880/binary-frames')`);
})
