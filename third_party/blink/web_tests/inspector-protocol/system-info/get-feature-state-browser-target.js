(async function(testRunner) {
  await testRunner.startBlank(
      'Tests SystemInfo.getFeatureState() from browser target');

  const response = await testRunner.browserP().SystemInfo.getFeatureState(
      {featureState: 'PrerenderHoldback'});
  testRunner.log(response);

  testRunner.completeTest();
})
