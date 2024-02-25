(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests that desktop device emulation affects media rules, viewport meta tag, body dimensions and window.screen.');

  await session.protocol.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false,
    fitWindow: false,
    scale: 1,
    screenWidth: 1200,
    screenHeight: 1000,
    positionX: 110,
    positionY: 120
  });

  var viewport = 'w=dw';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
