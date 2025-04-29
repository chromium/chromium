(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  async function clearSiteDataPrefetchCacheValueTest() {
    const {page, session, dp} = await testRunner.startBlank(
        `Tests that Preload.prefetchStatusUpdated is dispatched as Failure for prefetch requests if prefetch cache is cleared on same origin using prefetchCache value.`);

    await dp.Preload.enable();

    // Navigate to the referral url to initiate the prefetch.
    session.navigate(
        'https://127.0.0.1:8443/inspector-protocol/prefetch/resources/referral_prefetch.https.html')

    // Confirm that the prefetch request is running.
    let statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Running');
    testRunner.log(statusReport);

    // Confirm that the prefetch request is completed and ready to use.
    statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Ready');
    testRunner.log(statusReport);

    // Trigger clearing the prefetch cache through Clear-Site-Data response
    // headers on same origin and then navigate to the target prefetch page.
    session.evaluate(`
      window.open('https://127.0.0.1:8443/inspector-protocol/prefetch/resources/clear-site-data-prefetchCache.php');
      setTimeout(() => {
        window.location.href = 'https://127.0.0.1:8443/inspector-protocol/prefetch/resources/target_prefetch.html';
      }, 1000);
    `);

    // Confirm that the prefetch request failed after clearing the prefetch
    // cache.
    statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Failure');
    testRunner.log(statusReport);
  }

  async function clearSiteDataCacheValueTest() {
    const {page, session, dp} = await testRunner.startBlank(
        `Tests that Preload.prefetchStatusUpdated is dispatched as Failure for prefetch requests if prefetch cache is cleared on same origin using cache value.`);

    await dp.Preload.enable();

    // Navigate to the referral url to initiate the prefetch.
    session.navigate(
        'https://127.0.0.1:8443/inspector-protocol/prefetch/resources/referral_prefetch.https.html')

    // Confirm that the prefetch request is running.
    let statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Running');
    testRunner.log(statusReport);

    // Confirm that the prefetch request is completed and ready to use.
    statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Ready');
    testRunner.log(statusReport);

    // Trigger clearing the prefetch cache through Clear-Site-Data response
    // headers on same origin and then navigate to the target prefetch page.
    session.evaluate(`
      window.open('https://127.0.0.1:8443/inspector-protocol/prefetch/resources/clear-site-data-cache.php');
      setTimeout(() => {
        window.location.href = 'https://127.0.0.1:8443/inspector-protocol/prefetch/resources/target_prefetch.html';
      }, 1000);
    `);

    // Confirm that the prefetch request failed after clearing the prefetch
    // cache.
    statusReport = await dp.Preload.oncePrefetchStatusUpdated(
        e => e.params.status === 'Failure');
    testRunner.log(statusReport);
  }

  testRunner.runTestSuite(
      [clearSiteDataPrefetchCacheValueTest, clearSiteDataCacheValueTest]);
});
