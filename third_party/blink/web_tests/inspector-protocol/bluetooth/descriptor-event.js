(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth descriptor operation events handling');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  await helper.requestDevice({
    acceptAllDevices: true,
    optionalServices: [BluetoothHelper.HEART_RATE_SERVICE_UUID]
  });
  await helper.setupGattOperationHandler();
  // Prepare descriptor operation handling.
  bp.BluetoothEmulation.onDescriptorOperationReceived(
      ({params: {descriptorId, type, data}}) => {
        testRunner.log(`Descriptor operation received: descriptorId: ${
            descriptorId}, type: ${type}, data: ${data}`);
        bp.BluetoothEmulation.simulateDescriptorOperationResponse({
          descriptorId,
          type,
          code: BluetoothHelper.HCI_SUCCESS,
          ...(type === 'read' && {data: 'W29iamVjdCBBcnJheUJ1ZmZlcl0='}),
        });
      });

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
  await bp.BluetoothEmulation.addDescriptor({
    characteristicId: measurementIntervalCharacteristicId,
    descriptorUuid:
        BluetoothHelper.CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
  });

  const testDescriptorOperations =
      async (serviceUuid, characteristicUuid, descriptorUuid) => {
    const devices = await navigator.bluetooth.getDevices();
    const server = await devices[0].gatt.connect();
    let readResult;
    try {
      const service = await server.getPrimaryService(serviceUuid);
      const characteristic =
          await service.getCharacteristic(characteristicUuid);
      const descriptor = await characteristic.getDescriptor(descriptorUuid);
      readResult =
          btoa(await descriptor.readValue().then(value => value.buffer));
      await descriptor.writeValue(new Uint8Array([1]));
    } catch (e) {
      return e.message;
    }
    return readResult;
  };

  // Start the test.
  testRunner.log(await session.evaluateAsync(
      testDescriptorOperations, BluetoothHelper.HEART_RATE_SERVICE_UUID,
      BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
      BluetoothHelper.CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID));

  testRunner.completeTest();
});
