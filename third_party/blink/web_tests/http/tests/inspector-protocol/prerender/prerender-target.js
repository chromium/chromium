(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations receives the target updates`);

  const events = [];
  const logEvent = e => events.push(e);

  const tp = tabTargetSession.protocol;
  tp.Target.enable();

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();

  // For the triggering page.
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();
  await dp1.Target.enable();

  // Navigate to the triggering page.
  session1.navigate('resources/simple-prerender.html');

  async function waitForPrerenderStatusUpdated(dp, expectedStatus) {
    dp.Preload.oncePrerenderStatusUpdated(e => {
        events.push(e);
        return e.params.status === expectedStatus;
    });
  }

  // Wait until the prerendered page is ready.
  await Promise.all([
      waitForPrerenderStatusUpdated(dp1, 'Ready'),
      tp.Target.onceAttachedToTarget(e => events.push(e)),
  ]);

  // For the prerndered page.
  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  // Activate prerendered page.
  session1.evaluate(`document.getElementById('link').click()`);

  await Promise.all([
      waitForPrerenderStatusUpdated(dp2, 'Success'),
      tp.Target.onceDetachedFromTarget(e => events.push(e)),
      tp.Target.onceTargetInfoChanged(e => events.push(e)),
  ]);

  testRunner.log(events, 'Events (in order)');

  testRunner.completeTest();
});
