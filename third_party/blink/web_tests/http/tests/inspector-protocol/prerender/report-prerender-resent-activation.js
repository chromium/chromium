(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations report the resent activation.`);
  await dp.Page.enable();

  // Navigate to speculation rules Prerender Page.
  await page.navigate('resources/simple-prerender.html');
  session.evaluate(`document.getElementById('link').click()`);
  const statusReport = await dp.Page.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  await dp.Page.disable();
  dp.Page.enable();
  const resentStatusReport = await dp.Page.oncePrerenderAttemptCompleted();
  testRunner.log(resentStatusReport, '', ['initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
