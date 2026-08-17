(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session} = await testRunner.startBlank(
      'Tests that overriding CPU performance tier changes the navigator property');

  const tierNames = ['unknown', 'low', 'mid', 'high', 'ultra'];
  const nameToTier = {
    'unknown': 0,
    'low': 1,
    'mid': 2,
    'high': 3,
    'ultra': 4,
  };

  const currentUrl = await session.evaluate('location.href');

  async function testValid(run, tierName) {
    const expectedNumber = nameToTier[tierName];
    const response1 =
        await session.protocol.Emulation.setCPUPerformanceOverride(
            {performanceTier: tierName});
    if (response1.error) {
      testRunner.log(`Unexpected failure to set valid tier, run ${run}: ${
          response1.error.message}`);
    }
    const newValue = await session.evaluate('navigator.cpuPerformance');
    testRunner.log(
        `Match after setting tier, run ${run}: ${newValue === expectedNumber}`);

    await session.navigate(currentUrl);
    const response2 =
        await session.protocol.Emulation.setCPUPerformanceOverride(
            {performanceTier: tierName});
    if (response2.error) {
      testRunner.log(`Unexpected failure to set valid tier, run ${
          run} after reload: ${response2.error.message}`);
    }
    const afterReload = await session.evaluate('navigator.cpuPerformance');
    testRunner.log(`Match after setting tier, run ${run} after reload: ${
        afterReload === expectedNumber}`);
  }

  const defaultValue = await session.evaluate('navigator.cpuPerformance');

  for (let i = 1; i < 5; ++i) {
    const tierIndex = (defaultValue + i) % 5;
    await testValid(i, tierNames[tierIndex]);
  }

  const lastValue = await session.evaluate('navigator.cpuPerformance');

  async function testInvalid(overrideValue) {
    const response = await session.protocol.Emulation.setCPUPerformanceOverride(
        {performanceTier: overrideValue});
    if (response.error) {
      testRunner.log(`Failed to set invalid tier ${overrideValue}: ${
          response.error.message}`);
    } else {
      testRunner.log(
          `Unexpected success setting invalid tier ${overrideValue}`);
    }
    const afterInvalid = await session.evaluate('navigator.cpuPerformance');
    testRunner.log(`Match after setting invalid tier ${overrideValue}: ${
        afterInvalid === lastValue}`);
  }

  await testInvalid('bogus');

  async function testReset() {
    const response =
        await session.protocol.Emulation.setCPUPerformanceOverride({});
    if (response.error) {
      testRunner.log(
          `Unexpected failure to reset tier: ${response.error.message}`);
    }
    const clearedValue = await session.evaluate('navigator.cpuPerformance');
    testRunner.log(
        `Match after clearing tier: ${clearedValue === defaultValue}`);
  }

  await testReset();

  testRunner.completeTest();
})
