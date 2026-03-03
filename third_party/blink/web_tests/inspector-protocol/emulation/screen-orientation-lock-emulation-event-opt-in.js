(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Emulation.screenOrientationLockChanged event emission and opt-in.');

  function waitForEventOrTimeout(eventPromise, timeoutMs = 1000) {
    return Promise.race([
      eventPromise,
      new Promise(resolve => setTimeout(() => resolve(null), timeoutMs)),
    ]);
  }

  async function setDeviceMetrics(screenOrientationLockEmulation) {
    await dp.Emulation.setDeviceMetricsOverride({
      width: 400,
      height: 800,
      deviceScaleFactor: 1,
      mobile: true,
      screenOrientationLockEmulation,
    });
  }

  async function simulateLock() {
    await session.evaluate(() => {
      testRunner.simulateScreenOrientationLockChanged(true, 'landscape-primary');
    });
  }

  async function simulateUnlock() {
    await session.evaluate(() => {
      testRunner.simulateScreenOrientationLockChanged(false, 'landscape-primary');
    });
  }

  testRunner.log('Without opt-in:');
  await setDeviceMetrics(false);

  const eventsWithoutOptIn = [];
  const noOptInHandler = event => eventsWithoutOptIn.push(event.params);
  dp.Emulation.onScreenOrientationLockChanged(noOptInHandler);

  await simulateLock();
  await simulateUnlock();
  await new Promise(resolve => setTimeout(resolve, 100));

  dp.Emulation.offScreenOrientationLockChanged(noOptInHandler);
  testRunner.log(`Event count: ${eventsWithoutOptIn.length}`);

  testRunner.log('With opt-in:');
  await setDeviceMetrics(true);

  const lockEventPromise = dp.Emulation.onceScreenOrientationLockChanged(
      event => event.params.locked);
  await simulateLock();
  const lockEvent = await waitForEventOrTimeout(lockEventPromise);
  if (!lockEvent) {
    testRunner.log('Lock event: <none>');
  } else {
    testRunner.log(lockEvent.params, 'Lock event:');
  }

  const unlockEventPromise = dp.Emulation.onceScreenOrientationLockChanged(
      event => !event.params.locked);
  await simulateUnlock();
  const unlockEvent = await waitForEventOrTimeout(unlockEventPromise);
  if (!unlockEvent) {
    testRunner.log('Unlock event: <none>');
  } else {
    testRunner.log(unlockEvent.params, 'Unlock event:');
  }

  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.completeTest();
})
