(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Test sampling native memory snapshot.`);

  await dp.Memory.startSampling({samplingInterval: 10000, suppressRandomness: true});

  await session.evaluate(`
    const canvas = document.createElement('canvas');
    canvas.width = 640;
    canvas.height = 1024;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
    document.body.appendChild(canvas);
    `);
  const message = await dp.Memory.getAllTimeSamplingProfile();

  const profile = message.result.profile;
  const foundTheSample = profile.samples.some(sample =>
    sample.size >= 640 * 1024 && sample.stack.some(frame =>
      frame.includes('HTMLCanvasElement') || frame.includes('CanvasRenderingContext')));
  testRunner.log('Found sample: ' + foundTheSample);

  testRunner.completeTest();
})
