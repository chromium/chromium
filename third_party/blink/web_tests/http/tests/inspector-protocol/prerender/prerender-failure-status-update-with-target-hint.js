(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations receives the failure status updates`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp = session.protocol;
  await dp.Preload.enable();

  session.navigate('resources/bad-http-prerender-with-target-hint.html');

  // Pending
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  // Running
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  // Failure
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  testRunner.completeTest();
});
