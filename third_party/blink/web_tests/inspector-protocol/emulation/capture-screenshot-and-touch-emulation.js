(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Testing that touch emulation is preserved across Page.captureScreenshot');

  async function logTouchState() {
    const state = {
      enabled: await session.evaluate(
          () => window.matchMedia('(pointer: coarse)').matches),
      maxTouchPoints: await session.evaluate(() => navigator.maxTouchPoints),
    };
    testRunner.log(`Touch state: ${JSON.stringify(state)}`);
  }

  testRunner.log('Emulation.setTouchEmulationEnabled');
  await dp.Emulation.setTouchEmulationEnabled({
    enabled: true,
    maxTouchPoints: 7,
  });
  await logTouchState();

  testRunner.log('Page.captureScreenshot(captureBeyondViewport: true)');
  await dp.Page.captureScreenshot({format: 'png', captureBeyondViewport: true});
  await logTouchState();

  testRunner.completeTest();
})
