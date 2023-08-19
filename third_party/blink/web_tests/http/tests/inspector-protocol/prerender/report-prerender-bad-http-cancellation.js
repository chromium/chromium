(async function(testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations reports bad http status failure on triggering`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  session1.navigate('resources/bad-http-prerender.html');
  testRunner.log(
      await dp1.Preload.oncePrerenderAttemptCompleted(), '',
      ['loaderId', 'initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
