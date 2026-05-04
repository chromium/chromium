(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {session, dp} = await testRunner.startBlank('Tests that text-size-adjust is applied when mobile emulation is enabled.');

  await session.evaluate(() => {
    document.body.innerHTML = '<div id="target" style="font-size: 10px; text-size-adjust: 200%;">Test</div>';
  });

  async function logFontSize() {
    testRunner.log(await session.evaluate(() => {
      const target = document.getElementById('target');
      return window.getComputedStyle(target).fontSize;
    }));
  }

  testRunner.log('Before emulation:');
  await logFontSize();

  testRunner.log('Enabling mobile emulation:');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true,
    scale: 1
  });
  await logFontSize();

  testRunner.log('Clearing emulation:');
  await dp.Emulation.clearDeviceMetricsOverride();
  await logFontSize();

  testRunner.completeTest();
})
