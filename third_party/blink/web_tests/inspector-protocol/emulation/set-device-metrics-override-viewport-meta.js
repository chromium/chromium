(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests Emulation.setDeviceMetricsOverride(viewportMeta) affects layout width.');

  const testPage = '../resources/device-emulation.html?w=320';

  async function getClientWidth() {
    await session.navigate(testPage);
    return await session.evaluate('document.documentElement.clientWidth');
  }

  // Initial state (desktop default: 800x600, viewport meta ignored)
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false
  });
  testRunner.log('Initial clientWidth (mobile: false, default): ' + await getClientWidth());

  // Test 1: mobile: false, viewportMeta: 'enable'
  testRunner.log('1. Mobile: false, viewportMeta: "enable"');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false,
    viewportMeta: 'enable'
  });
  testRunner.log('clientWidth: ' + await getClientWidth());

  // Test 2: mobile: false, viewportMeta: 'default'
  testRunner.log('2. Mobile: false, viewportMeta: "default"');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false,
    viewportMeta: 'default'
  });
  testRunner.log('clientWidth: ' + await getClientWidth());

  // Test 3: mobile: true, viewportMeta: 'default'
  testRunner.log('3. Mobile: true, viewportMeta: "default"');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true,
    viewportMeta: 'default'
  });
  testRunner.log('clientWidth: ' + await getClientWidth());

  // Test 4: mobile: true, viewportMeta: 'enable'
  testRunner.log('4. Mobile: true, viewportMeta: "enable"');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true,
    viewportMeta: 'enable'
  });
  testRunner.log('clientWidth: ' + await getClientWidth());

  // Test 5: Defaulting when omitted
  testRunner.log('5. Defaulting when viewportMeta is omitted');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false
  });
  testRunner.log('mobile: false -> clientWidth: ' + await getClientWidth());

  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true
  });
  testRunner.log('mobile: true -> clientWidth: ' + await getClientWidth());

  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.completeTest();
})
