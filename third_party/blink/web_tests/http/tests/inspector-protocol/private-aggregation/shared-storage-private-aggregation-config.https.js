(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage events with private aggregation configs.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'https://127.0.0.1:8443/';
  const resources = baseOrigin +
      'inspector-protocol/shared-storage-private-aggregation/resources/'

  const events = [];
  await dp.Storage.setSharedStorageTracking({enable: true});

  session.evaluate(`
      const scriptURL = "${resources}shared-storage-module.js";
      sharedStorage.worklet.addModule(scriptURL);
  `);

  events.push((await dp.Storage.onceSharedStorageAccessed()).params);

  session.evaluate(`(function() {
      const data = {
          enableDebugMode: true,
          contributions: [{bucket: 1n, value: 2}]
      };
      const config = {
          filteringIdMaxBytes: 8,
          maxContributions: 20,
      };
      sharedStorage.run(
          "contribute-to-histogram",
          {data, privateAggregationConfig: config, keepAlive: true});
  })()`);

  events.push((await dp.Storage.onceSharedStorageAccessed()).params);

  session.evaluate(`(function() {
      const data = {
          enableDebugMode: true,
          contributions: [{bucket: 1n, value: 2}]
      };
      const config = {
          aggregationCoordinatorOrigin: "${baseOrigin}",
          contextId: "example_context_id",
      };
      sharedStorage.selectURL(
          "contribute-to-histogram",
          [{url: "https://google.com/"}, {url: "https://chromium.org/"}],
          {data, privateAggregationConfig: config});
  })()`);

  events.push((await dp.Storage.onceSharedStorageAccessed()).params);

  testRunner.log(events, 'Events: ', [
    'accessTime', 'mainFrameId', 'urnUuid', 'workletOrdinal', 'workletTargetId',
    'serializedData'
  ]);

  testRunner.completeTest();
})
