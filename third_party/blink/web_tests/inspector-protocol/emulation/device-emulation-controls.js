(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that form controls are rendered with correct theme. See crbug.com/591315.');

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(800, 600, 1);

  var viewport = 'none';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.log(await session.evaluate(`
    var input = document.createElement('input');
    input.type = 'radio';
    document.body.appendChild(input);
    'measured radio: ' + input.offsetWidth + 'x' + input.offsetHeight
  `));
  testRunner.log(await session.evaluate(`
    var input = document.createElement('input');
    input.type = 'checkbox';
    document.body.appendChild(input);
    'measured checkbox: ' + input.offsetWidth + 'x' + input.offsetHeight
  `));
  testRunner.completeTest();
})
