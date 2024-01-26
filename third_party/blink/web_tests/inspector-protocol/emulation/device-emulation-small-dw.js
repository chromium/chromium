(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`
    Tests that device emulation affects media rules, viewport meta tag, body dimensions and window.screen.
    Emulating small device on a page with viewport "width=device-width" set should work without reload for page without scrollbar.
  `);

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(380, 420, 1);

  var viewport = 'w=dw';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);
  await session.evaluate(`document.body.classList.add('small')`);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
