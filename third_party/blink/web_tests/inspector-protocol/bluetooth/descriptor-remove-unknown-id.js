(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth removing an unknown descriptor id');
  const bp = testRunner.browserP();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();

  // Start the test.
  const result = await bp.BluetoothEmulation.removeDescriptor({
    descriptorId: 'unknown descriptor id'
  });
  testRunner.log(result);

  testRunner.completeTest();
});
