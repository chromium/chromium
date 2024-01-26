(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that forced dark mode can be emulated.');
  // Note: the output log for this test can be viewed as HTML to
  // simplify review.

  await session.navigate('../resources/blank.html');

  async function logScreenshotData() {
    const response = await dp.Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width: 40,
        height: 40,
        scale: 1,
      },
    });
    const imageData = response.result.data;
    testRunner.log(`<img src="data:image/png;base64,${imageData}">`);
  }

  async function setAutoDarkModeOverride(enabled) {
    testRunner.log(`<p>Emulating auto dark mode: ${enabled}`);
    const response = await dp.Emulation.setAutoDarkModeOverride({
      enabled
    });
    if (response.error) {
      testRunner.log(JSON.stringify(response.error, null, 2));
      return;
    }
    await logScreenshotData();
  }

  // Baseline screenshot.
  testRunner.log(`<p>Baseline:`);
  await logScreenshotData();

  // Test setting dark mode.
  await setAutoDarkModeOverride(true);
  // Test setting dark mode twice.
  await setAutoDarkModeOverride(true);

  // Test disable force dark mode emulation.
  await setAutoDarkModeOverride(false);

  // Test reset force dark mode emulation.
  await setAutoDarkModeOverride(true);
  await setAutoDarkModeOverride(undefined);

  // Test that dark mode emulation survives navigation.
  await setAutoDarkModeOverride(true);
  testRunner.log(`<p>Navigating&mldr;`);
  await session.navigate('../resources/blank.html');
  await logScreenshotData();

  testRunner.completeTest();
});
