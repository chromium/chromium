(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations receives the status updates`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  session1.navigate('resources/simple-prerender.html');

  // Pending
  const resultPending = await dp1.Preload.oncePrerenderStatusUpdated();
  testRunner.log(resultPending, '', ['loaderId', 'sessionId']);

  // Running
  testRunner.log(
      await dp1.Preload.oncePrerenderStatusUpdated(), '',
      ['loaderId', 'sessionId']);
  // Ready
  testRunner.log(
      await dp1.Preload.oncePrerenderStatusUpdated(), '',
      ['loaderId', 'sessionId']);

  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  // Activate prerendered page.
  session1.evaluate(`document.getElementById('link').click()`);

  // Success
  const resultSuccess = await dp2.Preload.oncePrerenderStatusUpdated();
  testRunner.log(resultSuccess, '', ['loaderId', 'sessionId']);

  if (resultPending.params.key.loaderId !== resultSuccess.params.key.loaderId) {
    testRunner.log('loaderId should remain consistent.');
  }

  testRunner.completeTest();
});
