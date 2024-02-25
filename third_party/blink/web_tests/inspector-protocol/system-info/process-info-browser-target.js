(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      'Tests ProcessInfo retrieval from browser target');

  const response = await testRunner.browserP().SystemInfo.getProcessInfo();

  // The number of renderer processes varies, so log only the very first one.
  let seenRenderer = false;
  for (process of response.result.processInfo) {
    if (process.type === 'renderer') {
      if (!seenRenderer) {
        seenRenderer = true;
      } else {
        continue;
      }
    }
    // Avoid all processes but browser and renderer to ensure stable test.
    if (process.type === 'browser' || process.type === 'renderer')
      testRunner.log(process, 'Process:', ['id', 'cpuTime']);
  }

  testRunner.completeTest();
})
