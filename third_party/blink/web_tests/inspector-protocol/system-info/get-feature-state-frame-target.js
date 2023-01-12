(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests SystemInfo.getFeatureState() from frame target');

  const response =
      await dp.SystemInfo.getFeatureState({featureState: 'PrerenderHoldback'});
  testRunner.log(response);

  testRunner.completeTest();
})
