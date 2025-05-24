(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth adding and removing characteristic from a service');
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
  const {result: {serviceId: heartRateServiceId}} =
      await bp.BluetoothEmulation.addService({
        address: helper.peripheralAddress(),
        serviceUuid: BluetoothHelper.HEART_RATE_SERVICE_UUID,
      });

  const getCharacteristics = async (serviceUuid) => {
    const devices = await navigator.bluetooth.getDevices();
    const server = await devices[0].gatt.connect();
    const toPropertyStrings = (properties) => {
      let propertyStrings = [];
      if (properties.broadcast) {
        propertyStrings.push('broadcast');
      }
      if (properties.read) {
        propertyStrings.push('read');
      }
      if (properties.write_without_response) {
        propertyStrings.push('write_without_response');
      }
      if (properties.write) {
        propertyStrings.push('write');
      }
      if (properties.notify) {
        propertyStrings.push('notify');
      }
      if (properties.indicate) {
        propertyStrings.push('indicate');
      }
      if (properties.authenticated_signed_writes) {
        propertyStrings.push('authenticated_signed_writes');
      }
      if (properties.reliableWrite) {
        propertyStrings.push('reliableWrite');
      }
      if (properties.writableAuxiliaries) {
        propertyStrings.push('writableAuxiliaries');
      }
      return propertyStrings;
    };

    let characteristics = [];
    try {
      const service = await server.getPrimaryService(serviceUuid);
      characteristics = await service.getCharacteristics();
    } catch (e) {
      return e.message;
    }
    return characteristics.map(
        c => [c.uuid].concat(toPropertyStrings(c.properties)));
  };

  // Start the test.
  const {result: {characteristicId: measurementIntervalCharacteristicId}} =
      await bp.BluetoothEmulation.addCharacteristic({
        serviceId: heartRateServiceId,
        characteristicUuid:
            BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        properties: {
          read: true,
          write: false,
          notify: true,
        }
      });
  testRunner.log(`After adding measurement interval characteristic: ${
      await session.evaluateAsync(
          getCharacteristics, BluetoothHelper.HEART_RATE_SERVICE_UUID)}`);

  const {result: {characteristicId: dateTimeCharacteristicId}} =
      await bp.BluetoothEmulation.addCharacteristic({
        serviceId: heartRateServiceId,
        characteristicUuid: BluetoothHelper.DATE_TIME_CHARACTERISTIC_UUID,
        properties: {
          read: true,
        }
      });
  testRunner.log(`After adding date time characteristic: ${
      await session.evaluateAsync(
          getCharacteristics, BluetoothHelper.HEART_RATE_SERVICE_UUID)}`);

  await bp.BluetoothEmulation.removeCharacteristic({
    characteristicId: dateTimeCharacteristicId
  });
  testRunner.log(`After removing date time characteristic: ${
      await session.evaluateAsync(
          getCharacteristics, BluetoothHelper.HEART_RATE_SERVICE_UUID)}`);

  await bp.BluetoothEmulation.removeCharacteristic({
    characteristicId: measurementIntervalCharacteristicId
  });
  testRunner.log(`After removing measurement interval characteristic: ${
      await session.evaluateAsync(
          getCharacteristics, BluetoothHelper.HEART_RATE_SERVICE_UUID)}`);

  testRunner.completeTest();
});
