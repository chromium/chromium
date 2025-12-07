(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that device emulation without viewport override works correctly.');

  const DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  const deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(0, 0, 1);

  const viewport = 'w=dw';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);
  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
