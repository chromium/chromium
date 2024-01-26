(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that device emulation with insets affects window.screenWidth, window.screenHeight, screen.width and screen.height.');

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(1200, 1000, 1, {left: 10, top: 20, right: 30, bottom: 40});

  var viewport = 'none';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
