(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests that Bluetooth is unavailable when low-energy is not supported.');
  const bp = testRunner.browserP();

  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: false});
  testRunner.log(
      await session.evaluateAsync(() => navigator.bluetooth.getAvailability()));

  testRunner.completeTest();
});
