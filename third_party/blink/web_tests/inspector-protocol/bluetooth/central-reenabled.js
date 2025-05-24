(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests that Bluetooth is available when simulateCentral is re-enabled.');
  const bp = testRunner.browserP();

  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: true});
  testRunner.log(
      await session.evaluateAsync(() => navigator.bluetooth.getAvailability()),
      'Bluetooth availability');
  await bp.BluetoothEmulation.disable();
  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: true});
  testRunner.log(
      await session.evaluateAsync(() => navigator.bluetooth.getAvailability()),
      'Bluetooth availability');

  testRunner.completeTest();
});
