(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Bluetooth is unavailable when simulateCentral is set to absent');
  const bp = testRunner.browserP();

  await bp.BluetoothEmulation.enable({state: 'absent'});
  testRunner.log(await session.evaluateAsync(
    () => navigator.bluetooth.getAvailability()), undefined, 'Bluetooth availability');

  testRunner.completeTest();
});