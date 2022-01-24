(async function(testRunner) {
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

  async function getViewportResetCommands(count) {
    await waitForAnimationFrame();
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

  await dp.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: false,
    dontSetVisibleSize: false,
  });

  testRunner.log('Initial device metrics:');
  testRunner.log(await getViewportResetCommands(1));

  await dp.Overlay.setShowViewportSizeOnResize({
    show: true,
  });

  await dp.Emulation.setDeviceMetricsOverride({
    width: 500,
    height: 500,
    deviceScaleFactor: 1,
    mobile: false,
  });

  testRunner.log('Device metrics with changed viewport:');
  testRunner.log(await getViewportResetCommands(3));

  await dp.Emulation.setDeviceMetricsOverride({
    width: 300,
    height: 300,
    deviceScaleFactor: 1,
    mobile: false,
  });
  testRunner.log('Device metrics with scrollbar:');
  testRunner.log(await getViewportResetCommands(3));

  testRunner.completeTest();
});

