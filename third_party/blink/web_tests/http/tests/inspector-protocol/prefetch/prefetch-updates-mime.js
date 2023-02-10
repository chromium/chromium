(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
        `Tests that Page.prefetchStatusUpdated is dispatched for prefetching a resource whose MIME type is not supported.`);

  await dp.Page.enable();

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.mime.https.html")

  let statusReport = await dp.Page.oncePrefetchStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  statusReport = await dp.Page.oncePrefetchStatusUpdated();
  testRunner.log(statusReport, '', ['initiatingFrameId', 'sessionId']);

  testRunner.completeTest();
})
