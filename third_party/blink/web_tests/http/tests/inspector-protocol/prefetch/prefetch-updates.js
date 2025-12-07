(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  async function waitUntilStatus(dp, status) {
    await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === status);
  }

  async function basicTest() {
    const { page, dp } = await testRunner.startBlank(
      `Tests that Preload.prefetchStatusUpdated is dispatched for prefetch requests.`);

    await dp.Preload.enable();

    page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html")

    let statusReport = await dp.Preload.oncePrefetchStatusUpdated();
    testRunner.log(statusReport);

    statusReport = await dp.Preload.oncePrefetchStatusUpdated();
    testRunner.log(statusReport);
  }

  async function testRedispatchAfterEnable() {
    const { page, dp } = await testRunner.startBlank(
      `Tests that Preload.prefetchStatusUpdated is redispatched for a previously completed prefetch request.`);
    await dp.Preload.enable();
    page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html")

    // Wait till the prefetch completes.
    await waitUntilStatus(dp, 'Ready');

    // Disable and re-enable the Preload domain.
    await dp.Preload.disable();
    dp.Preload.enable();

    // A status update should be dispatched with the latest status.
    const prefetchStatusUpdated = (await dp.Preload.oncePrefetchStatusUpdated()).params;
    testRunner.log(prefetchStatusUpdated);
  }

  async function testRedispatchAfterEnable_FailedPrefetch() {
    const { page, dp } = await testRunner.startBlank(
      `Tests that Preload.prefetchStatusUpdated is redispatched for a previously failed prefetch request.`);
    await dp.Preload.enable();
    page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.mime.https.html")

    // Wait till the prefetch fails.
    await waitUntilStatus(dp, 'Failure');

    // Disable and re-enable the Preload domain.
    await dp.Preload.disable();
    dp.Preload.enable();

    // A status update should be dispatched with the latest status.
    const prefetchStatusUpdated = (await dp.Preload.oncePrefetchStatusUpdated()).params;
    testRunner.log(prefetchStatusUpdated);
  }

  async function testNoRedispatchAfterEnableIfCandidateRemoved() {
    const { page, dp, session } = await testRunner.startBlank(
      `Tests that Preload.prefetchStatusUpdated is not redispatched for a prefetch request if the candidate was removed.`);
    await dp.Preload.enable();
    page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html");
    await waitUntilStatus(dp, 'Ready');

    // Remove speculation candidate and wait for failure status.
    session.evaluate('document.getElementById("speculationrule").remove()');
    await waitUntilStatus(dp, 'Failure');

    // Disable and re-enable Preload domain. We should not see the failure
    // update re-dispatched for the removed candidate.
    await dp.Preload.disable();
    dp.Preload.onPrefetchStatusUpdated(() => {
      testRunner.fail('Received status update for removed speculation candidate.');
    });
    await dp.Preload.enable();
    testRunner.log('No update received after Preload domain is re-enabled.');
  }

  testRunner.runTestSuite([
    basicTest,
    testRedispatchAfterEnable,
    testRedispatchAfterEnable_FailedPrefetch,
    testNoRedispatchAfterEnableIfCandidateRemoved
  ]);
});
