(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
        `Tests that Preload.prefetchStatusUpdated is dispatched for prefetch requests.`);

  await dp.Preload.enable();

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html")

  let statusReport = await dp.Preload.oncePrefetchStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);

  statusReport = await dp.Preload.oncePrefetchStatusUpdated();
  testRunner.log(statusReport, '', ['loaderId', 'initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
})
