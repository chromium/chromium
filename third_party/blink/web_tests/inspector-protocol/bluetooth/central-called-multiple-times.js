(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that repeated Bluetooth.simulateCentral calls are avoided');
  const bp = testRunner.browserP();

  const first = await bp.BluetoothEmulation.enable({state: 'powered-on'});
  testRunner.log(first);
  const second = await bp.BluetoothEmulation.enable({state: 'powered-on'});
  testRunner.log(second);

  testRunner.completeTest();
});