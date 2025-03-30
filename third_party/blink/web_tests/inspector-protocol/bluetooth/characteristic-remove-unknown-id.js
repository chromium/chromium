(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth removing an unknown characteristic id');
  const bp = testRunner.browserP();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  const {result: {serviceId: heartRateServiceId}} =
      await bp.BluetoothEmulation.addService({
        address: helper.peripheralAddress(),
        serviceUuid: BluetoothHelper.HEART_RATE_SERVICE_UUID,
      });

  // Start the test.
  const result = await bp.BluetoothEmulation.removeCharacteristic({
    address: helper.peripheralAddress(),
    serviceId: heartRateServiceId,
    characteristicId: 'unknown characteristic id'
  });
  testRunner.log(result);

  testRunner.completeTest();
});
