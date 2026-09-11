(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Testing that mobile text-size-adjust is preserved across Page.captureScreenshot');

  await session.evaluate(() => {
    document.body.innerHTML =
        '<div id="target" style="font-size: 10px; text-size-adjust: 200%;">Test</div>';
  });

  async function logFontSize() {
    const size = await session.evaluate(() => {
      return window.getComputedStyle(document.getElementById('target')).fontSize;
    });
    testRunner.log(`Font size: ${size}`);
  }

  await logFontSize();

  testRunner.log('Emulation.setDeviceMetricsOverride(mobile: true)');
  // Mobile emulation enables text-size-adjust.
  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true,
    scale: 1,
  });
  await logFontSize();

  testRunner.log('Page.captureScreenshot(captureBeyondViewport: true)');
  await dp.Page.captureScreenshot({format: 'png', captureBeyondViewport: true});
  await logFontSize();

  testRunner.completeTest();
})
