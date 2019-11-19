(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/cookie.pl',
      `Tests DidReceiveWebSocketHandshakeResponse probe.`);

  await dp.Network.enable();

  await testURL('ws://localhost:8880/binary-frames');
  testRunner.completeTest();

  async function testURL(url) {
    session.evaluate(`window.ws = new WebSocket('${url}');`);
    const value = await dp.Network.onceWebSocketHandshakeResponseReceived();
    const headers = value.params.response.headers;
    testRunner.log(`\nnew WebSocket('${url}')`);
    dumpHeader(headers, 'Set-Cookie');
    dumpHeader(headers, 'Connection');
    dumpHeader(headers, 'Upgrade');
  }

  function dumpHeader(headers, property) {
    testRunner.log(`${property}: ${headers[property]}`);
  }
})
