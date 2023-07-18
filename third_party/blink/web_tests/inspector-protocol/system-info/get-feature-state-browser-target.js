(async function(testRunner) {
  await testRunner.startBlank(
      'Tests SystemInfo.getFeatureState() from browser target');

  const response = await testRunner.browserP().SystemInfo.getFeatureState(
      {featureState: 'PreloadingHoldback'});
  testRunner.log(response);

  testRunner.completeTest();
})
