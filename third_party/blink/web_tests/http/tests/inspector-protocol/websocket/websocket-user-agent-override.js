(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that WebSocket headers are set properly from overriden User Agent.`);

  var url = 'ws://localhost:8880/echo';
  var userAgentString = 'Mozilla/5.0 (Overridden User Agent)';

  testRunner.log('Test started');
  testRunner.log('Enabling network');
  await dp.Network.enable();
  testRunner.log('Network enabled');
  testRunner.log('Setting User Agent Override to: ' + userAgentString);
  await dp.Network.setUserAgentOverride({userAgent: userAgentString });
  testRunner.log('User Agent Set');
  testRunner.log('Sending command to open websocket');
  session.evaluate(`
    window.ws = new WebSocket('${url}');
  `);

  var request = await dp.Network.onceWebSocketWillSendHandshakeRequest();
  testRunner.log('User agent is: ' + request.params.request.headers['User-Agent']);
  testRunner.completeTest();
})
