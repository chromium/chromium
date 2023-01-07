(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations reports bad http status failure on triggering`);
  await dp.Page.enable();

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/bad-http-prerender.html');
  const statusReport = await dp.Page.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
