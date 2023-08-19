 // This test makes sure that the inspector is notified of the final status when
 // prerendering is cancelled for some reasons. To emulate the cancellation,
 // this test navigates the prerender trigger page to an unrelated page so that
 // prerendering is cancelled with the `Destroyed` final status.
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
   session1.navigate('resources/empty.html?navigateaway');

   testRunner.log(
       await dp1.Preload.oncePrerenderAttemptCompleted(), '',
       ['loaderId', 'initiatingFrameId', 'sessionId']);

   testRunner.completeTest();
 });
