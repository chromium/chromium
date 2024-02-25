(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Verifies that Network.emulateNetworkConditions stops requests when offline is enabled for service workers.`);
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await session.navigate('resources/repeat-fetch-service-worker.html');

  const attachedPromise = dp.Target.onceAttachedToTarget();
  swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.js');
  const attachedToTarget = await attachedPromise;

  const serviceWorkerSession = session.createChild(attachedToTarget.params.sessionId);
  const swdp = serviceWorkerSession.protocol;

  const networkPromise = swdp.Network.enable();
  swdp.Runtime.runIfWaitingForDebugger();
  await networkPromise;

  // Wait for the main request to complete before going offline.
  await swdp.Network.onceLoadingFinished();

  const response = await swdp.Network.emulateNetworkConditions({
    downloadThroughput: -1,
    latency: 0,
    offline: true,
    uploadThroughput: -1
  });

  testRunner.log('response.error: ' + response.error);

  serviceWorkerSession.evaluate(`fetch('/foo')`);
  const loadingFailed = await swdp.Network.onceLoadingFailed();

  testRunner.log('loadingFailed.params.errorText: ' + loadingFailed.params.errorText);

  testRunner.log('navigator.onLine: ' + await serviceWorkerSession.evaluate('navigator.onLine'));

  testRunner.completeTest();
});
