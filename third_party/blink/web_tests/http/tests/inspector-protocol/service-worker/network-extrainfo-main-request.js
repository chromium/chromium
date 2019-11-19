(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Verifies that requests made for service worker main scripts get Network.*ExtraInfo events for them.');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await session.navigate('resources/repeat-fetch-service-worker.html');

  swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.js');
  const attachedToTarget = await dp.Target.onceAttachedToTarget();

  const swdp = session.createChild(attachedToTarget.params.sessionId).protocol;
  await swdp.Network.enable();

  swdp.Runtime.runIfWaitingForDebugger();

  const [
    requestWillBeSent,
    requestWillBeSentExtraInfo,
    responseReceived,
    responseReceivedExtraInfo
  ] = await Promise.all([
      swdp.Network.onceRequestWillBeSent(),
      swdp.Network.onceRequestWillBeSentExtraInfo(),
      swdp.Network.onceResponseReceived(),
      swdp.Network.onceResponseReceivedExtraInfo()
  ]);

  const idsMatch = requestWillBeSent.params.requestId === requestWillBeSentExtraInfo.params.requestId
    && requestWillBeSent.params.requestId === responseReceived.params.requestId
    && requestWillBeSent.params.requestId === responseReceivedExtraInfo.params.requestId;
  testRunner.log('requestWillBeSent url: ' + requestWillBeSent.params.request.url);
  testRunner.log('requestIds match: ' + idsMatch);
  testRunner.completeTest();
});
