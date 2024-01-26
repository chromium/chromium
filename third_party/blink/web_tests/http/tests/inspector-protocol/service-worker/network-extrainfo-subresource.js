(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Verifies that subresource requests made by service workers get Network.*ExtraInfo events for them.');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  let swdp = null;
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    swdp = session.createChild(event.params.sessionId).protocol;
  });

  await session.navigate('resources/repeat-fetch-service-worker.html');
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.js');

  await dp.Page.enable();
  await dp.Page.reload();
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.js');
  await swdp.Network.enable();

  const [
    requestWillBeSent,
    requestWillBeSentExtraInfo,
    responseReceived,
    responseReceivedExtraInfo,
    evaluate
  ] = await Promise.all([
      swdp.Network.onceRequestWillBeSent(),
      swdp.Network.onceRequestWillBeSentExtraInfo(),
      swdp.Network.onceResponseReceived(),
      swdp.Network.onceResponseReceivedExtraInfo(),
      session.evaluate(`fetch('${testRunner.url('./resources/hello-world.txt')}')`)
  ]);

  const idsMatch = requestWillBeSent.params.requestId === requestWillBeSentExtraInfo.params.requestId
    && requestWillBeSent.params.requestId === responseReceived.params.requestId
    && requestWillBeSent.params.requestId === responseReceivedExtraInfo.params.requestId;
  testRunner.log('requestWillBeSent url: ' + requestWillBeSent.params.request.url);
  testRunner.log('requestIds match: ' + idsMatch);
  testRunner.completeTest();
});
