(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const { page, session, dp } = await testRunner.startBlank(
    `Tests that Preload.prefetchStatusUpdated is dispatched as Success for prefetch requests if prefetch cache is cleared on different origin.`);

  await dp.Preload.enable();

  // Navigate to the referral url to initiate the prefetch.
  session.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/referral_prefetch.https.html")

  // Confirm that the prefetch request is running.
  let statusReport = await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === "Running");
  testRunner.log(statusReport);

  // Confirm that the prefetch request is completed and ready to use.
  statusReport = await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === "Ready");
  testRunner.log(statusReport);

  // Trigger clearing the prefetch cache through Clear-Site-Data response headers
  // on different origin and then navigate to the target prefetch page.
  session.evaluate(`
    window.open('http://127.0.0.1:8000/inspector-protocol/prefetch/resources/clear-site-data-prefetchCache.php');
    setTimeout(() => {
      window.location.href = 'https://127.0.0.1:8443/inspector-protocol/prefetch/resources/target_prefetch.html';
    }, 1000);
  `);

  statusReport = await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === "Success");
  testRunner.log(statusReport);

  testRunner.completeTest();
});
