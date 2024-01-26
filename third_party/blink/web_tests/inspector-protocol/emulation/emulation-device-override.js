(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests emulation of the user agent.');

  // vertical: ok
  var response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'vertical', offset: 30, maskLength: 20}
  });

  testRunner.log(response);

  // invalid width and height
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'vertical', offset: 30, maskLength: 20}
  });

  testRunner.log(response);

  // invalid display feature parameters.
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'vertical', offset: -30, maskLength: 20}
  });

  testRunner.log(response);

  // invalid display feature orientation type string
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'vertical1', offset: -30, maskLength: 20}
  });

  testRunner.log(response);

  // vertical: mask exceeds boundary
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'vertical', offset: 30, maskLength: 120}
  });

  testRunner.log(response);

  // horizontal: ok
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'horizontal', offset: 30, maskLength: 120}
  });

  testRunner.log(response);

  // horizontal: mask exceeds boundary
  response = await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 200,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.2,
    screenWidth: 100,
    screenHeight: 200,
    positionX: 0,
    positionY: 0,
    displayFeature: { orientation: 'horizontal', offset: 130, maskLength: 120}
  });

  testRunner.log(response);

  testRunner.completeTest();
});
