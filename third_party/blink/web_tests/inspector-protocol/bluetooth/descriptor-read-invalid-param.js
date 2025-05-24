(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth descriptor read response simulation invalid paramter');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  // Setup UUIDs.
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
        properties: {
          read: true,
          write: true,
          indicate: true,
        }
      });
  const {result: {descriptorId: userDescriptionDescriptorId}} =
      await bp.BluetoothEmulation.addDescriptor({
        characteristicId: measurementIntervalCharacteristicId,
        descriptorUuid:
            BluetoothHelper.CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
      });

  // Start test.
  const resultSuccessCodeWithoutData =
      await bp.BluetoothEmulation.simulateDescriptorOperationResponse({
        descriptorId: userDescriptionDescriptorId,
        type: 'read',
        code: BluetoothHelper.HCI_SUCCESS
      });
  testRunner.log(resultSuccessCodeWithoutData);

  const resultTimeoutCodeWithData =
      await bp.BluetoothEmulation.simulateDescriptorOperationResponse({
        descriptorId: userDescriptionDescriptorId,
        type: 'read',
        code: BluetoothHelper.HCI_CONNECTION_TIMEOUT,
        data: 'W29iamVjdCBBcnJheUJ1ZmZlcl0='
      });
  testRunner.log(resultTimeoutCodeWithData);

  testRunner.completeTest();
});
