(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp} = await testRunner.startBlank(
      `Tests getDOMCountersForLeakDetection output.`);

  const {result} = await dp.Memory.getDOMCountersForLeakDetection();

  for (const counter of result.counters) {
    testRunner.log(`${counter.name}=${counter.count}`);
  }

  testRunner.completeTest();
})
