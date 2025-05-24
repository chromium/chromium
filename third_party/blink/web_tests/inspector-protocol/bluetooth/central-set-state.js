(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests updating the state of the simulateCentral.');
  const bp = testRunner.browserP();

  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: true});
  testRunner.log(
      await session.evaluateAsync(() => navigator.bluetooth.getAvailability()),
      'Bluetooth availability when the adapter is powered-on');
  await bp.BluetoothEmulation.setSimulatedCentralState(
      {state: 'powered-off', leSupported: true});
  testRunner.log(await session.evaluateAsync(
      () => navigator.bluetooth.getAvailability(),
      'Bluetooth availability when the adapter is powered-off'));
  await bp.BluetoothEmulation.setSimulatedCentralState(
      {state: 'absent', leSupported: true});
  testRunner.log(await session.evaluateAsync(
      () => navigator.bluetooth.getAvailability(),
      'Bluetooth availability when the adapter is absent'));

  testRunner.completeTest();
});
