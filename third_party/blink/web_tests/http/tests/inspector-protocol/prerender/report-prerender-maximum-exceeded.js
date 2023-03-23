(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations reports failure on triggering`);
  await dp.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/multiple-prerender.html');
  const statusReport = await dp.Preload.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
