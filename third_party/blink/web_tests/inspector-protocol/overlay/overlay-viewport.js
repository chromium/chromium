(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <div style="width: 400px; height: 400px;">Test page</div>
  `, 'Verifies that Overlay.setShowViewportSizeOnResize works.');

  await dp.DOM.enable();
  await dp.Overlay.enable();
  // Page domain is required by the viewport size overlay.
  await dp.Page.enable();

  await dp.Overlay.setInspectMode({
    mode: 'searchForNode',
    highlightConfig: {},
  });

  async function waitForAnimationFrame() {
    await session.evaluateAsync(() => {
      return new Promise(resolve => requestAnimationFrame(resolve));
    });
  }

  async function takeViewportResetCommands(count) {
    return await session.evaluate((count) => {
      return internals.evaluateInInspectorOverlay(`(function () {
        const commands = window.commands;
        window.commands = [];
        return JSON.stringify(commands.slice(0, ${count}), null, 2);
      })()`);
    }, count);
  }

  await session.evaluate(() => {
    return internals.evaluateInInspectorOverlay(`(function () {
      window.commands = [];
      window.dispatch = ([name, data]) => {
        window.commands.push({name, data});
      };
    })()`);
  });

  async function runTest(label, metrics, commandCount) {
    // Consume another frame and delete any commands that might come from
    // a previous test. On Mac and Windows, an additional frame might be
    // rendered during the call to setDeviceMetricsOverride resulting in overlay
    // commands using the previous emulation params being recorded.
    await waitForAnimationFrame();
    await takeViewportResetCommands(0);
    const result = await dp.Emulation.setDeviceMetricsOverride(metrics);
    await waitForAnimationFrame();
    testRunner.log('Response to setDeviceMetricsOverride:');
    testRunner.log(result);
    testRunner.log(label);
    testRunner.log(await takeViewportResetCommands(commandCount));
  }

  await runTest('Initial device metrics:', {
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false,
    dontSetVisibleSize: false,
  }, 1);

  await dp.Overlay.setShowViewportSizeOnResize({
    show: true,
  });

  await runTest('Device metrics with changed viewport:', {
    width: 500,
    height: 500,
    deviceScaleFactor: 1,
    mobile: false,
  }, 3);

  await runTest('Device metrics with scrollbar:', {
    width: 300,
    height: 300,
    deviceScaleFactor: 1,
    mobile: false,
  }, 3);

  testRunner.completeTest();
});

