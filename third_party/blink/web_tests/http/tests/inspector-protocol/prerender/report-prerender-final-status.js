(async function(testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations report the final status`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  await session1.navigate('resources/simple-prerender.html');

  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  session1.evaluate(`document.getElementById('link').click()`);
  testRunner.log(
      await dp2.Preload.oncePrerenderAttemptCompleted(), '',
      ['loaderId', 'initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
