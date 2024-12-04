(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
        `Tests that Preload.prefetchStatusUpdated is dispatched for prefetching a resource whose MIME type is not supported.`);

  await dp.Preload.enable();

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.mime.https.html")

  let statusReport = await dp.Preload.oncePrefetchStatusUpdated();
  testRunner.log(statusReport);

  statusReport = await dp.Preload.oncePrefetchStatusUpdated();
  testRunner.log(statusReport);

  testRunner.completeTest();
})
