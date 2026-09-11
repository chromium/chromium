(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Testing that auto dark mode is preserved across Page.captureScreenshot');

  await session.evaluate(() => {
    document.body.innerHTML =
        '<div id="target" style="color-scheme: light; background-color: light-dark(rgb(255, 255, 255), rgb(0, 0, 0));">Test</div>';
  });

  async function logBackgroundColor() {
    const color = await session.evaluate(() => {
      return window.getComputedStyle(document.getElementById('target')).backgroundColor;
    });
    testRunner.log(`Background color: ${color}`);
  }

  await logBackgroundColor();

  testRunner.log('Emulation.setAutoDarkModeOverride({enabled: true})');
  await dp.Emulation.setAutoDarkModeOverride({enabled: true});
  await logBackgroundColor();

  testRunner.log('Page.captureScreenshot(captureBeyondViewport: true)');
  await dp.Page.captureScreenshot({format: 'png', captureBeyondViewport: true});
  await logBackgroundColor();

  testRunner.completeTest();
})
