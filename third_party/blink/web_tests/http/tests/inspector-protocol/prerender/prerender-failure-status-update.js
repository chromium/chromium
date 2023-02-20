 (async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations receives the failure status updates`);
  await dp.Page.enable();

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/bad-http-prerender.html');
  let statusReport = await dp.Page.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  statusReport = await dp.Page.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  statusReport = await dp.Page.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);
  testRunner.completeTest();
});
