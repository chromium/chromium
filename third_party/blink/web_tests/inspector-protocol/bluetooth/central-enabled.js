(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Bluetooth.simulateCentral sets Bluetooth availability');
  const bp = testRunner.browserP();

  await bp.BluetoothEmulation.enable({state: 'powered-on'});
  testRunner.log(await session.evaluateAsync(
    () => navigator.bluetooth.getAvailability()
  ));

  testRunner.completeTest();
});