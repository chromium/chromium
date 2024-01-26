(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`
    Tests that turning on device emulation with a non-1 device pixel ratio sets the
    appropriate initial scale. Page scale is reflected by the innerWidth and
    innerHeight properties. Since the layout width is set to 980 (in the viewport
    meta tag), and the emulated viewport width is 490 px, the initial scale should
    be 490/980 = 0.5. i.e. innerWidth=980 innerHeight=640.`);

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);
  // 980px viewport loaded into a 490x320 screen should load at 0.5 scale.
  await deviceEmulator.emulate(490, 320, 3);

  var viewport = 'w=980';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
