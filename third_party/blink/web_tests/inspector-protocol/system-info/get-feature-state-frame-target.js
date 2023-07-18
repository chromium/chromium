(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests SystemInfo.getFeatureState() from frame target');

  const response =
      await dp.SystemInfo.getFeatureState({featureState: 'PreloadingHoldback'});
  testRunner.log(response);

  testRunner.completeTest();
})
