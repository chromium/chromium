(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {session, dp} = await testRunner.startBlank('Tests that device emulation of device posture is propagated and powers Device Posture API.');

  await session.navigate('../resources/device-emulation.html');

  async function runAndAwaitPostureChange(action) {
    await session.evaluate(`
      window.postureChangedPromise = Promise.all(
        [window, ...Array.from(window.frames)].map(
          frame => new Promise(resolve => {
            frame.navigator.devicePosture.addEventListener('change', resolve, {once: true});
          })
        )
      );
    `);
    await action();
    await session.evaluateAsync(`
      window.postureChangedPromise.finally(() => {
        delete window.postureChangedPromise;
      })
    `);
  }

  testRunner.log(await session.evaluate(`dumpDevicePosture()`));

  await runAndAwaitPostureChange(() => dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.,
    devicePosture: {type: 'folded'}
  }));
  testRunner.log(await session.evaluate(`dumpDevicePosture()`));

  await runAndAwaitPostureChange(() =>
                                     dp.Emulation.clearDeviceMetricsOverride());
  testRunner.log(await session.evaluate(`dumpDevicePosture()`));

  testRunner.completeTest();
})
