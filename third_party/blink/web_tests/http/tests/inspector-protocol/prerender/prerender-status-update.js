(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that prerender navigations receives the status updates`);
  const enableResponse = await dp.Preload.enable();
  testRunner.log(enableResponse);

  // Navigate to speculation rules Prerender Page.
  page.navigate('resources/simple-prerender.html');

  let statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'sessionId']);
  let loaderId = statusReport.params.key.loaderId;
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'sessionId']);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'sessionId']);

  session.evaluate(`document.getElementById('link').click()`);
  statusReport = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'sessionId']);
  let loaderIdActivation = statusReport.params.key.loaderId;

  if (loaderId !== loaderIdActivation) {
    testRunner.log('loaderId should remain consistent.');
  }

  testRunner.completeTest();
});
