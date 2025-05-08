(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth characteristic operation events handling');
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
  // Prepare characteristic operation handling.
  bp.BluetoothEmulation.onCharacteristicOperationReceived(
      ({params: {characteristicId, type, data, writeType}}) => {
        testRunner.log(`Characteristic operation received: characteristicId: ${
            characteristicId}, type: ${type}, data: ${data}, writeType: ${
            writeType}`);
        bp.BluetoothEmulation.simulateCharacteristicOperationResponse({
          characteristicId,
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
  // The following two descriptors are needed for testing start and stop
  // notification subscription.
  await bp.BluetoothEmulation.addDescriptor({
    characteristicId: measurementIntervalCharacteristicId,
    descriptorUuid:
        BluetoothHelper.CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
  });
  await bp.BluetoothEmulation.addDescriptor({
    characteristicId: measurementIntervalCharacteristicId,
    descriptorUuid:
        BluetoothHelper.CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID
  });

  const testCharacteristicOperations =
      async (serviceUuid, characteristicUuid) => {
    const devices = await navigator.bluetooth.getDevices();
    const server = await devices[0].gatt.connect();
    let readResult;
    try {
      const service = await server.getPrimaryService(serviceUuid);
      const characteristic =
          await service.getCharacteristic(characteristicUuid);
      readResult =
          btoa(await characteristic.readValue().then(value => value.buffer));
      await characteristic.writeValue(new Uint8Array([1]));
      await characteristic.writeValueWithResponse(new Uint8Array([1]));
      await characteristic.writeValueWithoutResponse(new Uint8Array([1]));
      await characteristic.startNotifications();
      await characteristic.stopNotifications();
    } catch (e) {
      return e.message;
    }
    return readResult;
  };

  // Start the test.
  testRunner.log(await session.evaluateAsync(
      testCharacteristicOperations, BluetoothHelper.HEART_RATE_SERVICE_UUID,
      BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID));

  testRunner.completeTest();
});
