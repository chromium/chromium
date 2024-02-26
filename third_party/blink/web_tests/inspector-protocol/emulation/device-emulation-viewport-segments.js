(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests that device emulation of viewport segments is propagated and powers VisualViewport segments API.');

  let deviceMetrics = {
    width: 800,
    height: 600,
    deviceScaleFactor: 2,
    mobile: false,
    fitWindow: false,
    scale: 0.5,
    screenWidth: 1200,
    screenHeight: 1000,
    positionX: 110,
    positionY: 120,
  };

  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);

  await session.navigate('../resources/device-emulation.html');

  testRunner.log("No segments:");
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.log("Side-by-side segments");
  deviceMetrics.displayFeature = {
      orientation: "vertical",
      offset: 390,
      maskLength: 20
  };
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.log("Unspecified display feature with scale");
  delete deviceMetrics.displayFeature;
  // Setting width/height to 0 indicates that the widget should have an
  // emulated size based on the size of the widget in DIP, but takes the
  // scale into account. Given a scale of 0.5, we expect the window
  // segment (and window itself) dimensions to double.
  deviceMetrics.width = 0;
  deviceMetrics.height = 0;
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.log("Stacked segments");
  deviceMetrics.width = 800;
  deviceMetrics.height = 600;
  deviceMetrics.displayFeature = {
      orientation: "horizontal",
      offset: 290,
      maskLength: 20
  };
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.log("Emulation disabled");
  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.completeTest();
})
