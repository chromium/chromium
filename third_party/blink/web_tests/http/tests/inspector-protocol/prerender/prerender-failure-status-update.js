(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations receives the failure status updates`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  session1.navigate('resources/bad-http-prerender.html');

  testRunner.log(
      await dp1.Preload.oncePrerenderStatusUpdated(), '',
      ['loaderId', 'sessionId']);
  testRunner.log(
      await dp1.Preload.oncePrerenderStatusUpdated(), '',
      ['loaderId', 'sessionId']);
  testRunner.log(
      await dp1.Preload.oncePrerenderStatusUpdated(), '',
      ['loaderId', 'sessionId']);

  testRunner.completeTest();
});
