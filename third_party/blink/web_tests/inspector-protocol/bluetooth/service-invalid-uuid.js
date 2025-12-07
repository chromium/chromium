(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth adding an invalid service UUID');
  const bp = testRunner.browserP();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();

  // Start the test.
  const result = await bp.BluetoothEmulation.addService(
      {address: helper.peripheralAddress(), serviceUuid: 'abc'});
  testRunner.log(result);

  testRunner.completeTest();
});
