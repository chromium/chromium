(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Verifies which network conditions socket-layer network throttling ' +
      'accepts.');

  const rule = {
    urlPattern: '',
    latency: 100,
    downloadThroughput: -1,
    uploadThroughput: -1,
  };
  const patternRule = {
    urlPattern: 'http://127.0.0.1:*/*',
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1,
  };

  async function logResponse(description, responsePromise) {
    const {result, error} = await responsePromise;
    if (error) {
      testRunner.log(`${description}: error ${error.code}: ${error.message}`);
    } else if (result.ruleIds) {
      testRunner.log(`${description}: ${result.ruleIds.length} rule id(s)`);
    } else {
      testRunner.log(`${description}: success`);
    }
  }

  function emulateByRule(params) {
    return dp.Network.emulateNetworkConditionsByRule(params);
  }

  await logResponse('A rule without a URL pattern',
                    emulateByRule({matchedNetworkConditions: [rule]}));
  await logResponse('A throttling rule with a URL pattern', emulateByRule({
                      matchedNetworkConditions: [{...patternRule, latency: 100}]
                    }));
  await logResponse(
      'An offline rule with a URL pattern',
      emulateByRule(
          {matchedNetworkConditions: [{...patternRule, offline: true}]}));
  await logResponse(
      'An offline rule with a URL pattern before a throttling one',
      emulateByRule({
        matchedNetworkConditions: [{...patternRule, offline: true}, rule],
      }));
  await logResponse('A rule with a URL pattern that doesn\'t throttle',
                    emulateByRule({matchedNetworkConditions: [patternRule]}));
  await logResponse(
      'A rule with a URL pattern that doesn\'t throttle before a throttling one',
      emulateByRule({matchedNetworkConditions: [patternRule, rule]}));
  await logResponse(
      'A negative latency',
      emulateByRule({matchedNetworkConditions: [{...rule, latency: -1}]}));
  await logResponse(
      'An overflowing latency',
      emulateByRule({matchedNetworkConditions: [{...rule, latency: 1e308}]}));
  // The socket layer applies no values of offline rules.
  await logResponse(
      'A negative latency in an offline rule', emulateByRule({
        matchedNetworkConditions: [{...rule, latency: -1, offline: true}],
      }));
  await logResponse(
      'Offline',
      emulateByRule({offline: true, matchedNetworkConditions: [rule]}));
  await logResponse(
      'Offline, with a throttling rule with a URL pattern', emulateByRule({
        offline: true,
        matchedNetworkConditions: [{...patternRule, latency: 100}],
      }));
  await logResponse(
      'An offline rule',
      emulateByRule({matchedNetworkConditions: [{...rule, offline: true}]}));
  await logResponse('Offline service worker', emulateByRule({
                      emulateOfflineServiceWorker: true,
                      matchedNetworkConditions: [rule],
                    }));
  const deprecatedConditions = {
    offline: false,
    latency: -1,
    downloadThroughput: -1,
    uploadThroughput: -1
  };
  await logResponse(
      'A negative latency with the deprecated emulateNetworkConditions',
      dp.Network.emulateNetworkConditions(
          {...deprecatedConditions, downloadThroughput: 1024}));
  // Conditions that don't throttle don't reach the socket layer.
  await logResponse(
      'A negative latency without throttling, with the deprecated ' +
          'emulateNetworkConditions',
      dp.Network.emulateNetworkConditions(deprecatedConditions));

  testRunner.completeTest();
})
