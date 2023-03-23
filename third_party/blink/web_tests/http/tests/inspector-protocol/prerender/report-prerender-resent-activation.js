(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations report the resent activation.`);
  await dp.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  await page.navigate('resources/simple-prerender.html');
  session.evaluate(`document.getElementById('link').click()`);
  const statusReport = await dp.Preload.oncePrerenderAttemptCompleted();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);
  await dp.Preload.disable();
  dp.Preload.enable();
  const resentStatusReport = await dp.Preload.oncePrerenderAttemptCompleted();
  testRunner.log(resentStatusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
