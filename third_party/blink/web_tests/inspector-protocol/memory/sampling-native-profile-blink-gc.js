(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test sampling native memory profiler (Blink GC hook).`);

  await dp.Memory.startSampling({samplingInterval: 10000, suppressRandomness: true});
  testRunner.log('Sampling started');

  await session.evaluate(function() {
    const div = document.createElement('div');
    document.body.appendChild(div);
    for (let i = 0; i < 1000; ++i)
      div.addEventListener('click', new Function());
  });
  const message = await dp.Memory.getSamplingProfile();
  await dp.Memory.stopSampling();
  testRunner.log('Sampling stopped');

  const profile = message.result.profile;
  const foundTheSample = profile.samples.some(sample =>
    sample.stack.some(frame => frame.includes('AddEventListener')));
  testRunner.log('Found sample: ' + foundTheSample);
  if (!foundTheSample)
    testRunner.log(profile);

  testRunner.completeTest();
})
