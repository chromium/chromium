(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that navigator.sendBeacon() is reported as type Ping.`);

  await dp.Network.enable();
  session.evaluate(`
    navigator.sendBeacon('${testRunner.url('resources/empty.html')}', 'foo');
  `);

  dp.Network.onRequestWillBeSent(event => {
    const params = event.params;
    testRunner.log(params.type);
    testRunner.completeTest();
  });
})
