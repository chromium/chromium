(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations receives the status updates`);
  const enableResponse = await dp.Preload.enable();
  testRunner.log(enableResponse);

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/simple-prerender.html');

  let statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  session.evaluate(`document.getElementById('link').click()`);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
});
