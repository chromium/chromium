(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that reload of an OOPIF page doesn't cause a crash`);

  dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
  dp.Network.enable();

  dp.Page.navigate({url: 'http://localhost:8000/inspector-protocol/resources/iframe-navigation.html'});

  const oopifRequests = new Set();
  dp.Network.onRequestWillBeSent(event => {
    const params = event.params;
    if (/oopif/.test(params.request.url))
      oopifRequests.add(params.requestId);
  });
  dp.Network.onLoadingFinished(event => {
    if (!oopifRequests.has(event.params.requestId))
      return;
    // Site isolation disabled, nothing to test here, bail out.
    testRunner.log('PASSED: alive and kicking!');
    testRunner.completeTest();
  });
  const attachedEvent = await dp.Target.onceAttachedToTarget();
  session.evaluate(`location.reload()`);
  await dp.Target.onceDetachedFromTarget();
  await dp.Target.onceAttachedToTarget();

  testRunner.log('PASSED: alive and kicking!');
  testRunner.completeTest();
})
