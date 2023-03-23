 // This test makes sure that the inspector is notified of the final status when
 // prerendering is cancelled for some reasons. To emulate the cancellation,
 // this test navigates the prerender trigger page to an unrelated page so that
 // prerendering is cancelled with the `Destroyed` final status.
(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations report the final status`);
  await dp.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  await page.navigate('resources/simple-prerender.html');
  page.navigate('resources/empty.html?navigateaway');
  const statusReport = await dp.Preload.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);
  testRunner.completeTest();
});
