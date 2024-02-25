(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that vision deficiencies can be emulated.');
  // Note: the output log for this test can be viewed as HTML to
  // simplify review.

  await session.navigate('../resources/vision-deficiency.html');

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

  async function setEmulatedVisionDeficiency(id) {
    testRunner.log(`<p>Emulating ${JSON.stringify(id)}:`);
    const response = await dp.Emulation.setEmulatedVisionDeficiency({
      type: id,
    });
    if (response.error) {
      testRunner.log(JSON.stringify(response.error, null, 2));
      return;
    }
    await logScreenshotData();
  }

  await setEmulatedVisionDeficiency('none');
  await setEmulatedVisionDeficiency('achromatopsia');
  await setEmulatedVisionDeficiency('blurredVision');
  await setEmulatedVisionDeficiency('reducedContrast');
  await setEmulatedVisionDeficiency('none');
  await setEmulatedVisionDeficiency('deuteranopia');
  await setEmulatedVisionDeficiency('none');
  await setEmulatedVisionDeficiency('protanopia');
  await setEmulatedVisionDeficiency('tritanopia');
  // Test setting the already-active vision deficiency.
  await setEmulatedVisionDeficiency('tritanopia');
  // Test setting unknown vision deficiencies.
  await setEmulatedVisionDeficiency('some-invalid-deficiency');
  await setEmulatedVisionDeficiency('');
  // Test setting no-longer-supported vision deficiencies.
  await setEmulatedVisionDeficiency('achromatomaly');
  await setEmulatedVisionDeficiency('deuteranomaly');
  await setEmulatedVisionDeficiency('protanomaly');
  await setEmulatedVisionDeficiency('tritanomaly');

  testRunner.log(`<p>Navigating&mldr;`);
  await session.navigate('../resources/vision-deficiency.html');
  await logScreenshotData();
  await setEmulatedVisionDeficiency('achromatopsia');

  testRunner.completeTest();
});
