(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations reports failure on triggering`);
  await dp.Page.enable();

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/multiple-prerender.html');
  const statusReport = await dp.Page.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
