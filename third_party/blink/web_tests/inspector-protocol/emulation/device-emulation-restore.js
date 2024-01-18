(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that disabling device emulation restores back to original values.');

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);

  var viewport = 'none';
  await session.navigate('../resources/device-emulation.html?' + viewport);
  var originalMetrics = await session.evaluate(`dumpMetrics(true)`);

  await deviceEmulator.emulate(1200, 1000, 1);
  viewport = 'w=320';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);
  testRunner.log(await session.evaluate(`dumpMetrics(true)`));

  await deviceEmulator.clear();
  viewport = 'none';
  await session.navigate('../resources/device-emulation.html?' + viewport);
  var metrics = await session.evaluate(`dumpMetrics(true)`);
  if (metrics != originalMetrics)
    testRunner.log('Original metrics not restored.\n==== Original ===\n' + originalMetrics + '\n==== Restored ====\n' + metrics);
  else
    testRunner.log('Original metrics restored correctly.');

  testRunner.completeTest();
})
