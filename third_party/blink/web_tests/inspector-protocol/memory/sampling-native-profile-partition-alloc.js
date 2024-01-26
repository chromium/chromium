(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test sampling native memory profiler (PartitionAlloc hook).`);

  const chunkSize = 100000;
  await dp.Memory.startSampling({samplingInterval: chunkSize, suppressRandomness: true});
  testRunner.log('Sampling started');
  await session.evaluate(`window.memoryChunk = new Uint8Array(${chunkSize});`);
  const message = await dp.Memory.getSamplingProfile();
  await dp.Memory.stopSampling();
  testRunner.log('Sampling stopped');

  const profile = message.result.profile;
  const foundTheSample = profile.samples.some(sample =>
    sample.size >= chunkSize && sample.stack.some(frame => frame.includes('ArrayBuffer')));
  testRunner.log('Found sample: ' + foundTheSample);
  if (!foundTheSample)
    testRunner.log(profile);

  testRunner.completeTest();
})
