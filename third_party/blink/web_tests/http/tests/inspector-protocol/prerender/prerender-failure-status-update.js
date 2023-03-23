 (async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations receives the failure status updates`);
  await dp.Preload.enable();

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/bad-http-prerender.html');
  let statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);
  testRunner.completeTest();
});
