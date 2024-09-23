(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/simple.html',
      'Verifies that the request head has static routing information on the main resource.');
  const swHelper =
      (await testRunner.loadScript('../../resources/service-worker-helper.js'))(
          dp, session);

  await Promise.all([
    dp.Network.enable(),
    dp.Page.enable(),
  ]);

  await swHelper.installSWAndWaitForActivated(
      'service-worker-router-fetch-all.js');

  const responseReceivedPromise = dp.Network.onceResponseReceived();

  await dp.Page.reload();

  const responseReceived = await responseReceivedPromise;
  testRunner.log(responseReceived.params.response.serviceWorkerRouterInfo);

  const router_evaluation_start = responseReceived.params.response.timing.workerRouterEvaluationStart !== undefined;
  testRunner.log("workerRouterEvaluationStart exists: " + router_evaluation_start);

  testRunner.completeTest();
});
