(async function(testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      'Tests that Preload.prerenderStatusUpdated is sent for speculation rule with "prerender_until_script" action.');

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  session1.navigate(testRunner.url('resources/prerender-until-script.html'));

  // Pending
  testRunner.log(await dp1.Preload.oncePrerenderStatusUpdated());
  // Running
  testRunner.log(await dp1.Preload.oncePrerenderStatusUpdated());

  testRunner.completeTest();
})
