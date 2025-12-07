(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth adding an invalid descriptor UUID');
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
  const {result: {characteristicId: measurementIntervalCharacteristicId}} =
      await bp.BluetoothEmulation.addCharacteristic({
        serviceId: heartRateServiceId,
        characteristicUuid:
            BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        properties: {read: true}
      });

  // Start the test.
  const result = await bp.BluetoothEmulation.addDescriptor({
    characteristicId: measurementIntervalCharacteristicId,
    descriptorUuid: 'abc'
  });
  testRunner.log(result);

  testRunner.completeTest();
});
