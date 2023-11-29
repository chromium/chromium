(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/simple.html',
      'Verifies that the request head has static routing information on sub resources.');
  const swHelper =
      (await testRunner.loadScript('../../resources/service-worker-helper.js'))(
          dp, session);

  await Promise.all([
    dp.Network.enable(),
    dp.Page.enable(),
  ]);

  await swHelper.installSWAndWaitForActivated(
      'service-worker-router-fetch-all.js');

  await session.evaluate(
      `fetch('${testRunner.url('./resources/does-not-exists.txt')}')`)

  const responseReceived = await dp.Network.onceResponseReceived();
  testRunner.log(responseReceived.params.response.serviceWorkerRouterInfo);

  testRunner.completeTest();
});
