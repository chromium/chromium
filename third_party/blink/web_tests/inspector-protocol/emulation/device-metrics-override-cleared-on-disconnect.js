(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that the view size set by Emulation.setDeviceMetricsOverride is restored when the session disconnects without clearing the override.');

  async function viewportSize(activeSession) {
    return activeSession.evaluate(
        `window.innerWidth + 'x' + window.innerHeight`);
  }

  const initialSize = await viewportSize(session);

  await dp.Emulation.setDeviceMetricsOverride({
    width: 400,
    height: 300,
    deviceScaleFactor: 0,
    mobile: false,
  });
  testRunner.log('Size with override: ' + await viewportSize(session));

  // A second session observes the page while the first one goes away.
  const observer = await page.createSession();
  await observer.protocol.Page.enable();

  testRunner.log('Disconnecting session without clearing the override');
  await session.disconnect();

  let restoredSize = await viewportSize(observer);
  while (restoredSize !== initialSize) {
    await observer.protocol.Page.onceFrameResized();
    restoredSize = await viewportSize(observer);
  }
  testRunner.log('Size restored after disconnect: ' +
                 (restoredSize === initialSize));

  testRunner.completeTest();
})
